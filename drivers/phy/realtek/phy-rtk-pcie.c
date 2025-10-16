// SPDX-License-Identifier: GPL-2.0+
/*
 * Realtek PCIe Controller PHY Driver for U-Boot
 *
 * Copyright (C) 2024
 *
 * Ported from Linux kernel driver
 * Original: Copyright (C) 2020 Realtek
 */

#include <common.h>
#include <dm.h>
#include <dm/device_compat.h>
#include <generic-phy.h>
#include <regmap.h>
#include <syscon.h>
#include <asm/io.h>
#include <linux/bitops.h>
#include <linux/delay.h>
#include <linux/err.h>

/* MDIO Control Register */
#define PCIE_MDIO_CTR			0xC1C
#define LINK_CONTROL_LINK_STATUS_REG	0x80

/* MDIO bits */
#define MDIO_BUSY			BIT(7)
#define MDIO_RDY			BIT(4)
#define MDIO_SRST			BIT(1)
#define MDIO_WRITE			BIT(0)
#define MDIO_REG_SHIFT			8
#define MDIO_DATA_SHIFT			16

struct rtd_pcie_phy {
	struct udevice *dev;
	struct regmap *pcie_base;
	int slot;	/* PCIe slot number (1 or 2) */
};

/* Forward declarations for calibration functions */
static int pcie_front_end_offset_calibrate(struct rtd_pcie_phy *rtd_phy);
static int pcie_OOBS_calibrate(struct rtd_pcie_phy *rtd_phy);

/**
 * mdio_reset() - Reset MDIO interface
 */
static void mdio_reset(struct rtd_pcie_phy *rtd_phy)
{
	unsigned int val;

	debug("PCIe PHY: MDIO reset\n");

	/* Read current MDIO_CTR state before reset */
	regmap_read(rtd_phy->pcie_base, PCIE_MDIO_CTR, &val);
	debug("PCIe PHY: MDIO_CTR before reset = 0x%08x (BUSY=%d)\n",
	       val, !!(val & MDIO_BUSY));

	/* Issue MDIO reset */
	regmap_write(rtd_phy->pcie_base, PCIE_MDIO_CTR, MDIO_SRST | MDIO_WRITE);

	/* Wait a bit for reset to take effect */
	udelay(100);

	/* Read state after reset */
	regmap_read(rtd_phy->pcie_base, PCIE_MDIO_CTR, &val);
	debug("PCIe PHY: MDIO_CTR after reset = 0x%08x (BUSY=%d)\n",
	       val, !!(val & MDIO_BUSY));
}

/**
 * mdio_wait_busy() - Wait for MDIO to become ready
 */
static int mdio_wait_busy(struct rtd_pcie_phy *rtd_phy)
{
	unsigned int val;
	int cnt = 0;

	regmap_read(rtd_phy->pcie_base, PCIE_MDIO_CTR, &val);
	while ((val & MDIO_BUSY) && cnt < 10) {
		udelay(10);
		regmap_read(rtd_phy->pcie_base, PCIE_MDIO_CTR, &val);
		cnt++;
	}

	if (val & MDIO_BUSY)
		return -EBUSY;

	return 0;
}

/**
 * write_mdio_reg() - Write to MDIO register
 */
static int write_mdio_reg(struct rtd_pcie_phy *rtd_phy, u8 reg, u16 data)
{
	unsigned int val;

	if (mdio_wait_busy(rtd_phy)) {
		dev_err(rtd_phy->dev, "MDIO is busy (write)\n");
		return -EBUSY;
	}

	val = (reg << MDIO_REG_SHIFT) |
	      (data << MDIO_DATA_SHIFT) | MDIO_WRITE;
	regmap_write(rtd_phy->pcie_base, PCIE_MDIO_CTR, val);

	return 0;
}

/**
 * read_mdio_reg() - Read from MDIO register
 */
static int read_mdio_reg(struct rtd_pcie_phy *rtd_phy, u8 reg)
{
	unsigned int addr;
	unsigned int val;

	if (mdio_wait_busy(rtd_phy)) {
		dev_err(rtd_phy->dev, "MDIO is busy (read)\n");
		return -EBUSY;
	}

	addr = reg << MDIO_REG_SHIFT;
	regmap_write(rtd_phy->pcie_base, PCIE_MDIO_CTR, addr);

	if (mdio_wait_busy(rtd_phy)) {
		dev_err(rtd_phy->dev, "MDIO is busy (read wait)\n");
		return -EBUSY;
	}

	regmap_read(rtd_phy->pcie_base, PCIE_MDIO_CTR, &val);
	return val >> MDIO_DATA_SHIFT;
}

