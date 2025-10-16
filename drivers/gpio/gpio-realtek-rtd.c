// SPDX-License-Identifier: GPL-2.0+
/*
 * Realtek RTD GPIO driver for U-Boot
 *
 * Copyright (C) 2024
 *
 * Simplified from Linux kernel driver
 * Original: Copyright (c) 2023 Realtek Semiconductor Corp.
 */

#include <common.h>
#include <dm.h>
#include <dm/device_compat.h>
#include <errno.h>
#include <asm/gpio.h>
#include <asm/io.h>
#include <linux/bitops.h>

/**
 * struct rtd_gpio_info - GPIO register configuration
 * @name: GPIO device name
 * @num_gpios: The number of GPIOs
 * @dir_offset: Offset for GPIO direction registers
 * @dato_offset: Offset for GPIO data output registers
 * @dati_offset: Offset for GPIO data input registers
 */
struct rtd_gpio_info {
	const char	*name;
	unsigned int	num_gpios;
	u8		dir_offset[3];
	u8		dato_offset[3];
	u8		dati_offset[3];
};

struct rtd_gpio_priv {
	void __iomem			*base;
	const struct rtd_gpio_info	*info;
};

/* RTD1619B ISO GPIO configuration - 82 GPIOs */
static const struct rtd_gpio_info rtd_iso_gpio_info = {
	.name			= "rtd_iso_gpio",
	.num_gpios		= 82,
	.dir_offset		= { 0x0, 0x18, 0x2c },
	.dato_offset		= { 0x4, 0x1c, 0x30 },
	.dati_offset		= { 0x8, 0x20, 0x34 },
};

static int rtd_gpio_dir_offset(struct rtd_gpio_priv *priv, unsigned int offset)
{
	return priv->info->dir_offset[offset / 32];
}

static int rtd_gpio_dato_offset(struct rtd_gpio_priv *priv, unsigned int offset)
{
	return priv->info->dato_offset[offset / 32];
}

static int rtd_gpio_dati_offset(struct rtd_gpio_priv *priv, unsigned int offset)
{
	return priv->info->dati_offset[offset / 32];
}

static int rtd_gpio_direction_input(struct udevice *dev, unsigned int offset)
{
	struct rtd_gpio_priv *priv = dev_get_priv(dev);
	int reg_offset;
	u32 val;

	if (offset >= priv->info->num_gpios)
		return -EINVAL;

	reg_offset = rtd_gpio_dir_offset(priv, offset);

	val = readl(priv->base + reg_offset);
	val &= ~BIT(offset % 32);  /* 0 = input */
	writel(val, priv->base + reg_offset);

	debug("GPIO%d: set direction input\n", offset);

	return 0;
}

static int rtd_gpio_direction_output(struct udevice *dev, unsigned int offset,
				     int value)
{
	struct rtd_gpio_priv *priv = dev_get_priv(dev);
	int dir_offset, dato_offset;
	u32 val;

	if (offset >= priv->info->num_gpios)
		return -EINVAL;

	/* Set output value first */
	dato_offset = rtd_gpio_dato_offset(priv, offset);
	val = readl(priv->base + dato_offset);
	if (value)
		val |= BIT(offset % 32);
	else
		val &= ~BIT(offset % 32);
	writel(val, priv->base + dato_offset);

	/* Then set direction to output */
	dir_offset = rtd_gpio_dir_offset(priv, offset);
	val = readl(priv->base + dir_offset);
	val |= BIT(offset % 32);  /* 1 = output */
	writel(val, priv->base + dir_offset);

	debug("GPIO%d: set direction output, value %d\n", offset, value);

	return 0;
}

