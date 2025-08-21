#include <stdlib.h>
#include <pmic.h>
#include <linux/libfdt.h>
#include <rtk/pwm.h>

struct pwm_dt_config {
	int id;
	int period;
	int enable;
	unsigned int *duty_cycle_table;
	unsigned int *volt_table;
	unsigned int volt_table_size;
	unsigned int default_duty_cycle;
	int loc;

	int duty_cycle;
	struct pmic_entry ents[2];
};

#define ADDR_BUF_LEN     32
#define PWM_VIRT_REG_ENABLE     0
#define PWM_VIRT_REG_VOLT       1
static const struct type_ops pwm_dt_ops;

static const char *enable[] = { "disable", "enable" };
static struct pwm_dt_config pwm_dt_config_template = {
	.ents[PWM_VIRT_REG_ENABLE] = {
		.name = "enable",
		.reg = PWM_VIRT_REG_ENABLE,
		.mask = 0xff,
		.type = PMIC_ENTRY_VALMAP_STRING_ARRAY,
		.size = ARRAY_SIZE(enable),
		.string_array = {
			.array = enable,
		},
	},
	.ents[PWM_VIRT_REG_VOLT] = {
		.name = "volt",
		.reg = PWM_VIRT_REG_VOLT,
		.mask = 0xff,
		.type = PMIC_ENTRY_VALMAP_U32_ARRAY,
	},
};

struct pmic_device *pwm_dt_create_device(struct pmic_driver *drv)
{
	struct pmic_device *dev;

	dev = malloc(sizeof(*dev));
	if (!dev)
		return NULL;

	memset(dev, 0, sizeof(*dev));

	if (drv->dev_cnt == 0)
		snprintf(dev->name, sizeof(dev->name), "%s", drv->name);
	else
		snprintf(dev->name, sizeof(dev->name), "%s%d", drv->name, drv->dev_cnt);
	snprintf(dev->full_name, sizeof(dev->full_name), "%s%d-pwmx-x", drv->name, drv->dev_cnt);
	dev->ops = &pwm_dt_ops;
	return dev;
}

static int pwm_dt_write(struct pmic_device *dev, unsigned char reg, unsigned char val)
{
	struct pwm_dt_config *c = dev->private;
	int f = 1000000000 / c->period;

	switch (reg) {
	case PWM_VIRT_REG_ENABLE:
		c->enable = val;

		__pwm_set_freq(c->id, f, false);

		if (c->enable) {
			pwm_enable(c->id, c->enable);
			pwm_set_duty_rate(c->id, c->default_duty_cycle);
			_rtk_pwm_pinmux(c->id, c->loc, c->enable);
		} else {
			_rtk_pwm_pinmux(c->id, c->loc, c->enable);
			pwm_set_duty_rate(c->id, c->default_duty_cycle);
			pwm_enable(c->id, c->enable);
		}
		break;
	case PWM_VIRT_REG_VOLT:

		c->duty_cycle = val;
		pwm_set_duty_rate(c->id, c->duty_cycle_table[c->duty_cycle]);
		break;
	}
	return 0;
}

static int pwm_dt_read(struct pmic_device *dev, unsigned char reg, unsigned char *val)
{
	struct pwm_dt_config *c = dev->private;

	switch (reg) {
	case PWM_VIRT_REG_ENABLE:
		*val = c->enable;
		break;
	case PWM_VIRT_REG_VOLT:
		*val = c->duty_cycle;
		break;
	}
	return 0;
}

static const struct type_ops pwm_dt_ops = {
	.read = pwm_dt_read,
	.write = pwm_dt_write,
};

static unsigned long pwm_dt_get_fdt_addr(void)
{
	unsigned long fdt_addr = 0;

	fdt_addr = env_get_ulong("fdt_loadaddr", 16, 0x02100000);
	return fdt_addr;
}

static int parse_pin_node(void *fdt, int offset, struct pwm_dt_config *c)
{
	const char *name;
	int pwmid, pwmloc;

	/* simply get from name */
	name = fdt_get_name(fdt, offset, NULL);
	pwmid = name[3] - '0';
	pwmloc =  name[5] - '0';

	printf("pmic-pwm: %s: pwmid=%d, loc=%d\n", __func__, pwmid, pwmloc);
	if (c->id != pwmid) {
		printf("pmic-pwm: %s: pwmid mismatch\n", __func__);
		return PMIC_ERR;
	}

	c->loc = pwmloc;
	return 0;
}