/**
 * rtd16xxb_pcie_phy_general_init() - General PHY initialization for RTD16xxB
 */
static int rtd16xxb_pcie_phy_general_init(struct rtd_pcie_phy *rtd_phy)
{
	debug("PCIe PHY: General initialization\n");

	mdio_reset(rtd_phy);

	/* Gen1 settings */
	write_mdio_reg(rtd_phy, 0x29, 0xFF13);
	write_mdio_reg(rtd_phy, 0x2A, 0x3D60);
	write_mdio_reg(rtd_phy, 0x05, 0xFAD3);
	write_mdio_reg(rtd_phy, 0x06, 0x0013);
	write_mdio_reg(rtd_phy, 0x01, 0xA852);
	write_mdio_reg(rtd_phy, 0x0A, 0xB650);
	write_mdio_reg(rtd_phy, 0x28, 0xF802);
	write_mdio_reg(rtd_phy, 0x0A, 0xB670);
	write_mdio_reg(rtd_phy, 0x24, 0x4F10);
	write_mdio_reg(rtd_phy, 0x23, 0xCB66);
	write_mdio_reg(rtd_phy, 0x20, 0xC4CC);
	write_mdio_reg(rtd_phy, 0x22, 0x0013);
	write_mdio_reg(rtd_phy, 0x21, 0x55AA);
	write_mdio_reg(rtd_phy, 0x2F, 0xA008);
	write_mdio_reg(rtd_phy, 0x0B, 0x9905);
	write_mdio_reg(rtd_phy, 0x09, 0x720C);
	write_mdio_reg(rtd_phy, 0x29, 0xFF13);
	write_mdio_reg(rtd_phy, 0x2B, 0xA801);

	/* Gen2 settings */
	write_mdio_reg(rtd_phy, 0x69, 0xFF13);
	write_mdio_reg(rtd_phy, 0x6A, 0x3D60);
	write_mdio_reg(rtd_phy, 0x45, 0xFAD3);
	write_mdio_reg(rtd_phy, 0x5E, 0x6EEB);
	write_mdio_reg(rtd_phy, 0x46, 0x0013);
	write_mdio_reg(rtd_phy, 0x41, 0x484A);
	write_mdio_reg(rtd_phy, 0x4A, 0xB650);
	write_mdio_reg(rtd_phy, 0x68, 0xF802);
	write_mdio_reg(rtd_phy, 0x63, 0xCB66);
	write_mdio_reg(rtd_phy, 0x60, 0xC4EE);
	write_mdio_reg(rtd_phy, 0x62, 0x0013);
	write_mdio_reg(rtd_phy, 0x61, 0x55AA);
	write_mdio_reg(rtd_phy, 0x6F, 0xA008);
	write_mdio_reg(rtd_phy, 0x4B, 0x9905);
	write_mdio_reg(rtd_phy, 0x49, 0x720C);
	write_mdio_reg(rtd_phy, 0x69, 0xFF13);
	write_mdio_reg(rtd_phy, 0x6B, 0xA801);

	return 0;
}

/**
 * rtd16xxb_pcie1_phy_init() - Initialize PCIe slot 1 PHY
 */
static int rtd16xxb_pcie1_phy_init(struct rtd_pcie_phy *rtd_phy)
{
	debug("PCIe1 PHY: Initializing\n");

	mdio_reset(rtd_phy);
	rtd16xxb_pcie_phy_general_init(rtd_phy);

	/* Note: TX swing calibration from OTP is optional and skipped in U-Boot */

	pcie_OOBS_calibrate(rtd_phy);
	pcie_front_end_offset_calibrate(rtd_phy);

	debug("PCIe1 PHY: Initialization complete\n");
	return 0;
}

/**
 * rtd16xxb_pcie2_phy_init() - Initialize PCIe slot 2 PHY
 */
