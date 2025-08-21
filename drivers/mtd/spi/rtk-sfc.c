// SPDX-License-Identifier: GPL-2.0-or-later
/*
 * Realtek SPI Nor Flash Controller Driver
 *
 * Copyright (c) 2021 Realtek Technologies Co., Ltd.
 */
#ifndef __UBOOT__
#include <linux/bitops.h>
#include <linux/clk.h>
#include <linux/dma-mapping.h>
#include <linux/iopoll.h>
#include <linux/module.h>
#include <linux/mtd/mtd.h>
#include <linux/mtd/partitions.h>
#include <linux/mtd/spi-nor.h>
#include <linux/of.h>
#include <linux/platform_device.h>
#include <linux/slab.h>
#include <linux/regmap.h>
#include <linux/mfd/syscon.h>
#else
#include <common.h>
#include <dm.h>
#include <dm/device_compat.h>
#include <dm/devres.h>
#include <spi.h>
#include <spi-mem.h>
//#include <fdtdec.h>
//#include <log.h>

#include <linux/bitops.h>
#include <linux/compat.h>
#include <linux/mtd/mtd.h>
//#include <linux/mtd/partitions.h>
#include <linux/mtd/spi-nor.h>
#include <linux/io.h>
#include <linux/ioport.h>
#include <linux/delay.h>
#include <regmap.h>
#include <syscon.h>
#endif /* __UBOOT__ */

#define SB2_SFC_OFFSET		(0x800)
#define SFC_OPCODE		(SB2_SFC_OFFSET + 0x00)
#define SFC_CTL			(SB2_SFC_OFFSET + 0x04)
#define SFC_SCK			(SB2_SFC_OFFSET + 0x08)
#define SFC_CE			(SB2_SFC_OFFSET + 0x0c)
#define SFC_WP			(SB2_SFC_OFFSET + 0x10)
#define SFC_POS_LATCH		(SB2_SFC_OFFSET + 0x14)
#define SFC_WAIT_WR		(SB2_SFC_OFFSET + 0x18)
#define SFC_EN_WR		(SB2_SFC_OFFSET + 0x1c)
#define SFC_FAST_RD		(SB2_SFC_OFFSET + 0x20)
#define SFC_SCK_TAP		(SB2_SFC_OFFSET + 0x24)
#define SFP_OPCODE2		(SB2_SFC_OFFSET + 0x28)

#define MD_BASE_ADDE		0x9801b700
#define MD_FDMA_DDR_SADDR	(0x0c)
#define MD_FDMA_FL_SADDR	(0x10)
#define MD_FDMA_CTRL2		(0x14)
#define MD_FDMA_CTRL1		(0x18)

#define RTKSFC_DMA_MAX_LEN	0x100
#define RTKSFC_MAX_CHIP_NUM	1
#define RTKSFC_WAIT_TIMEOUT	1000000	
#define RTKSFC_OP_READ		0x0
#define RTKSFC_OP_WRITE		0x1

#define NOR_BASE_PHYS		0x88100000

struct rtksfc_host {
#ifndef __UBOOT__
	struct device *dev;
	struct mutex lock;
#else
	struct udevice *dev;
#endif /* __UBOOT__ */
	struct regmap *sb2;
	void __iomem *iobase;
	void __iomem *mdbase;
#ifndef __UBOOT__
	void *buffer;
	dma_addr_t dma_buffer;
#endif /* __UBOOT__ */
};

#ifdef __UBOOT__
#define msleep(a) udelay(a * 1000)
#endif /* __UBOOT__ */

static int rtk_spi_nor_read_status(struct rtksfc_host *host)
{
	int i = 100;

	while (i--) {
		regmap_write(host->sb2, SFC_OPCODE, 0x05);
		regmap_write(host->sb2, SFC_CTL, 0x10);

		if ((*(volatile unsigned char *)host->iobase) & 0x1)
			msleep(100);
		else 
			return 0;
	}

	return -1;
}