static int rtd_gpio_get_value(struct udevice *dev, unsigned int offset)
{
	struct rtd_gpio_priv *priv = dev_get_priv(dev);
	int dir_offset, dato_offset, dati_offset, dat_offset;
	u32 val;

	if (offset >= priv->info->num_gpios)
		return -EINVAL;

	/* Check direction first */
	dir_offset = rtd_gpio_dir_offset(priv, offset);
	val = readl(priv->base + dir_offset);

	/* If output, read from dato; if input, read from dati */
	if (val & BIT(offset % 32)) {
		dato_offset = rtd_gpio_dato_offset(priv, offset);
		dat_offset = dato_offset;
	} else {
		dati_offset = rtd_gpio_dati_offset(priv, offset);
		dat_offset = dati_offset;
	}

	val = readl(priv->base + dat_offset);

	return !!(val & BIT(offset % 32));
}

static int rtd_gpio_set_value(struct udevice *dev, unsigned int offset,
			      int value)
{
	struct rtd_gpio_priv *priv = dev_get_priv(dev);
	int dato_offset;
	u32 val;

	if (offset >= priv->info->num_gpios)
		return -EINVAL;

	dato_offset = rtd_gpio_dato_offset(priv, offset);

	val = readl(priv->base + dato_offset);
	if (value)
		val |= BIT(offset % 32);
	else
		val &= ~BIT(offset % 32);
	writel(val, priv->base + dato_offset);

	debug("GPIO%d: set value %d\n", offset, value);

	return 0;
}

static int rtd_gpio_get_function(struct udevice *dev, unsigned int offset)
{
	struct rtd_gpio_priv *priv = dev_get_priv(dev);
	int reg_offset;
	u32 val;

	if (offset >= priv->info->num_gpios)
		return -EINVAL;

	reg_offset = rtd_gpio_dir_offset(priv, offset);
	val = readl(priv->base + reg_offset);

	if (val & BIT(offset % 32))
		return GPIOF_OUTPUT;
	else
		return GPIOF_INPUT;
}

static int rtd_gpio_probe(struct udevice *dev)
{
	struct gpio_dev_priv *uc_priv = dev_get_uclass_priv(dev);
	struct rtd_gpio_priv *priv = dev_get_priv(dev);
	fdt_addr_t addr;

	priv->info = (const struct rtd_gpio_info *)dev_get_driver_data(dev);
	if (!priv->info) {
		dev_err(dev, "Failed to get GPIO info\n");
		return -EINVAL;
	}

	/* Get base address from device tree */
	addr = dev_read_addr_index(dev, 0);
	if (addr == FDT_ADDR_T_NONE) {
		dev_err(dev, "Failed to get GPIO base address\n");
		return -EINVAL;
	}
	priv->base = (void __iomem *)addr;

	/* Setup GPIO chip */
	uc_priv->bank_name = priv->info->name;
	uc_priv->gpio_count = priv->info->num_gpios;

	dev_info(dev, "Realtek GPIO: %s, %d GPIOs at 0x%p\n",
		 priv->info->name, priv->info->num_gpios, priv->base);

	return 0;
}

static const struct dm_gpio_ops rtd_gpio_ops = {
	.direction_input	= rtd_gpio_direction_input,
	.direction_output	= rtd_gpio_direction_output,
	.get_value		= rtd_gpio_get_value,
	.set_value		= rtd_gpio_set_value,
	.get_function		= rtd_gpio_get_function,
};

static const struct udevice_id rtd_gpio_ids[] = {
	{ .compatible = "realtek,gpio", .data = (ulong)&rtd_iso_gpio_info },
	{ .compatible = "realtek,rtd1619b-iso-gpio", .data = (ulong)&rtd_iso_gpio_info },
	{ }
};

U_BOOT_DRIVER(gpio_realtek_rtd) = {
	.name	= "gpio_realtek_rtd",
	.id	= UCLASS_GPIO,
	.of_match = rtd_gpio_ids,
	.probe	= rtd_gpio_probe,
	.ops	= &rtd_gpio_ops,
	.priv_auto = sizeof(struct rtd_gpio_priv),
};