static int rtd16xxb_pcie2_phy_init(struct rtd_pcie_phy *rtd_phy)
{
	debug("PCIe2 PHY: Initializing\n");

	mdio_reset(rtd_phy);
	rtd16xxb_pcie_phy_general_init(rtd_phy);

	/* Note: TX swing calibration from OTP is optional and skipped in U-Boot */

	pcie_OOBS_calibrate(rtd_phy);
	pcie_front_end_offset_calibrate(rtd_phy);

	debug("PCIe2 PHY: Initialization complete\n");
	return 0;
}

/**
 * gray_to_binary() - Convert gray code to binary
 */
static u8 gray_to_binary(u8 gray)
{
	u8 binary;

	binary = gray & BIT(4);
	binary |= (gray ^ (binary >> 1)) & BIT(3);
	binary |= (gray ^ (binary >> 1)) & BIT(2);
	binary |= (gray ^ (binary >> 1)) & BIT(1);
	binary |= (gray ^ (binary >> 1)) & BIT(0);

	return binary;
}

/**
 * pcie_LEQ_calibrate() - Linear Equalizer calibration
 */
static void pcie_LEQ_calibrate(struct rtd_pcie_phy *rtd_phy, int speed)
{
	int val;
	u8 gray_code;
	u8 binary_code;

	debug("PCIe PHY: LEQ calibration for speed %d\n", speed);

	if (speed == 1) {
		val = read_mdio_reg(rtd_phy, 0x1f);
		gray_code = (val & GENMASK(15, 11)) >> 11;
		binary_code = gray_to_binary(gray_code);

		val = read_mdio_reg(rtd_phy, 0x24);
		val = (val & ~GENMASK(6, 2)) | (binary_code << 2);
		write_mdio_reg(rtd_phy, 0x24, val);

		val = read_mdio_reg(rtd_phy, 0x0a);
		val = val | BIT(5);
		write_mdio_reg(rtd_phy, 0x0a, val);
	} else if (speed == 2) {
		val = read_mdio_reg(rtd_phy, 0x5f);
		gray_code = (val & GENMASK(15, 11)) >> 11;
		binary_code = gray_to_binary(gray_code);

		val = read_mdio_reg(rtd_phy, 0x64);
		val = (val & ~GENMASK(6, 2)) | (binary_code << 2);
		write_mdio_reg(rtd_phy, 0x64, val);

		val = read_mdio_reg(rtd_phy, 0x4a);
		val = val | BIT(5);
		write_mdio_reg(rtd_phy, 0x4a, val);
	}
}

/**
 * pcie_front_end_offset_calibrate() - Front-end offset calibration
 */