static void rtk_spi_nor_read_mode(struct rtksfc_host *host)
{
	unsigned int tmp;

	regmap_write(host->sb2, SFC_OPCODE, 0x03);
	regmap_write(host->sb2, SFC_CTL, 0x18);
	tmp = *(volatile unsigned int *)(host->iobase);
}

#ifndef __UBOOT__
static void rtk_spi_nor_dualread_mode(struct rtksfc_host *host)
{
	unsigned int tmp;

	regmap_write(host->sb2, SFC_OPCODE, 0x23b);
	regmap_write(host->sb2, SFC_CTL, 0x19);
	tmp = *(volatile unsigned int *)(host->iobase);
}
#endif /* __UBOOT__ */

static void rtk_spi_nor_write_mode(struct rtksfc_host *host)
{
	regmap_write(host->sb2, SFC_OPCODE, 0x02);
	regmap_write(host->sb2, SFC_CTL, 0x18);
}

static void rtk_spi_nor_enable_auto_write(struct rtksfc_host *host)
{
	regmap_write(host->sb2, SFC_WAIT_WR, 0x105);
	regmap_write(host->sb2, SFC_EN_WR, 0x106);
}

static void rtk_spi_nor_disable_auto_write(struct rtksfc_host *host)
{
	regmap_write(host->sb2, SFC_WAIT_WR, 0x005);
	regmap_write(host->sb2, SFC_EN_WR, 0x006);
}

unsigned int rtk_set_reg(void __iomem *addr, unsigned int offset, int length, unsigned int value)
{
	unsigned int value1, temp;
	int j;

	value1 = readl(addr);

	temp = 1;

	for (j = 0; j < length; j++)
		temp = temp * 2;

	value1 = (value1 & ~((temp - 1) << offset)) | (value << offset);

	writel(value1, addr);

	return value1;
}

static void rtk_spi_nor_init_setting(void)
{
	void __iomem *regbase;

	regbase = ioremap(0x98000018, 0x4);
	writel(readl(regbase) & (( ~(0x1 << 0)) | (0x0 << 0)), regbase);
	iounmap(regbase);

	regbase = ioremap(0x9804e000, 0x200);
	rtk_set_reg(regbase + 0x120, 11, 1, 1);
	rtk_set_reg(regbase, 24, 8, 0xAA);
	iounmap(regbase);
}

static void rtk_spi_nor_driving(unsigned int vdd, unsigned int p_drv, unsigned int n_drv)
{
	void __iomem *regbase;

	regbase = ioremap(0x9804E000, 0x100);

	/* CLK */
	rtk_set_reg(regbase + 0x2c, 12, 1, vdd);
	rtk_set_reg(regbase + 0x2c, 9, 3, p_drv);
	rtk_set_reg(regbase + 0x2c, 6, 3, n_drv);
	rtk_set_reg(regbase + 0x2c, 1, 1, 0);
	rtk_set_reg(regbase + 0x2c, 0, 1, 1);

	/* CSN */
	rtk_set_reg(regbase + 0x2c, 25, 1, vdd);
	rtk_set_reg(regbase + 0x2c, 22, 3, p_drv);
	rtk_set_reg(regbase + 0x2c, 19, 3, n_drv);
	rtk_set_reg(regbase + 0x2c, 14, 1, 1);
	rtk_set_reg(regbase + 0x2c, 13, 1, 1);

	/* MOSI */
	rtk_set_reg(regbase + 0x28, 27, 1, vdd);
	rtk_set_reg(regbase + 0x28, 24, 3, p_drv);
	rtk_set_reg(regbase + 0x28, 21, 3, n_drv);
	rtk_set_reg(regbase + 0x28, 16, 1, 0);
	rtk_set_reg(regbase + 0x28, 15, 1, 1);

	/* MISO */
	rtk_set_reg(regbase + 0x30, 6, 1, vdd);
	rtk_set_reg(regbase + 0x30, 3, 3, p_drv);
	rtk_set_reg(regbase + 0x30, 0, 3, n_drv);
	rtk_set_reg(regbase + 0x2c, 27, 1, 1);
	rtk_set_reg(regbase + 0x2c, 26, 1, 1);

	iounmap(regbase);
}

