/* SPDX-License-Identifier: GPL-2.0+ */
/*
 * Realtek PCIe Clock and Reset Helper
 *
 * Copyright (C) 2024
 *
 * Clock and reset control for PCIe on RTD1619B using regmap
 * Implements proper write-enable logic for Realtek hardware.
 */

#ifndef __PCIE_RTD_CLK_RST_H__
#define __PCIE_RTD_CLK_RST_H__

#define DEBUG

#include <linux/bitops.h>
#include <regmap.h>

/* Clock Enable Registers for PCIe */
#define CRT_CLK_EN_2			0x005c		/* PCIE1 clock */
#define CRT_CLK_EN_4			0x008c		/* PCIE2 clock */

/* PCIe1 Clock bit - Register offset 0x5c, bit 18 */
#define CLK_EN_PCIE1_BIT		18
/* PCIe2 Clock bit - Register offset 0x8c, bit 0 */
#define CLK_EN_PCIE2_BIT		0

/* Soft Reset Registers for PCIe */
#define CRT_SOFT_RESET2			0x0008		/* Not used for PCIe1/2 */
#define CRT_SOFT_RESET4			0x000c		/* PCIE1 resets */
#define CRT_SOFT_RESET9			0x0098		/* PCIE2 resets */

/* PCIe1 Reset bits - Register offset 0x50 */
#define RSTN_PCIE1_STITCH_BIT		0x10
#define RSTN_PCIE1_PHY_BIT		0x12
#define RSTN_PCIE1_BIT			0x14
#define RSTN_PCIE1_CORE_BIT		0x16
#define RSTN_PCIE1_POWER_BIT		0x18
#define RSTN_PCIE1_NONSTITCH_BIT	0x1a
#define RSTN_PCIE1_PHY_MDIO_BIT		0x1c

/* FIXME: THOSE BITS ARE WRONG */
/* PCIe2 Reset bits - Register offset 0x98 */
#define RSTN_PCIE2_STITCH_BIT		0
#define RSTN_PCIE2_PHY_BIT		1
#define RSTN_PCIE2_BIT			2
#define RSTN_PCIE2_CORE_BIT		3
#define RSTN_PCIE2_POWER_BIT		4
#define RSTN_PCIE2_NONSTITCH_BIT	5
#define RSTN_PCIE2_PHY_MDIO_BIT		6

/**
 * rtd_clk_update_bits_we() - Update register bits with write-enable logic
 * @regmap: regmap for CRT controller
 * @reg: register offset
 * @bit: bit position
 * @enable: true to enable (set bit), false to disable (clear bit)
 *
 * For Realtek hardware with write-enable logic:
 * - Bit N is the control bit
 * - Bit N+1 is the write-enable bit (must be set to modify bit N)
 * - To enable: set both bit N and N+1
 * - To disable: set only bit N+1 (bit N = 0)
 */
static inline int rtd_clk_update_bits_we(struct regmap *regmap, uint reg,
					 uint bit, bool enable)
{
	u32 mask, val;

	/* Mask covers both control bit and write-enable bit */
	mask = BIT(bit) | BIT(bit + 1);
	/* Value: always set write-enable bit, conditionally set control bit */
	val = BIT(bit + 1) | (enable ? BIT(bit) : 0);

	return regmap_update_bits(regmap, reg, mask, val);
}

/**
 * rtd_rst_update_bits_we() - Update reset bits with write-enable logic
 * @regmap: regmap for CRT controller
 * @reg: register offset
 * @bit: bit position
 * @assert: true to assert reset (clear bit), false to deassert (set bit)
 *
 * For Realtek reset logic (active-low):
 * - Bit N is the reset control bit (0=asserted, 1=deasserted)
 * - Bit N+1 is the write-enable bit
 * - To assert reset: set only bit N+1 (bit N = 0)
 * - To deassert reset: set both bit N and N+1
 */
