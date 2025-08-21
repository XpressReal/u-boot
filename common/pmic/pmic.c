#include <stdlib.h>
#include <pmic.h>

DEFINE_PMIC_DRIVER_WEAK(g2227);
DEFINE_PMIC_DRIVER_WEAK(g2237);
DEFINE_PMIC_DRIVER_WEAK(sy8827e);
DEFINE_PMIC_DRIVER_WEAK(sy8824c);
DEFINE_PMIC_DRIVER_WEAK(apw8889);
DEFINE_PMIC_DRIVER_WEAK(apw8886);
DEFINE_PMIC_DRIVER_WEAK(pwm_dt);

static void pmic_add_drivers(void)
{
	ADD_PMIC_DRIVER(g2227);
	ADD_PMIC_DRIVER(g2237);
	ADD_PMIC_DRIVER(sy8827e);
	ADD_PMIC_DRIVER(sy8824c);
	ADD_PMIC_DRIVER(apw8889);
	ADD_PMIC_DRIVER(apw8886);
	ADD_PMIC_DRIVER(pwm_dt);
}
__WEAK struct pmic_device *pwm_dt_create_device(struct pmic_driver *drv)
{
	return NULL;
}

static void pmic_destroy_device(struct pmic_device *dev)
{
	if (!dev)
		return;
	if (dev->private)
		free(dev->private);
	free(dev);
}

int pmic_write_reg(struct pmic_device *dev, unsigned char reg, unsigned char val)
{

	return dev->ops->write(dev, reg, val);
}

int pmic_read_reg(struct pmic_device *dev, unsigned char reg, unsigned char *val)
{
	return dev->ops->read(dev, reg, val);
}

int pmic_entry_read(struct pmic_device *dev, const struct pmic_entry *ent, unsigned char *val)
{
	int ret;
	unsigned char tmp;

	ret = pmic_read_reg(dev, ent->reg, &tmp);
	if (ret)
		return ret;

	tmp &= ent->mask;
	tmp >>= ffs(ent->mask) - 1;

	*val = tmp;
	return 0;
}

int pmic_entry_write(struct pmic_device *dev, const struct pmic_entry *ent, unsigned char val)
{
	int ret;
	unsigned char tmp;
	unsigned int lsb = ffs(ent->mask) - 1;

	ret = pmic_read_reg(dev, ent->reg, &tmp);
	if (ret)
		return ret;

	val <<= lsb;
	val &= ent->mask;

	tmp &= ~ent->mask;
	tmp |= val;

	ret = pmic_write_reg(dev, ent->reg, tmp);

	return ret;
}

int pmic_entry_val_to_str(const struct pmic_entry *ent, unsigned char val, char *str, int size)
{
	int ret = 0;

	if (val > ent->size)
		return PMIC_ERR;

	switch (ent->type) {
	case PMIC_ENTRY_VALMAP_U32_ARRAY:
		ret = snprintf(str, size, "%u", ent->u32_array.array[val]);
		break;
	case PMIC_ENTRY_VALMAP_U32_STEP:
		ret = snprintf(str, size, "%u", ent->u32_step.start + ent->u32_step.step * val);
		break;
	case PMIC_ENTRY_VALMAP_STRING_ARRAY:
		ret = PMIC_ERR;
		if (ent->string_array.array[val])
			ret = snprintf(str, size, "%s", ent->string_array.array[val]);
		break;
	}
	if (ret < 0 || ret >= size)
		return PMIC_ERR;
	return 0;
}

int pmic_entry_str_to_val(const struct pmic_entry *ent, const char *str, unsigned char *val)
{
	unsigned int strv;
	int i;

	switch (ent->type) {
	case PMIC_ENTRY_VALMAP_U32_ARRAY:
		strv = atoi(str);
		if (strv == 0)
			return PMIC_ERR;
		for (i = 0; i < ent->size; i++)
			if (strv == ent->u32_array.array[i])
				break;
		if (i >= ent->size)
			return PMIC_ERR;
		*val = i;
		break;
	case PMIC_ENTRY_VALMAP_U32_STEP:
		strv = atoi(str);
		if (strv == 0)
			return PMIC_ERR;
		strv -= ent->u32_step.start;
		if ((strv % ent->u32_step.step) != 0)
			return PMIC_ERR;
		*val = strv / ent->u32_step.step;
		break;

	case PMIC_ENTRY_VALMAP_STRING_ARRAY:
		for (i = 0; i < ent->size; i++)
			if (ent->string_array.array[i] && !strcmp(ent->string_array.array[i], str))
				break;
		if (i >= ent->size)
			return PMIC_ERR;
		*val = i;
		break;
	}
	return 0;
}

#define PMIC_DRIVER_MAX 10
static struct pmic_driver *pmic_drv[PMIC_DRIVER_MAX];
static int pmic_drv_num = 0;

int pmic_driver_register(struct pmic_driver *drv)
{
	if (pmic_drv_num >= PMIC_DRIVER_MAX)
		return PMIC_ERR;
	drv->dev_cnt = 0;
	pmic_drv[pmic_drv_num++] = drv;
	return 0;
}

int pmic_device_probe(const char *name, int arg0, int arg1)
{
	int i;
	struct pmic_driver *drv = NULL;
	struct pmic_device *dev = NULL;
	int ret;

	if (pmic_drv_num == 0)
		pmic_add_drivers();


	for (i = 0; i < pmic_drv_num; i++)
		if (!strcmp(name, pmic_drv[i]->name)) {
			drv = pmic_drv[i];
			break;
		}

	if (!drv) {
		printf("%s: no match driver for device '%s'\n", __func__, name);
		return PMIC_ERR;
	}

	if (!drv->probe) {
		printf("%s: no probe function for driver '%s'\n", __func__, name);
		return PMIC_ERR;
	}


	switch(drv->type) {
	case PMIC_DEVICE_TYPE_I2C:
		dev = i2c_create_device(drv, arg0, arg1);
		break;
	case PMIC_DEVICE_TYPE_PWM:
		dev = pwm_dt_create_device(drv);
		break;
	}

	if (!dev) {
		printf("%s: no device created for '%s'\n", __func__, name);
		return PMIC_ERR;
	}

	dev->ents = drv->ents;
	dev->n_ents = drv->n_ents;
	ret = drv->probe(dev);
	if (ret)
		pmic_destroy_device(dev);
	else {
		pmic_cmd_device_add(dev);
		drv->dev_cnt ++;
	}

	return ret;
}

#define PMIC_CMD_DEVICE_MAX 20
static struct pmic_device *cmd_dev[PMIC_CMD_DEVICE_MAX];
static int cmd_dev_num = 0;

void pmic_cmd_device_add(struct pmic_device *dev)
{
	if (cmd_dev_num >= PMIC_CMD_DEVICE_MAX) {
		printf("cmd device list is full\n");
		return;
	}

	cmd_dev[cmd_dev_num++] = dev;
}

struct pmic_device *pmic_cmd_device_find(const char *name)
{
	int i;

	for (i = 0; i < cmd_dev_num; i++)
		if (!strcmp(cmd_dev[i]->name, name))
			return cmd_dev[i];
	return NULL;
}

struct pmic_device *pmic_cmd_device_find_by_index(int i)
{
	if (i > cmd_dev_num || i >= PMIC_CMD_DEVICE_MAX)
		return NULL;
	return cmd_dev[i];
}