static void rtk_spi_nor_init(struct rtksfc_host *host)
{
	rtk_spi_nor_init_setting();

	host->iobase = ioremap(NOR_BASE_PHYS, 0x2000000);
	host->mdbase = ioremap(MD_BASE_ADDE, 0x30);

	regmap_write(host->sb2, SFC_SCK, 0x00000013);
	regmap_write(host->sb2, SFC_CE, 0x001a1307);
	regmap_write(host->sb2, SFC_POS_LATCH, 0x00000000);
	regmap_write(host->sb2, SFC_WAIT_WR, 0x00000005);
	regmap_write(host->sb2, SFC_EN_WR, 0x00000006);

	rtk_spi_nor_driving(1, 0x1, 0x0);

	return;	
}

static int rtkspi_command_read(struct rtksfc_host *host, const struct spi_mem_op *op)
{
	u8 opcode;
	size_t len = op->data.nbytes;
	unsigned int value;

	if (op->cmd.dtr)
		opcode = op->cmd.opcode >> 8;
	else
		opcode = op->cmd.opcode;

	regmap_write(host->sb2, SFC_OPCODE, opcode);
	regmap_write(host->sb2, SFC_CTL, 0x00000010);
	value = *(volatile unsigned int *)host->iobase;
	memcpy_fromio(op->data.buf.in, (unsigned char *)host->iobase, len);

	return 0;
}

static int rtkspi_command_write(struct rtksfc_host *host, const struct spi_mem_op *op)
{
	u8 opcode;
	const u8 *buf = op->data.buf.out;
	unsigned int tmp;
	int ret = 0;

	if (op->cmd.dtr)
		opcode = op->cmd.opcode >> 8;
	else
		opcode = op->cmd.opcode;

	regmap_write(host->sb2, SFC_OPCODE, opcode);

	switch (opcode) {
	case SPINOR_OP_WRSR:
		regmap_write(host->sb2, SFC_CTL, 0x10);
		*(volatile unsigned char *)(host->iobase) = buf[0];
		break;	
	
	case SPINOR_OP_WREN:
		regmap_write(host->sb2, SFC_CTL, 0x0);
		tmp = *(volatile unsigned char *)(host->iobase);	
		break;

	case SPINOR_OP_WRDI:
		regmap_write(host->sb2, SFC_CTL, 0x0);
		break;

	case SPINOR_OP_EN4B:
		regmap_write(host->sb2, SFC_CTL, 0x0);
		tmp = *(volatile unsigned int *)(host->iobase);
		/* controller setting */
		regmap_write(host->sb2, SFP_OPCODE2, 0x1);
		break;

	case SPINOR_OP_EX4B:
		regmap_write(host->sb2, SFC_CTL, 0x0);
		tmp = *(volatile unsigned int *)(host->iobase);
		/* controller setting */
		regmap_write(host->sb2, SFP_OPCODE2, 0x0);
		break;

	case SPINOR_OP_BE_4K:
	case SPINOR_OP_BE_4K_4B:
	case SPINOR_OP_SE:
	case SPINOR_OP_SE_4B:
		regmap_write(host->sb2, SFC_CTL, 0x08);
		tmp = *(volatile unsigned char *)(host->iobase + op->addr.val);
		ret = rtk_spi_nor_read_status(host);
		break;
	case SPINOR_OP_CHIP_ERASE:
		pr_info("Erase whole flash.\n");
		regmap_write(host->sb2, SFC_CTL, 0x0);
		tmp = *(volatile unsigned char *)host->iobase;
		ret = rtk_spi_nor_read_status(host);
		break;

	default:
		pr_warn("rtkspi_command_write, unknow command:0x%x\n", opcode);
		break;
		//return -ENOTSUPP;
	}

	return ret;
}