static inline int rtd_rst_update_bits_we(struct regmap *regmap, uint reg,
					 uint bit, bool assert)
{
	u32 mask, val;

	/* Mask covers both control bit and write-enable bit */
	mask = BIT(bit) | BIT(bit + 1);
	/* Value: always set write-enable bit, conditionally set control bit */
	/* For reset: asserted=0, deasserted=1, so we invert the assert flag */
	val = BIT(bit + 1) | (!assert ? BIT(bit) : 0);

	return regmap_update_bits(regmap, reg, mask, val);
}

/**
 * rtd_pcie_clk_enable() - Enable PCIe clock
 * @regmap: regmap for CRT controller
 * @slot: PCIe slot number (1 or 2)
 *
 * Enables the clock for the specified PCIe slot.
 */
static inline int rtd_pcie_clk_enable(struct regmap *regmap, int slot)
{
	uint reg, bit;
	int ret;

	if (slot == 1) {
		reg = CRT_CLK_EN_2;
		bit = CLK_EN_PCIE1_BIT;
	} else if (slot == 2) {
		reg = CRT_CLK_EN_4;
		bit = CLK_EN_PCIE2_BIT;
	} else {
		return -EINVAL;
	}

	debug("PCIe%d: Enabling clock (reg=0x%x, bit=%d)\n", slot, reg, bit);
	ret = rtd_clk_update_bits_we(regmap, reg, bit, true);
	if (ret)
		pr_err("PCIe%d: Failed to enable clock: %d\n", slot, ret);

	return ret;
}

/**
 * rtd_pcie_reset_assert() - Assert PCIe resets
 * @regmap: regmap for CRT controller
 * @slot: PCIe slot number (1 or 2)
 *
 * Asserts all resets for the specified PCIe slot.
 */
static inline int rtd_pcie_reset_assert(struct regmap *regmap, int slot)
{
	uint reg;
	int ret = 0;

	if (slot == 1)
		reg = CRT_SOFT_RESET4;
	else if (slot == 2)
		reg = CRT_SOFT_RESET9;
	else
		return -EINVAL;

	debug("PCIe%d: Asserting resets (reg=0x%x)\n", slot, reg);

	/* Assert all PCIe resets */
	ret |= rtd_rst_update_bits_we(regmap, reg,
				      slot == 1 ? RSTN_PCIE1_STITCH_BIT : RSTN_PCIE2_STITCH_BIT, true);
	ret |= rtd_rst_update_bits_we(regmap, reg,
				      slot == 1 ? RSTN_PCIE1_BIT : RSTN_PCIE2_BIT, true);
	ret |= rtd_rst_update_bits_we(regmap, reg,
				      slot == 1 ? RSTN_PCIE1_CORE_BIT : RSTN_PCIE2_CORE_BIT, true);
	ret |= rtd_rst_update_bits_we(regmap, reg,
				      slot == 1 ? RSTN_PCIE1_POWER_BIT : RSTN_PCIE2_POWER_BIT, true);
	ret |= rtd_rst_update_bits_we(regmap, reg,
				      slot == 1 ? RSTN_PCIE1_NONSTITCH_BIT : RSTN_PCIE2_NONSTITCH_BIT, true);

	if (ret)
		pr_err("PCIe%d: Failed to assert resets: %d\n", slot, ret);

	return ret;
}

/**
 * rtd_pcie_reset_deassert() - Deassert PCIe resets
 * @regmap: regmap for CRT controller
 * @slot: PCIe slot number (1 or 2)
 *
 * Deasserts all resets for the specified PCIe slot.
 */