static int pcie_front_end_offset_calibrate(struct rtd_pcie_phy *rtd_phy)
{
	int val;
	int cnt;

	debug("PCIe PHY: Front-end offset calibration\n");

	/* Wait for calibration ready - Lane 0 */
	cnt = 0;
	val = read_mdio_reg(rtd_phy, 0x1f);
	while (!(val & BIT(15)) && cnt < 10) {
		udelay(10);
		val = read_mdio_reg(rtd_phy, 0x1f);
		cnt++;
	}
	if (cnt == 10) {
		dev_err(rtd_phy->dev, "Front End: wait reg(0x1f) bit15 timeout\n");
		return -EBUSY;
	}

	/* Wait for calibration ready - Lane 1 */
	cnt = 0;
	val = read_mdio_reg(rtd_phy, 0x5f);
	while (!(val & BIT(15)) && cnt < 10) {
		udelay(10);
		val = read_mdio_reg(rtd_phy, 0x5f);
		cnt++;
	}
	if (cnt == 10) {
		dev_err(rtd_phy->dev, "Front End: wait reg(0x5f) bit15 timeout\n");
		return -EBUSY;
	}

	/* Apply calibration settings */
	val = read_mdio_reg(rtd_phy, 0x0D);
	val &= ~BIT(6);
	write_mdio_reg(rtd_phy, 0x0D, val);

	val = read_mdio_reg(rtd_phy, 0x4D);
	val &= ~BIT(6);
	write_mdio_reg(rtd_phy, 0x4D, val);

	val = read_mdio_reg(rtd_phy, 0x19);
	val &= ~BIT(2);
	write_mdio_reg(rtd_phy, 0x19, val);

	val = read_mdio_reg(rtd_phy, 0x59);
	val &= ~BIT(2);
	write_mdio_reg(rtd_phy, 0x59, val);

	write_mdio_reg(rtd_phy, 0x10, 0x000C);
	write_mdio_reg(rtd_phy, 0x50, 0x000C);

	/* Check calibration results */
	val = read_mdio_reg(rtd_phy, 0x1f);
	val = (val & GENMASK(4, 1)) >> 1;
	if ((val != 0x0 && val != 0xF))
		return 0;

	val = read_mdio_reg(rtd_phy, 0x5f);
	val = (val & GENMASK(4, 1)) >> 1;
	if ((val != 0x0 && val != 0xF))
		return 0;

	/* Apply correction if needed */
	val = read_mdio_reg(rtd_phy, 0x0B);
	val |= 0x3 << 2;
	write_mdio_reg(rtd_phy, 0x0B, val);

	val = read_mdio_reg(rtd_phy, 0x4B);
	val |= 0x3 << 2;
	write_mdio_reg(rtd_phy, 0x4B, val);

	val = read_mdio_reg(rtd_phy, 0x09);
	val |= BIT(9);
	write_mdio_reg(rtd_phy, 0x09, val);

	val = read_mdio_reg(rtd_phy, 0x49);
	val |= BIT(9);
	write_mdio_reg(rtd_phy, 0x49, val);

	val = read_mdio_reg(rtd_phy, 0x09);
	val &= ~BIT(9);
	write_mdio_reg(rtd_phy, 0x09, val);

	val = read_mdio_reg(rtd_phy, 0x49);
	val &= ~BIT(9);
	write_mdio_reg(rtd_phy, 0x49, val);

	val = read_mdio_reg(rtd_phy, 0x09);
	val |= BIT(9);
	write_mdio_reg(rtd_phy, 0x09, val);

	val = read_mdio_reg(rtd_phy, 0x49);
	val |= BIT(9);
	write_mdio_reg(rtd_phy, 0x49, val);

	val = read_mdio_reg(rtd_phy, 0x0D);
	val |= BIT(6);
	write_mdio_reg(rtd_phy, 0x0D, val);

	val = read_mdio_reg(rtd_phy, 0x4D);
	val |= BIT(6);
	write_mdio_reg(rtd_phy, 0x4D, val);

	val = read_mdio_reg(rtd_phy, 0x19);
	val |= BIT(2);
	write_mdio_reg(rtd_phy, 0x19, val);

	val = read_mdio_reg(rtd_phy, 0x59);
	val |= BIT(2);
	write_mdio_reg(rtd_phy, 0x59, val);

	write_mdio_reg(rtd_phy, 0x10, 0x3C4);
	write_mdio_reg(rtd_phy, 0x50, 0x3C4);

	/* Final wait for calibration */
	cnt = 0;
	val = read_mdio_reg(rtd_phy, 0x1f);
	while (!(val & BIT(15)) && cnt < 10) {
		udelay(10);
		val = read_mdio_reg(rtd_phy, 0x1f);
		cnt++;
	}
	if (cnt == 10) {
		dev_err(rtd_phy->dev, "Front End again: wait reg(0x1f) bit15 timeout\n");
		return -EBUSY;
	}

	cnt = 0;
	val = read_mdio_reg(rtd_phy, 0x5f);
	while (!(val & BIT(15)) && cnt < 10) {
		udelay(10);
		val = read_mdio_reg(rtd_phy, 0x5f);
		cnt++;
	}
	if (cnt == 10) {
		dev_err(rtd_phy->dev, "Front End again: wait reg(0x5f) bit15 timeout\n");
		return -EBUSY;
	}

	return 0;
}

/**
 * pcie_OOBS_calibrate() - Out-of-band signaling calibration
 */