static int rtk_spi_nor_byte_transfer(struct rtksfc_host *host, loff_t offset,
				size_t len, unsigned char *buf, u8 op_type)
{
	if (op_type == RTKSFC_OP_READ)
		memcpy_fromio(buf, (unsigned char *)(host->iobase + offset), len);
	else
		memcpy_toio((unsigned char *)(host->iobase + offset), buf, len);

	return len;
}

#ifndef __UBOOT__
static int rtk_spi_nor_dma_transfer(struct rtksfc_host *host, loff_t offset,
					size_t len, u8 op_type)
{
	unsigned int val;

	writel(0x0a, host->mdbase + MD_FDMA_CTRL1);

	/* setup MD DDR addr and flash addr */
	writel((unsigned long)(host->dma_buffer),  
				host->mdbase + MD_FDMA_DDR_SADDR);
	writel((unsigned long)((volatile u8*)(NOR_BASE_PHYS + offset)),  
				host->mdbase + MD_FDMA_FL_SADDR);

	if (op_type == RTKSFC_OP_READ)
		val = (0xC000000 | len);
	else
		val = (0x6000000 | len);
	
	writel(val, host->mdbase + MD_FDMA_CTRL2);
	/* go */
	writel(0x03, host->mdbase + MD_FDMA_CTRL1);

	while (readl(host->mdbase + MD_FDMA_CTRL1) & 0x1)
		udelay(1);

	return rtk_spi_nor_read_status(host);
}
#endif /* __UBOOT__ */

static ssize_t rtk_spi_nor_read(struct rtksfc_host *host, const struct spi_mem_op *op)
{
	loff_t from, n_from;
	size_t len;
	size_t n_len = 0, r_len = 0;
	unsigned int offset;
	u_char *read_buf = op->data.buf.in;
	int ret;
#ifndef __UBOOT__
	int i;
#endif /* __UBOOT__ */

	from = op->addr.val;
	len = op->data.nbytes;

	offset = from & 0x3;

	/* Byte stage */
	if (offset != 0) {
		r_len = (offset > len) ? len : offset;
		rtk_spi_nor_read_mode(host);
		ret = rtk_spi_nor_byte_transfer(host, from, r_len, (u8 *)read_buf,
							RTKSFC_OP_READ);
	}

	n_from = from + r_len;
	n_len = len - r_len;

#ifndef __UBOOT__
	/* DMA stage */
	while (n_len > 0) {
		r_len = (n_len >= RTKSFC_DMA_MAX_LEN) ? RTKSFC_DMA_MAX_LEN : n_len;

		if (op->cmd.opcode == 0x3b)
			rtk_spi_nor_dualread_mode(host);
		else
			rtk_spi_nor_read_mode(host);

		ret = rtk_spi_nor_dma_transfer(host, n_from, r_len,
							RTKSFC_OP_READ);
		if (ret) {
			pr_err("DMA read timeout\n");
			return ret;
		}

		for (i = 0; i < r_len; i++)
			*(u8 *)(read_buf + offset + i) = *(u8 *)(host->buffer + i);

		n_len -= r_len;
		offset += r_len;
		n_from += r_len;
	}
#else
	if (n_len > 0) {
		rtk_spi_nor_read_mode(host);
		ret = rtk_spi_nor_byte_transfer(host, n_from, n_len, (u8 *)read_buf + r_len,
							RTKSFC_OP_READ);
	}
#endif /* __UBOOT__ */

	return len;
}