static int parse_pwm_regulator_node_pinctrl_0(void *fdt, int offset, struct pwm_dt_config *c)
{
	const unsigned int *p;
	unsigned int phandle;
	int pin_offset;

	p = fdt_getprop(fdt, offset, "pinctrl-0", NULL);
	if (!p) {
		printf("pmic-pwm: %s: no pinctrl-0\n", __func__);
		return PMIC_ERR;
	}

	phandle = fdt32_to_cpu(*p);
	pin_offset = fdt_node_offset_by_phandle(fdt, phandle);
	if (pin_offset < 0) {
		printf("pmic-pwm: %s: fdt_node_offset_by_phandle() failed\n", __func__);
		return PMIC_ERR;
	}

	return parse_pin_node(fdt, pin_offset, c);

}

static int parse_pwm_regulator_node_pwms(void *fdt, int offset, struct pwm_dt_config *c)
{
	const unsigned int *p;
	int len;

	p = fdt_getprop(fdt, offset, "pwms", &len);
	if (len < 0) {
		printf("pmic-pwm: %s: no pwms\n", __func__);
		return PMIC_ERR;
	}

	c->id = fdt32_to_cpu(*(p + 1));
	c->period = fdt32_to_cpu(*(p + 2));
	printf("pmic-pwm: %s: pwmid=%d: period=%d\n", __func__, c->id, c->period);

	return 0;
}

static int parse_pwm_regulator_node_voltage_table(void *fdt, int offset, struct pwm_dt_config *c)
{
	const unsigned int *p;
	unsigned int *v;
	unsigned int *d;
	unsigned int best_v = 0;
	int len;
	int i;
	int size;

	p = fdt_getprop(fdt, offset, "voltage-table", &len);
	if (len < 0) {
		printf("pmic-pwm: no voltage-table in /pwm-regulator\n");
		return PMIC_ERR;
	}

	size = len / 8;
	v = malloc(sizeof(*v) * size * 2);
	if (!v)
		return PMIC_ERR;

	d = v + size;
	for (i = 0; i < size; i++) {
		v[i] = fdt32_to_cpu(*(p + i * 2));
		d[i] = fdt32_to_cpu(*(p + i * 2 + 1));

		if (best_v == 0 || (v[i] >= 1000000 && best_v > v[i])) {
			best_v = v[i];
			c->default_duty_cycle = d[i];
		}
	}

	c->duty_cycle_table = d;
	c->volt_table = v;
	c->volt_table_size = size;

	return 0;
}

static int parse_pwm_regulator_node(void *fdt, int offset, struct pwm_dt_config *c)
{
	int ret;

	ret = parse_pwm_regulator_node_pwms(fdt, offset, c);
	if (ret)
		return ret;

	ret = parse_pwm_regulator_node_pinctrl_0(fdt, offset, c);
	if (ret)
		return ret;

	return parse_pwm_regulator_node_voltage_table(fdt, offset, c);
}

static int parse_dt(struct pwm_dt_config *c)
{
	void *fdt = (void *)pwm_dt_get_fdt_addr();
	int offset;

	if (!fdt) {
		printf("pmic-pwm: %s: invalid fdt_addr: %p\n", __func__, fdt);
		return PMIC_ERR;
	}

	offset = fdt_path_offset(fdt, "/pwm-regulator");
	if (offset < 0) {
		printf("pmic-pwm: %s: no /pwm-regulator\n", __func__);
		return PMIC_ERR;
	}

	return parse_pwm_regulator_node(fdt, offset, c);
}

static int pwm_dt_probe(struct pmic_device *dev)
{
	struct pwm_dt_config *c;
	int ret;

	c = malloc(sizeof(*c));
	if (!c)
		return PMIC_ERR;

	memcpy(c, &pwm_dt_config_template, sizeof(*c));
	ret = parse_dt(c);
	if (ret) {
		free(c);
		return ret;
	}
	c->ents[PWM_VIRT_REG_VOLT].u32_array.array = c->volt_table;
	c->ents[PWM_VIRT_REG_VOLT].size = c->volt_table_size;

	dev->n_ents  = 2;
	dev->ents    = c->ents;
	dev->private = c;
	snprintf(dev->full_name, sizeof(dev->full_name), "%s-pwm%d-%d", dev->name, c->id, c->period);

	rtk_pwm_init();

	return 0;
}
DEFINE_PMIC_PWM_DRIVER(pwm_dt, pwm_dt_probe);