static int pcie_OOBS_calibrate(struct rtd_pcie_phy *rtd_phy)
{
	int val;
	int cnt;
	int tmp;

	debug("PCIe PHY: OOBS calibration\n");

	val = read_mdio_reg(rtd_phy, 0x09);
	val &= ~BIT(4);
	write_mdio_reg(rtd_phy, 0x09, val);

	val = read_mdio_reg(rtd_phy, 0x49);
	val &= ~BIT(4);
	write_mdio_reg(rtd_phy, 0x49, val);

	val = read_mdio_reg(rtd_phy, 0x09);
	val |= BIT(9);
	write_mdio_reg(rtd_phy, 0x09, val);

	val = read_mdio_reg(rtd_phy, 0x49);
	val |= BIT(9);
	write_mdio_reg(rtd_phy, 0x49, val);

	val = read_mdio_reg(rtd_phy, 0x09);
	val &= ~BIT(9);
	write_mdio_reg(rtd_phy, 0x09, val);

	val = read_mdio_reg(rtd_phy, 0x49);
	val &= ~BIT(9);
	write_mdio_reg(rtd_phy, 0x49, val);

	val = read_mdio_reg(rtd_phy, 0x09);
	val |= BIT(9);
	write_mdio_reg(rtd_phy, 0x09, val);

	val = read_mdio_reg(rtd_phy, 0x49);
	val |= BIT(9);
	write_mdio_reg(rtd_phy, 0x49, val);

	val = read_mdio_reg(rtd_phy, 0x0D);
	val |= BIT(6);
	write_mdio_reg(rtd_phy, 0x0D, val);

	val = read_mdio_reg(rtd_phy, 0x4D);
	val |= BIT(6);
	write_mdio_reg(rtd_phy, 0x4D, val);

	val = read_mdio_reg(rtd_phy, 0x19);
	val |= BIT(2);
	write_mdio_reg(rtd_phy, 0x19, val);

	val = read_mdio_reg(rtd_phy, 0x59);
	val |= BIT(2);
	write_mdio_reg(rtd_phy, 0x59, val);

	write_mdio_reg(rtd_phy, 0x10, 0x03C4);
	write_mdio_reg(rtd_phy, 0x50, 0x03C4);

	/* Wait for Lane 0 */
	cnt = 0;
	val = read_mdio_reg(rtd_phy, 0x1f);
	while ((val & BIT(6)) != 0 && cnt < 10) {
		udelay(10);
		val = read_mdio_reg(rtd_phy, 0x1f);
		cnt++;
	}
	if (cnt == 10) {
		dev_err(rtd_phy->dev, "OOBS: wait reg(0x1f) bit6 == 0 timeout\n");
		return -EBUSY;
	}

	/* Wait for Lane 1 */
	cnt = 0;
	val = read_mdio_reg(rtd_phy, 0x5f);
	while ((val & BIT(6)) != 0 && cnt < 10) {
		udelay(10);
		val = read_mdio_reg(rtd_phy, 0x5f);
		cnt++;
	}
	if (cnt == 10) {
		dev_err(rtd_phy->dev, "OOBS: wait reg(0x5f) bit6 == 0 timeout\n");
		return -EBUSY;
	}

	mdelay(1);

	val = read_mdio_reg(rtd_phy, 0x19);
	val |= BIT(2);
	write_mdio_reg(rtd_phy, 0x19, val);

	val = read_mdio_reg(rtd_phy, 0x59);
	val |= BIT(2);
	write_mdio_reg(rtd_phy, 0x59, val);

	write_mdio_reg(rtd_phy, 0x10, 0x03C4);
	write_mdio_reg(rtd_phy, 0x50, 0x03C4);

	tmp = read_mdio_reg(rtd_phy, 0x1f);
	tmp = (tmp & GENMASK(12, 8)) >> 8;
	val = read_mdio_reg(rtd_phy, 0x03);
	val = (val & ~GENMASK(5, 1)) | (tmp << 1);
	write_mdio_reg(rtd_phy, 0x03, val);

	tmp = read_mdio_reg(rtd_phy, 0x5f);
	tmp = (tmp & GENMASK(12, 8)) >> 8;
	val = read_mdio_reg(rtd_phy, 0x43);
	val = (val & ~GENMASK(5, 1)) | (tmp << 1);
	write_mdio_reg(rtd_phy, 0x43, val);

	val = read_mdio_reg(rtd_phy, 0x09);
	val |= BIT(4);
	write_mdio_reg(rtd_phy, 0x09, val);

	val = read_mdio_reg(rtd_phy, 0x49);
	val |= BIT(4);
	write_mdio_reg(rtd_phy, 0x49, val);

	return 0;
}

/**
 * rtd16xxb_pcie_phy_calibrate() - Run PHY calibration after link up
 */