static ssize_t rtk_spi_nor_write(struct rtksfc_host *host, const struct spi_mem_op *op)
{
	loff_t to = op->addr.val;
	size_t len = op->data.nbytes;
	const u_char *write_buf = op->data.buf.out;
	int r_len = (int)len, w_len = 0;
	u_char *w_buf = (u_char *)write_buf;
	int offset;
	int ret;
#ifndef __UBOOT__
	int i;
#endif /* __UBOOT__ */

	rtk_spi_nor_enable_auto_write(host);
	offset = to & 0x3;

	/* byte stage */
	if (offset != 0) {
		w_len = (offset > len) ? len : offset;
		rtk_spi_nor_write_mode(host);
		ret = rtk_spi_nor_byte_transfer(host, to, w_len, (u8 *)w_buf,
							RTKSFC_OP_WRITE);
		w_buf += w_len;
	}

	to = to + w_len;
	r_len = (int)len - w_len;

#ifndef __UBOOT__
	/* DMA stage */
	offset = 0;
	while (r_len > 0) {
		w_len = (r_len >= RTKSFC_DMA_MAX_LEN) ? RTKSFC_DMA_MAX_LEN : r_len;

		for(i = 0; i < w_len; i++)
			*(u8 *)(host->buffer + i) = *(u8 *)(w_buf + i);

		rtk_spi_nor_write_mode(host);

		ret = rtk_spi_nor_dma_transfer(host, to + offset, w_len,
							RTKSFC_OP_WRITE);

		r_len = r_len - w_len;
		offset = offset + w_len;
		w_buf += w_len;
	}
#else
	if (r_len > 0) {
		rtk_spi_nor_write_mode(host);
		ret = rtk_spi_nor_byte_transfer(host, to, r_len, (u8 *)w_buf + w_len,
							RTKSFC_OP_WRITE);
	}
#endif /* __UBOOT__ */

	rtk_spi_nor_read_mode(host);
	rtk_spi_nor_disable_auto_write(host);

	return len;
}

#ifndef __UBOOT__
static int rtk_nor_exec_op(struct spi_mem *mem, const struct spi_mem_op *op)
#else
static int rtk_nor_exec_op(struct spi_slave *slave, const struct spi_mem_op *op)
#endif
{
#ifndef __UBOOT__
	struct rtksfc_host *host = spi_controller_get_devdata(mem->spi->master);
#else
	struct udevice *dev = dev_get_parent(slave->dev);
	struct rtksfc_host *host = dev_get_priv(dev);
#endif

	if ((op->data.nbytes == 0) ||
	    ((op->addr.nbytes != 3) && (op->addr.nbytes != 4))) {
		if (op->data.dir == SPI_MEM_DATA_IN)
			return rtkspi_command_read(host, op);
		else
			return rtkspi_command_write(host, op);
	}

	if (op->data.dir == SPI_MEM_DATA_OUT)
		return rtk_spi_nor_write(host, op);

	if (op->data.dir == SPI_MEM_DATA_IN)
		return rtk_spi_nor_read(host, op);

	pr_warn("exec_op, unknow command:0x%x\n", op->cmd.opcode);
	return -EINVAL;
}

static bool rtk_nor_supports_op(struct spi_mem *mem,
				const struct spi_mem_op *op)
{
	if (op->cmd.buswidth > 1)
		return false;

	if (op->addr.nbytes != 0) {
		if (op->addr.buswidth > 1)
			return false;
		if (op->addr.nbytes < 3 || op->addr.nbytes > 4)
			return false;
	}

	if (op->dummy.nbytes != 0) {
		if (op->dummy.buswidth > 1 || op->dummy.nbytes > 7)
			return false;
	}

	if (op->data.nbytes != 0 && op->data.buswidth > 4)
		return false;

	return spi_mem_default_supports_op(mem, op);
}

static const struct spi_controller_mem_ops rtk_nor_mem_ops = {
	.supports_op = rtk_nor_supports_op,
	.exec_op = rtk_nor_exec_op,
};

