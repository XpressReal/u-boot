#include <stdlib.h>
#include <rtk_i2c-lib.h>
#include <pmic.h>

static const struct type_ops i2c_ops;

struct pmic_device *i2c_create_device(struct pmic_driver *drv, int ch, int addr)
{
	struct pmic_device *dev;

	dev = malloc(sizeof(*dev));
	if (!dev)
		return NULL;
	memset(dev, 0, sizeof(*dev));

	if (drv->dev_cnt == 0)
		snprintf(dev->name, sizeof(dev->name), "%s", drv->name);
	else
		snprintf(dev->name, sizeof(dev->name), "%s-%d", drv->name, drv->dev_cnt);
	snprintf(dev->full_name, sizeof(dev->full_name), "%s-i2c%x-0x%x", drv->name, ch, addr);
	dev->i2c_ch = ch;
	dev->i2c_addr = addr;
	dev->ops = &i2c_ops;
	return dev;
}

static int i2c_write_reg(struct pmic_device *dev, unsigned char reg, unsigned char val)
{
	unsigned char data[2] = { reg, val };
	int ret;
	int cmd_ret = PMIC_OK;

	I2CN_Init(dev->i2c_ch);

	ret = I2C_Write_EX(dev->i2c_ch, dev->i2c_addr, 2, data, NO_READ);
	if (ret)
		cmd_ret = PMIC_ERR;

	I2CN_UnInit(dev->i2c_ch);
	return cmd_ret;
}

static int i2c_read_reg(struct pmic_device *dev, unsigned char reg, unsigned char *val)
{
	unsigned char out;
	int ret;
	int cmd_ret = PMIC_OK;

	I2CN_Init(dev->i2c_ch);

	ret = I2C_Write_EX(dev->i2c_ch, dev->i2c_addr, 1, &reg, NON_STOP);
	ret = I2C_Read_EX(dev->i2c_ch, dev->i2c_addr, 1, &reg, 1, &out, NON_STOP);
	if (ret)
		cmd_ret = PMIC_ERR;
	else
		*val = out;

	I2CN_UnInit(dev->i2c_ch);
	return cmd_ret;
}

static const struct type_ops i2c_ops = {
	.read = i2c_read_reg,
	.write = i2c_write_reg,
};