static int rtd16xxb_pcie_phy_calibrate(struct rtd_pcie_phy *rtd_phy)
{
	unsigned int val;
	int speed;

	debug("PCIe PHY: Running calibration\n");

	regmap_read(rtd_phy->pcie_base, LINK_CONTROL_LINK_STATUS_REG, &val);
	speed = (val & GENMASK(19, 16)) >> 16;

	debug("PCIe PHY: Link speed = %d\n", speed);

	pcie_LEQ_calibrate(rtd_phy, speed);

	return 0;
}

/* ==================== U-Boot Generic PHY Interface ==================== */

static int rtk_pcie_phy_init(struct phy *phy)
{
	struct rtd_pcie_phy *rtd_phy = dev_get_priv(phy->dev);

	debug("PCIe%d PHY: Init called\n", rtd_phy->slot);

	if (rtd_phy->slot == 1)
		return rtd16xxb_pcie1_phy_init(rtd_phy);
	else if (rtd_phy->slot == 2)
		return rtd16xxb_pcie2_phy_init(rtd_phy);

	return -EINVAL;
}

static int rtk_pcie_phy_power_on(struct phy *phy)
{
	struct rtd_pcie_phy *rtd_phy = dev_get_priv(phy->dev);

	debug("PCIe%d PHY: Power on\n", rtd_phy->slot);

	/* PHY power on is handled by reset controller in our simplified implementation */
	return 0;
}

static int rtk_pcie_phy_power_off(struct phy *phy)
{
	struct rtd_pcie_phy *rtd_phy = dev_get_priv(phy->dev);

	debug("PCIe%d PHY: Power off\n", rtd_phy->slot);

	/* PHY power off is handled by reset controller in our simplified implementation */
	return 0;
}

static int rtk_pcie_phy_configure(struct phy *phy, void *params)
{
	struct rtd_pcie_phy *rtd_phy = dev_get_priv(phy->dev);

	debug("PCIe%d PHY: Configure/calibrate\n", rtd_phy->slot);

	return rtd16xxb_pcie_phy_calibrate(rtd_phy);
}

static const struct phy_ops rtk_pcie_phy_ops = {
	.init = rtk_pcie_phy_init,
	.power_on = rtk_pcie_phy_power_on,
	.power_off = rtk_pcie_phy_power_off,
	.configure = rtk_pcie_phy_configure,
};

static int rtk_pcie_phy_probe(struct udevice *dev)
{
	struct rtd_pcie_phy *rtd_phy = dev_get_priv(dev);
	struct udevice *syscon_dev;
	int ret;

	rtd_phy->dev = dev;

	/* Get PCIe controller syscon (for MDIO register access) */
	ret = uclass_get_device_by_phandle(UCLASS_SYSCON, dev, "syscon", &syscon_dev);
	if (ret) {
		dev_err(dev, "Failed to get syscon device: %d\n", ret);
		return ret;
	}

	rtd_phy->pcie_base = syscon_get_regmap(syscon_dev);
	if (!rtd_phy->pcie_base) {
		dev_err(dev, "Failed to get regmap\n");
		return -EINVAL;
	}

	/* Determine slot number from compatible string */
	if (dev_read_bool(dev, "realtek,rtd16xxb-pcie-slot1-phy"))
		rtd_phy->slot = 1;
	else if (dev_read_bool(dev, "realtek,rtd16xxb-pcie-slot2-phy"))
		rtd_phy->slot = 2;
	else {
		/* Try to parse from compatible */
		const char *compatible = dev_read_string(dev, "compatible");
		if (strstr(compatible, "slot1"))
			rtd_phy->slot = 1;
		else if (strstr(compatible, "slot2"))
			rtd_phy->slot = 2;
		else {
			dev_err(dev, "Cannot determine PCIe slot number\n");
			return -EINVAL;
		}
	}

	dev_info(dev, "PCIe%d PHY driver initialized\n", rtd_phy->slot);

	return 0;
}

static const struct udevice_id rtk_pcie_phy_ids[] = {
	{ .compatible = "realtek,rtd16xxb-pcie-slot1-phy" },
	{ .compatible = "realtek,rtd16xxb-pcie-slot2-phy" },
	{ }
};

U_BOOT_DRIVER(rtk_pcie_phy) = {
	.name = "rtk_pcie_phy",
	.id = UCLASS_PHY,
	.of_match = rtk_pcie_phy_ids,
	.probe = rtk_pcie_phy_probe,
	.ops = &rtk_pcie_phy_ops,
	.priv_auto = sizeof(struct rtd_pcie_phy),
};