#ifndef __UBOOT__
static int rtk_spi_nor_probe(struct platform_device *pdev)
#else
static int rtk_spi_nor_probe(struct udevice *dev)
#endif /* __UBOOT__ */
{
#ifndef __UBOOT__
	struct spi_controller *ctrl;
	struct device *dev = &pdev->dev;
	struct rtksfc_host *host;
#else
	struct rtksfc_host *host = dev_get_priv(dev);
#endif /* __UBOOT__ */
	int ret = 0;

#ifndef __UBOOT__
	ctrl = spi_alloc_master(&pdev->dev, sizeof(*host));
	if (!ctrl)
		return -ENOMEM;

	ctrl->mode_bits = SPI_RX_DUAL | SPI_RX_QUAD | SPI_TX_DUAL | SPI_TX_QUAD;
	ctrl->bus_num = -1;
	ctrl->mem_ops = &rtk_nor_mem_ops;
	ctrl->num_chipselect = 1;
	ctrl->dev.of_node = pdev->dev.of_node;
	platform_set_drvdata(pdev, ctrl);
	host = spi_controller_get_devdata(ctrl);
	host->sb2 = syscon_node_to_regmap(pdev->dev.of_node->parent);
#else
	host->sb2 = syscon_node_to_regmap(dev_ofnode(dev_get_parent(dev)));
#endif /* __UBOOT__ */
	host->dev = dev;

	if (IS_ERR(host->sb2))
		return PTR_ERR(host->sb2);

#ifndef __UBOOT__
	ret = dma_set_mask_and_coherent(&pdev->dev, DMA_BIT_MASK(32));
	if (ret) {
		pr_err("Unable to set dma mask\n");
		return ret;
	}

	host->buffer = dmam_alloc_coherent(&pdev->dev, RTKSFC_DMA_MAX_LEN,
			&host->dma_buffer, GFP_KERNEL);
	if (!host->buffer)
		return -ENOMEM;

	mutex_init(&host->lock);
#endif /* __UBOOT__ */

	rtk_spi_nor_init(host);

#ifndef __UBOOT__
	ret = spi_register_controller(ctrl);
	if (ret < 0)
		mutex_destroy(&host->lock);
#endif /* __UBOOT__ */

	return ret;
}

#ifndef __UBOOT__
static int rtk_spi_nor_remove(struct platform_device *pdev)
{
	struct spi_controller *ctrl = platform_get_drvdata(pdev);
	struct rtksfc_host *host = spi_controller_get_devdata(ctrl);

	mutex_destroy(&host->lock);

	return 0;
}

static const struct of_device_id rtk_spi_nor_dt_ids[] = {
	{ .compatible = "realtek,rtd16xxb-sfc"},
	{ .compatible = "realtek,rtd13xx-sfc"},
	{ /* sentinel */ }
};
MODULE_DEVICE_TABLE(of, rtk_spi_nor_dt_ids);

static struct platform_driver rtk_spi_nor_driver = {
	.driver = {
		.name	= "rtk-sfc",
		.of_match_table = rtk_spi_nor_dt_ids,
	},
	.probe	= rtk_spi_nor_probe,
	.remove	= rtk_spi_nor_remove,
};

//module_platform_driver(rtk_spi_nor_driver);
static int __init rtk_sfc_init(void)
{
	return platform_driver_register(&rtk_spi_nor_driver);
}
late_initcall(rtk_sfc_init);

static void __exit rtk_sfc_exit(void)
{
	platform_driver_unregister(&rtk_spi_nor_driver);
}
module_exit(rtk_sfc_exit);

MODULE_LICENSE("GPL");
MODULE_AUTHOR("<jyanchou@realtek.com>");
MODULE_DESCRIPTION("Realtek SPI Nor Flash Controller Driver");
#else

static int rtk_spi_set_speed(struct udevice *dev, uint speed)
{
	return 0;
}

static int rtk_spi_set_mode(struct udevice *dev, uint mode)
{
	return 0;
}

static const struct dm_spi_ops rtk_spi_ops = {
	.mem_ops	= &rtk_nor_mem_ops,
	.set_speed	= rtk_spi_set_speed,
	.set_mode	= rtk_spi_set_mode,
};

static const struct udevice_id rtk_spi_nor_dt_ids[] = {
	{ .compatible = "realtek,rtd16xxb-sfc"},
	{ .compatible = "realtek,rtd13xx-sfc"},
	{ }
};

U_BOOT_DRIVER(rtk_sfc) = {
	.name		= "rtk_sfc",
	.id		= UCLASS_SPI,
	.of_match	= rtk_spi_nor_dt_ids,
	.ops		= &rtk_spi_ops,
	.probe		= rtk_spi_nor_probe,
	.priv_auto	= sizeof(struct rtksfc_host),
};
#endif /* __UBOOT__ */
