/* SPDX-License-Identifier: GPL-2.0+ */
/*
 * Realtek PCIe host controller driver header
 *
 * Copyright (C) 2024
 *
 * Ported from Linux kernel driver
 * Original: Copyright (c) 2017 Realtek Semiconductor Corp.
 */

#ifndef __PCIE_RTD_H__
#define __PCIE_RTD_H__

#include <pci.h>
#include <linux/bitops.h>

/*
 * Register Map Overview:
 * The ctrl_base contains both PCIe MAC registers (low offsets) and
 * wrapper/bridge registers (high offsets starting from 0xC00).
 * The cfg_base is used for configuration space access to downstream devices.
 */

/* PCIe MAC Registers (low offsets in ctrl_base) */
#define TYPE1_STATUS_COMMAND_REG		0x04
#define MEM_LIMIT_MEM_BASE_REG			0x20
#define PREF_MEM_LIMIT_PREF_MEM_BASE_REG	0x24
#define LINK_CONTROL_LINK_STATUS_REG		0x80
#define LINK_CONTROL2_LINK_STATUS2_REG		0xA0
#define PORT_LINK_CTRL_OFF			0x710

/* PCI-E Bridge Wrapper Registers (high offsets in ctrl_base) */
#define PCIE_SYS_CTR				0x00000C00
#define PCIE_INT_CTR				0x00000C04
#define PCIE_INDIR_CTR				0x00000C14
#define PCIE_MDIO_CTR				0x00000C1C
#define PCIE_CFG_CT				0x00000C38
#define PCIE_CFG_EN				0x00000C3C
#define PCIE_CFG_ST				0x00000C40
#define PCIE_CFG_ADDR				0x00000C44
#define PCIE_CFG_WDATA				0x00000C48
#define PCIE_CFG_RDATA				0x00000C4C
#define PCIE_DIR_EN				0x00000C78
#define PCIE_MAC_ST				0x00000CB4
#define PCIE_RCPL_ST				0x00000CBC
#define PCIE_SERVICE_REGION			0x00000D30
#define PCIE_DDR_START				0x00000D70
#define PCIE_DDR_END				0x00000D74
#define PCI_BASE_2				0x00000D5C

/* PCIE_SYS_CTR bits */
#define APP_LTSSM_EN				BIT(1)
#define APP_INIT_RST				BIT(16)

/* PCIE_MAC_ST bits */
#define LTSSM_STATE_MASK			GENMASK(9, 4)
#define LTSSM_STATE_L0				0x11
#define CFG_BUS_MASTER_EN			BIT(12)
#define RDLH_LINK_UP				BIT(14)

/* PCIE_CFG_CT bits */
#define GO_CT					BIT(0)

/* PCIE_CFG_ST bits */
#define CFG_ST_DONE				BIT(0)
#define CFG_ST_ERROR				BIT(1)

/* PCIE_CFG_EN bits */
#define BYTE_EN					BIT(20)
#define BYTE_CNT(x)				((x) << 16)
#define WRRD_EN(x)				((x) << 0)

/* PCIE_RCPL_ST error codes (bits [7:5]) */
#define RCPL_STATUS_UR				0x1	/* Unsupported Request - no device */
#define RCPL_STATUS_CRS				0x2	/* Config Request Retry - device not ready */
#define RCPL_STATUS_CA				0x4	/* Completer Abort - device rejected */

/* SCPU Wrapper PCIe MMIO Registers (accessed via scpu_wrapper syscon) */
#define PCIE1_START				0x630
#define PCIE1_END				0x634
#define PCIE1_CTRL				0x638
#define PCIE1_STAT				0x63C
#define PCIE2_START				0x640
#define PCIE2_END				0x644
#define PCIE2_CTRL				0x648
#define PCIE2_STAT				0x64C

/* Link wait timeout */
#define PCIE_CONNECT_TIMEOUT			2000

/* MISC_PHY_CTRL register (in main2_misc/m2tmx syscon) */
#define MISC_PHY_CTRL				0x50
#define PCIE1_SATA_SEL_OFFSET			BIT(8)
#define PCIE2_SATA_SEL_OFFSET			BIT(9)

struct rtd_pcie {
	struct udevice *dev;
	void __iomem *ctrl_base;	/* Controller base (MAC + wrapper registers) */
	void __iomem *cfg_base;		/* Configuration space for downstream devices */
	struct regmap *crt;		/* CRT clock/reset controller */
	struct regmap *scpu_wrapper;	/* SCPU wrapper syscon */
	struct regmap *main2_misc;	/* Main2/M2TMX misc syscon (for PHY mux control) */
	struct phy phy;			/* PCIe PHY */
	struct gpio_desc perst_gpio;	/* PERST# GPIO */
	int slot;			/* PCIe slot number (1 or 2) */
	int first_busno;		/* First bus number */
};

#endif /* __PCIE_RTD_H__ */