static inline int rtd_pcie_reset_deassert(struct regmap *regmap, int slot)
{
	uint reg;
	int ret = 0;
	u32 val_before, val_after;

	if (slot == 1)
		reg = CRT_SOFT_RESET4;
	else if (slot == 2)
		reg = CRT_SOFT_RESET9;
	else
		return -EINVAL;

	regmap_read(regmap, reg, &val_before);
	debug("PCIe%d: Reset reg 0x%x BEFORE deassert = 0x%08x\n", slot, reg, val_before);

	/* Deassert all PCIe resets */
	ret |= rtd_rst_update_bits_we(regmap, reg,
				      slot == 1 ? RSTN_PCIE1_STITCH_BIT : RSTN_PCIE2_STITCH_BIT, false);
	ret |= rtd_rst_update_bits_we(regmap, reg,
				      slot == 1 ? RSTN_PCIE1_BIT : RSTN_PCIE2_BIT, false);
	ret |= rtd_rst_update_bits_we(regmap, reg,
				      slot == 1 ? RSTN_PCIE1_CORE_BIT : RSTN_PCIE2_CORE_BIT, false);
	ret |= rtd_rst_update_bits_we(regmap, reg,
				      slot == 1 ? RSTN_PCIE1_POWER_BIT : RSTN_PCIE2_POWER_BIT, false);
	ret |= rtd_rst_update_bits_we(regmap, reg,
				      slot == 1 ? RSTN_PCIE1_NONSTITCH_BIT : RSTN_PCIE2_NONSTITCH_BIT, false);

	regmap_read(regmap, reg, &val_after);
	debug("PCIe%d: Reset reg 0x%x AFTER deassert  = 0x%08x (ret=%d)\n", slot, reg, val_after, ret);
	debug("PCIe%d: Expected all bits 0x10,0x14,0x16,0x18,0x1a = 1, got: 0x10=%d 0x14=%d 0x16=%d 0x18=%d 0x1a=%d\n",
	       slot,
	       !!(val_after & BIT(0x10)),
	       !!(val_after & BIT(0x14)),
	       !!(val_after & BIT(0x16)),
	       !!(val_after & BIT(0x18)),
	       !!(val_after & BIT(0x1a)));

	if (ret)
		pr_err("PCIe%d: Failed to deassert resets: %d\n", slot, ret);

	return ret;
}

/**
 * rtd_pcie_phy_reset_assert() - Assert PCIe PHY resets
 * @regmap: regmap for CRT controller
 * @slot: PCIe slot number (1 or 2)
 *
 * Asserts PHY resets for the specified PCIe slot.
 */
static inline int rtd_pcie_phy_reset_assert(struct regmap *regmap, int slot)
{
	uint reg;
	int ret = 0;

	if (slot == 1)
		reg = CRT_SOFT_RESET4;
	else if (slot == 2)
		reg = CRT_SOFT_RESET9;
	else
		return -EINVAL;

	debug("PCIe%d PHY: Asserting resets (reg=0x%x)\n", slot, reg);

	ret |= rtd_rst_update_bits_we(regmap, reg,
				      slot == 1 ? RSTN_PCIE1_PHY_BIT : RSTN_PCIE2_PHY_BIT, true);
	ret |= rtd_rst_update_bits_we(regmap, reg,
				      slot == 1 ? RSTN_PCIE1_PHY_MDIO_BIT : RSTN_PCIE2_PHY_MDIO_BIT, true);

	if (ret)
		pr_err("PCIe%d PHY: Failed to assert resets: %d\n", slot, ret);

	return ret;
}

/**
 * rtd_pcie_phy_reset_deassert() - Deassert PCIe PHY resets
 * @regmap: regmap for CRT controller
 * @slot: PCIe slot number (1 or 2)
 *
 * Deasserts PHY resets for the specified PCIe slot.
 */
static inline int rtd_pcie_phy_reset_deassert(struct regmap *regmap, int slot)
{
	uint reg;
	int ret = 0;

	if (slot == 1)
		reg = CRT_SOFT_RESET4;
	else if (slot == 2)
		reg = CRT_SOFT_RESET9;
	else
		return -EINVAL;

	debug("PCIe%d PHY: Deasserting resets (reg=0x%x)\n", slot, reg);

	ret |= rtd_rst_update_bits_we(regmap, reg,
				      slot == 1 ? RSTN_PCIE1_PHY_BIT : RSTN_PCIE2_PHY_BIT, false);
	ret |= rtd_rst_update_bits_we(regmap, reg,
				      slot == 1 ? RSTN_PCIE1_PHY_MDIO_BIT : RSTN_PCIE2_PHY_MDIO_BIT, false);

	if (ret)
		pr_err("PCIe%d PHY: Failed to deassert resets: %d\n", slot, ret);

	return ret;
}

#endif /* __PCIE_RTD_CLK_RST_H__ */
