// SPDX-License-Identifier: GPL-2.0+
/*
 * Realtek PCIe host controller driver for U-Boot
 *
 * Copyright (C) 2024
 *
 * Ported from Linux kernel driver
 * Original: Copyright (c) 2017 Realtek Semiconductor Corp.
 */

#include <common.h>
#include <dm.h>
#include <pci.h>
#include <asm/io.h>
#include <dm/device_compat.h>
#include <generic-phy.h>
#include <regmap.h>
#include <syscon.h>
#include <linux/delay.h>
#include <linux/ioport.h>
#include <asm-generic/gpio.h>

#include "pcie_rtd.h"
#include "pcie_rtd_clk_rst.h"


/* Forward declarations */
static int indirect_cfg_read(struct rtd_pcie *pcie, pci_dev_t bdf, u32 addr,
			     u32 *pdata, enum pci_size_t size);

/**
 * rtd_pcie_hw_init() - Hardware-specific initialization
 * @pcie: Pointer to the PCIe port structure
 *
 * Configures SCPU wrapper and sets up memory regions.
 *
 * Return: 0 on success, negative error code on failure
 */
static int rtd_pcie_hw_init(struct rtd_pcie *pcie)
{
	u32 mmio_start, mmio_end, mmio_size;
	u32 regmap_st = 0;
	int timeout;
	int ret;
	struct pci_region *mem_region = NULL;
	struct udevice *ctlr = pci_get_controller(pcie->dev);
	struct pci_controller *hose = dev_get_uclass_priv(ctlr);
	int i;

	debug("PCIe%d: Hardware initialization\n", pcie->slot);

	/* Find memory region from PCI controller */
	for (i = 0; i < hose->region_count; i++) {
		if (hose->regions[i].flags == PCI_REGION_MEM) {
			mem_region = &hose->regions[i];
			break;
		}
	}

	if (!mem_region) {
		dev_err(pcie->dev, "No memory region found\n");
		return -EINVAL;
	}

	mmio_start = mem_region->phys_start;
	mmio_size = mem_region->size;
	mmio_end = mmio_start + mmio_size - 1;

	debug("PCIe%d: MMIO region 0x%08x - 0x%08x\n",
	      pcie->slot, mmio_start, mmio_end);

	/* Configure SCPU wrapper for CPU→Device (outbound) MMIO access */
	if (pcie->slot == 1) {
		regmap_write(pcie->scpu_wrapper, PCIE1_START, mmio_start);
		regmap_write(pcie->scpu_wrapper, PCIE1_END, mmio_end);
		regmap_write(pcie->scpu_wrapper, PCIE1_CTRL, 0x1);

		timeout = 500;
		while (regmap_st != 0x1 && timeout > 0) {
			regmap_read(pcie->scpu_wrapper, PCIE1_STAT, &regmap_st);
			mdelay(1);
			timeout--;
		}
	} else if (pcie->slot == 2) {
		regmap_write(pcie->scpu_wrapper, PCIE2_START, mmio_start);
		regmap_write(pcie->scpu_wrapper, PCIE2_END, mmio_end);
		regmap_write(pcie->scpu_wrapper, PCIE2_CTRL, 0x1);

		timeout = 500;
		while (regmap_st != 0x1 && timeout > 0) {
			regmap_read(pcie->scpu_wrapper, PCIE2_STAT, &regmap_st);
			mdelay(1);
			timeout--;
		}
	}

	if (timeout == 0) {
		dev_err(pcie->dev, "Failed to set MMIO start/end\n");
		return -ETIMEDOUT;
	}

	/* Configure wrapper registers */
	/* Keep PCIE_DDR_START/END at MMIO range (same as Linux) */
	writel(mmio_start, pcie->ctrl_base + PCIE_DDR_START);
	writel(mmio_end, pcie->ctrl_base + PCIE_DDR_END);

	debug("PCIe%d: PCIE_DDR_START=0x%x, PCIE_DDR_END=0x%x\n",
	      pcie->slot, mmio_start, mmio_end);

	/* Disable SERVICE_REGION (set to 0) - no address translation needed */
	writel(0x0, pcie->ctrl_base + PCIE_SERVICE_REGION);
	debug("PCIe%d: PCIE_SERVICE_REGION=0x%x\n",
	      pcie->slot, readl(pcie->ctrl_base + PCIE_SERVICE_REGION));

	ret = readl(pcie->ctrl_base + PCIE_SYS_CTR);
	writel(ret | 0x24000000, pcie->ctrl_base + PCIE_SYS_CTR);

	writel(mmio_start, pcie->ctrl_base + PCI_BASE_2);

	debug("PCIe%d: Hardware initialization complete\n", pcie->slot);

	return 0;
}

static u32 get_pcie_mac_stat(struct rtd_pcie *pcie)
{
	int timeout = 10000;
	u32 mac_stat, mac_stat_tmp;

	mac_stat_tmp = readl(pcie->ctrl_base + PCIE_MAC_ST);
	while (timeout) {
		mac_stat = readl(pcie->ctrl_base + PCIE_MAC_ST);
		if (mac_stat == mac_stat_tmp)
			return mac_stat;
		else
			mac_stat_tmp = mac_stat;
		timeout--;
	}

	return 0;
}

/**
 * rtd_pcie_link_init() - Initialize PCIe link
 * @pcie: Pointer to the PCIe port structure
 *
 * Performs PHY initialization and link training.
 *
 * Return: 0 on success, negative error code on failure
 */
static int rtd_pcie_link_init(struct rtd_pcie *pcie)
{
	int timeout;
	u32 val;
	int cur_link_speed;
	int ret;
	u32 ltssm;
	u32 mac_stat;

	debug("PCIe%d: Link initialization start\n", pcie->slot);

	/* Assert PERST# */
	ret = dm_gpio_set_value(&pcie->perst_gpio, 0);
	if (ret) {
		dev_err(pcie->dev, "Failed to assert PERST#\n");
		return ret;
	}

	/* Initialize controller */
	writel(0x00140010, pcie->ctrl_base + PCIE_SYS_CTR);

	/* Verify controller register is accessible and MDIO is enabled */
	{
		u32 sys_ctr = readl(pcie->ctrl_base + PCIE_SYS_CTR);
		debug("PCIe%d: PCIE_SYS_CTR after init = 0x%08x (should be 0x00140010)\n",
		       pcie->slot, sys_ctr);
	}

	/* Initialize PHY */
	debug("PCIe%d: Initializing PHY\n", pcie->slot);
	ret = generic_phy_init(&pcie->phy);
	if (ret) {
		dev_err(pcie->dev, "Failed to initialize PHY: %d\n", ret);
		return ret;
	}

	mdelay(100);

	/* Deassert PERST# */
	debug("PCIe%d: Deasserting PERST#\n", pcie->slot);
	ret = dm_gpio_set_value(&pcie->perst_gpio, 1);
	if (ret) {
		dev_err(pcie->dev, "Failed to deassert PERST#\n");
		return ret;
	}
	debug("PCIe%d: PERST#=0x%x\n", pcie->slot, dm_gpio_get_value(&pcie->perst_gpio));

	/* Enable link training */
	writel(0x001E0022, pcie->ctrl_base + PCIE_SYS_CTR);
	writel(0x00010120, pcie->ctrl_base + PORT_LINK_CTRL_OFF);

	/* Wait for link up */
	debug("PCIe%d: Waiting for link up...\n", pcie->slot);

	timeout = PCIE_CONNECT_TIMEOUT;
	while (timeout > 0) {
		mac_stat = get_pcie_mac_stat(pcie);
		if (mac_stat & BIT(11)) {
			ltssm = (mac_stat & GENMASK(9, 4)) >> 4;
			if (ltssm == 0x11) {
				mdelay(1);
				mac_stat = get_pcie_mac_stat(pcie);
				ltssm = (mac_stat & GENMASK(9, 4)) >> 4;
				if (ltssm == 0x11)
					break;
			}
		}
		udelay(50);
		timeout--;
	}

	if (timeout == 0) {
		dev_err(pcie->dev, "Link down - no device connected, mac_stat=0x%x, ltssm=0x%x\n", mac_stat, ltssm);
		return -ENODEV;
	}

	dev_info(pcie->dev, "Link up!\n");

	/* Read and display link speed */
	val = readl(pcie->ctrl_base + LINK_CONTROL_LINK_STATUS_REG);
	cur_link_speed = (val & GENMASK(19, 16)) >> 16;

	switch (cur_link_speed) {
	case 0x1:
		dev_info(pcie->dev, "Link speed: Gen1 (2.5 GT/s)\n");
		break;
	case 0x2:
		dev_info(pcie->dev, "Link speed: Gen2 (5.0 GT/s)\n");
		break;
	case 0x3:
		dev_info(pcie->dev, "Link speed: Gen3 (8.0 GT/s)\n");
		break;
	default:
		dev_info(pcie->dev, "Link speed: Unknown (0x%x)\n", cur_link_speed);
		break;
	}

	/* Run PHY calibration */
	debug("PCIe%d: Running PHY calibration\n", pcie->slot);
	ret = generic_phy_configure(&pcie->phy, NULL);
	if (ret)
		dev_warn(pcie->dev, "PHY calibration failed: %d\n", ret);

	/* Enable bus mastering and memory access */
	writel(0x7, pcie->ctrl_base + TYPE1_STATUS_COMMAND_REG);

	/* Run hardware initialization */
	ret = rtd_pcie_hw_init(pcie);
	if (ret) {
		dev_err(pcie->dev, "Hardware init failed: %d\n", ret);
		return ret;
	}

	/* Set memory limit and base registers */
	writel(0x0000FFF0, pcie->ctrl_base + MEM_LIMIT_MEM_BASE_REG);
	writel(0x0000FFF0, pcie->ctrl_base + PREF_MEM_LIMIT_PREF_MEM_BASE_REG);

	/*
	 * Note: This controller is NOT a traditional PCI-to-PCI bridge!
	 * The endpoint device appears at bus 0, device 0 via indirect config.
	 * No bridge configuration needed.
	 */

	debug("PCIe%d: Link initialization complete\n", pcie->slot);

	return 0;
}

/**
 * pci_address_conversion() - Convert BDF to PCIe address format
 */
static u32 pci_address_conversion(pci_dev_t bdf, uint offset)
{
	int busno = PCI_BUS(bdf);
	int dev = PCI_DEV(bdf);
	int func = PCI_FUNC(bdf);

	return (busno << 24) | (dev << 19) | (func << 16) | (offset & 0xFFFF);
}

/**
 * pci_byte_mask() - Calculate byte mask for config access
 */
static u8 pci_byte_mask(u32 addr, enum pci_size_t size)
{
	int shift = addr & 3;

	if (size == PCI_SIZE_8)
		return 1 << shift;
	else if (size == PCI_SIZE_16)
		return 3 << shift;
	else if (size == PCI_SIZE_32)
		return 0xF;

	return 0;
}

/**
 * indirect_cfg_read() - Perform indirect configuration read
 */
static int indirect_cfg_read(struct rtd_pcie *pcie, pci_dev_t bdf, u32 addr,
			     u32 *pdata, enum pci_size_t size)
{
	int busno = PCI_BUS(bdf);
	u32 status, rcpl_st;
	u8 mask;
	int try_count = 1000;
	int shift = (addr & 3) * 8;

	/* Check for valid device on root bus */
	if (busno == pcie->first_busno && PCI_DEV(bdf) != 0)
		return -EINVAL;

	mask = pci_byte_mask(addr, size);
	if (!mask)
		return -EINVAL;

	/* Only debug first device on bus 1 to reduce spam */
	bool verbose = (busno == 1 && PCI_DEV(bdf) == 0);

	/* Set indirect control - 0x10 for root bus, 0x14 for downstream */
	if (busno == pcie->first_busno)
		writel(0x10, pcie->ctrl_base + PCIE_INDIR_CTR);
	else
		writel(0x14, pcie->ctrl_base + PCIE_INDIR_CTR);

	/* Clear status */
	writel(CFG_ST_ERROR | CFG_ST_DONE, pcie->ctrl_base + PCIE_CFG_ST);

	/* Set address */
	writel(addr & ~0x3, pcie->ctrl_base + PCIE_CFG_ADDR);

	/* Set byte enable and trigger read */
	writel(BYTE_CNT(mask) | BYTE_EN | WRRD_EN(0), pcie->ctrl_base + PCIE_CFG_EN);

	if (verbose) {
		debug("  INDIR_CTR=0x%x, CFG_ADDR=0x%x, CFG_EN=0x%x\n",
		      readl(pcie->ctrl_base + PCIE_INDIR_CTR),
		      readl(pcie->ctrl_base + PCIE_CFG_ADDR),
		      readl(pcie->ctrl_base + PCIE_CFG_EN));
	}

	writel(GO_CT, pcie->ctrl_base + PCIE_CFG_CT);

	/* Wait for completion */
	do {
		status = readl(pcie->ctrl_base + PCIE_CFG_ST);
		udelay(50);
	} while (!(status & CFG_ST_DONE) && try_count--);

	if (try_count < 0) {
		debug("PCIe: Config read timeout (addr=0x%x)\n", addr);
		goto error;
	}

	/* Check completion status */
	rcpl_st = readl(pcie->ctrl_base + PCIE_RCPL_ST);

	if (verbose) {
		debug("  CFG_ST=0x%x, RCPL_ST=0x%x, tries_left=%d\n",
		      status, rcpl_st, try_count);
	}

	if (status & CFG_ST_ERROR) {
		debug("PCIe: Config read error (addr=0x%x, CFG_ST=0x%x)\n", addr, status);
		goto error;
	}

	/* Check for completion errors (Unsupported Request, etc.) */
	if ((rcpl_st & 0xE0) != 0) {
		u8 err_st = (rcpl_st >> 5) & 0x7;
		if (verbose) {
			debug("PCIe: Completion error: 0x%x (UR/CRS/CA)\n", err_st);
		}
		/* This is expected when no device present - return 0xFFFFFFFF */
		*pdata = 0xFFFFFFFF;
		writel(CFG_ST_ERROR | CFG_ST_DONE, pcie->ctrl_base + PCIE_CFG_ST);
		return 0;
	}

	/* Clear status */
	writel(CFG_ST_ERROR | CFG_ST_DONE, pcie->ctrl_base + PCIE_CFG_ST);

	/* Read data and apply mask */
	*pdata = readl(pcie->ctrl_base + PCIE_CFG_RDATA);

	if (verbose) {
		debug("  CFG_RDATA=0x%x (before shift/mask)\n", *pdata);
	}

	*pdata = (*pdata >> shift) & pci_get_ff(size);

	return 0;

error:
	writel(CFG_ST_ERROR | CFG_ST_DONE, pcie->ctrl_base + PCIE_CFG_ST);
	return -EIO;
}

/**
 * rtd_pcie_read_config() - Read from PCI configuration space
 */
static int rtd_pcie_read_config(const struct udevice *bus, pci_dev_t bdf,
				uint offset, ulong *valuep,
				enum pci_size_t size)
{
	struct rtd_pcie *pcie = dev_get_priv(bus);
	int busno = PCI_BUS(bdf);
	int devno = PCI_DEV(bdf);
	int funcno = PCI_FUNC(bdf);
	u32 address;
	u32 value;
	int ret;

	/* Only bus 0, device 0, function 0 exists */
	if (busno != pcie->first_busno || devno != 0 || funcno != 0) {
		*valuep = pci_get_ff(size);
		return 0;
	}

	/*
	 * Realtek PCIe controller quirk:
	 * - The endpoint device appears at bus 0, device 0 (not bus 1)
	 * - Direct read = root complex config (vendor 0x20ec, device 0x6698)
	 * - Indirect read with INDIR_CTR=0x10 = actual endpoint device
	 */
	address = pci_address_conversion(bdf, offset);
	ret = indirect_cfg_read(pcie, bdf, address, &value, size);
	if (ret) {
		*valuep = pci_get_ff(size);
		return 0;
	}

	*valuep = value;
	return 0;
}

/**
 * indirect_cfg_write() - Perform indirect configuration write
 */
static int indirect_cfg_write(struct rtd_pcie *pcie, pci_dev_t bdf, u32 addr,
			      u32 data, enum pci_size_t size)
{
	int busno = PCI_BUS(bdf);
	u32 status;
	u8 mask;
	int try_count = 1000;
	int shift = (addr & 3) * 8;

	/* Check for valid device on root bus */
	if (busno == pcie->first_busno && PCI_DEV(bdf) != 0)
		return -EINVAL;

	mask = pci_byte_mask(addr, size);
	if (!mask)
		return -EINVAL;

	/* Prepare data with proper shift */
	data = (data & pci_get_ff(size)) << shift;

	/* Set indirect control - 0x12 for root bus, 0x16 for downstream */
	if (busno == pcie->first_busno)
		writel(0x12, pcie->ctrl_base + PCIE_INDIR_CTR);
	else
		writel(0x16, pcie->ctrl_base + PCIE_INDIR_CTR);

	/* Clear status */
	writel(CFG_ST_ERROR | CFG_ST_DONE, pcie->ctrl_base + PCIE_CFG_ST);

	/* Set address and data */
	writel(addr & ~0x3, pcie->ctrl_base + PCIE_CFG_ADDR);
	writel(data, pcie->ctrl_base + PCIE_CFG_WDATA);

	/* Set byte enable and trigger write */
	if (size == PCI_SIZE_32)
		writel(0x1, pcie->ctrl_base + PCIE_CFG_EN);
	else
		writel(BYTE_CNT(mask) | BYTE_EN | WRRD_EN(1), pcie->ctrl_base + PCIE_CFG_EN);

	writel(GO_CT, pcie->ctrl_base + PCIE_CFG_CT);

	/* Wait for completion */
	do {
		status = readl(pcie->ctrl_base + PCIE_CFG_ST);
		udelay(50);
	} while (!(status & CFG_ST_DONE) && try_count--);

	if (try_count < 0) {
		debug("PCIe: Config write timeout (addr=0x%x)\n", addr);
		goto error;
	}

	if (status & CFG_ST_ERROR) {
		debug("PCIe: Config write error (addr=0x%x)\n", addr);
		goto error;
	}

	/* Clear status */
	writel(CFG_ST_ERROR | CFG_ST_DONE, pcie->ctrl_base + PCIE_CFG_ST);

	return 0;

error:
	writel(CFG_ST_ERROR | CFG_ST_DONE, pcie->ctrl_base + PCIE_CFG_ST);
	return -EIO;
}

/**
 * rtd_pcie_write_config() - Write to PCI configuration space
 */
static int rtd_pcie_write_config(struct udevice *bus, pci_dev_t bdf,
				 uint offset, ulong value,
				 enum pci_size_t size)
{
	struct rtd_pcie *pcie = dev_get_priv(bus);
	int busno = PCI_BUS(bdf);
	int devno = PCI_DEV(bdf);
	int funcno = PCI_FUNC(bdf);
	u32 address;

	/* Only bus 0, device 0, function 0 exists */
	if (busno != pcie->first_busno || devno != 0 || funcno != 0)
		return 0;

	/* Use indirect config to write to endpoint device */
	address = pci_address_conversion(bdf, offset);
	indirect_cfg_write(pcie, bdf, address, value, size);

	/* Ensure config writes are visible before returning */
	mb();

	return 0;
}

/**
 * rtd_pcie_probe() - Probe PCIe controller
 */
static int rtd_pcie_probe(struct udevice *dev)
{
	struct rtd_pcie *pcie = dev_get_priv(dev);
	struct udevice *syscon_dev;
	fdt_addr_t addr;
	fdt_size_t size;
	int ret;

	pcie->dev = dev;
	pcie->first_busno = dev_seq(dev);

	debug("PCIe controller probe start\n");

	/* Determine slot number from compatible string */
	if (dev_read_bool(dev, "realtek,rtd16xxb-pcie-slot1")) {
		pcie->slot = 1;
	} else if (dev_read_bool(dev, "realtek,rtd16xxb-pcie-slot2")) {
		pcie->slot = 2;
	} else {
		const char *compatible = dev_read_string(dev, "compatible");
		if (strstr(compatible, "slot1"))
			pcie->slot = 1;
		else if (strstr(compatible, "slot2"))
			pcie->slot = 2;
		else {
			dev_err(dev, "Cannot determine PCIe slot number\n");
			return -EINVAL;
		}
	}

	dev_info(dev, "Realtek RTD1619B PCIe Slot %d controller\n", pcie->slot);

	/* Map controller registers */
	addr = dev_read_addr_size_index(dev, 0, &size);
	if (addr == FDT_ADDR_T_NONE) {
		dev_err(dev, "Failed to get controller base address\n");
		return -EINVAL;
	}
	pcie->ctrl_base = (void __iomem *)addr;
	debug("PCIe%d: ctrl_base = 0x%p\n", pcie->slot, pcie->ctrl_base);

	/* Map configuration space */
	addr = dev_read_addr_size_index(dev, 1, &size);
	if (addr == FDT_ADDR_T_NONE) {
		dev_err(dev, "Failed to get cfg base address\n");
		return -EINVAL;
	}
	pcie->cfg_base = (void __iomem *)addr;
	debug("PCIe%d: cfg_base = 0x%p\n", pcie->slot, pcie->cfg_base);

	/* Get SCPU wrapper syscon */
	ret = uclass_get_device_by_phandle(UCLASS_SYSCON, dev,
					   "syscon-scpu-wrapper", &syscon_dev);
	if (ret) {
		dev_err(dev, "Failed to get SCPU wrapper syscon: %d\n", ret);
		return ret;
	}
	pcie->scpu_wrapper = syscon_get_regmap(syscon_dev);
	if (!pcie->scpu_wrapper) {
		dev_err(dev, "Failed to get SCPU wrapper regmap\n");
		return -EINVAL;
	}

	/* Get main2_misc (m2tmx) syscon for PHY mux control */
	ret = uclass_get_device_by_phandle(UCLASS_SYSCON, dev,
					   "syscon", &syscon_dev);
	if (ret) {
		dev_err(dev, "Failed to get main2_misc syscon: %d\n", ret);
		return ret;
	}
	pcie->main2_misc = syscon_get_regmap(syscon_dev);
	if (!pcie->main2_misc) {
		dev_err(dev, "Failed to get main2_misc regmap\n");
		return -EINVAL;
	}

	/* Create regmap for CRT clock/reset controller (0x98000000, 0x1000) */
	ret = regmap_init_mem_range(dev_ofnode(dev), 0x98000000, 0x1000,
				     &pcie->crt);
	if (ret) {
		dev_err(dev, "Failed to create CRT regmap: %d\n", ret);
		return ret;
	}

	/* Configure PHY mux: select PCIe mode (not SATA) */
	{
		u32 tmp;
		regmap_read(pcie->main2_misc, MISC_PHY_CTRL, &tmp);
		if (pcie->slot == 1) {
			debug("PCIe1: Configuring PHY mux (PCIe mode, not SATA)\n");
			tmp &= ~PCIE1_SATA_SEL_OFFSET;
		} else if (pcie->slot == 2) {
			debug("PCIe2: Configuring PHY mux (PCIe mode, not SATA)\n");
			tmp &= ~PCIE2_SATA_SEL_OFFSET;
		}
		regmap_write(pcie->main2_misc, MISC_PHY_CTRL, tmp);
	}

	/* Get PERST GPIO */
	ret = gpio_request_by_name(dev, "perst-gpios", 0, &pcie->perst_gpio,
				   GPIOD_IS_OUT);
	if (ret) {
		dev_err(dev, "Failed to get PERST GPIO: %d\n", ret);
		return ret;
	}

	/* Get PHY */
	ret = generic_phy_get_by_index(dev, 0, &pcie->phy);
	if (ret) {
		dev_err(dev, "Failed to get PHY: %d\n", ret);
		return ret;
	}

	/* Deassert all resets (as per Linux driver) */
	debug("PCIe%d: Deasserting resets\n", pcie->slot);
	ret = rtd_pcie_reset_deassert(pcie->crt, pcie->slot);
	if (ret) {
		dev_err(dev, "Failed to deassert PCIe resets: %d\n", ret);
		return ret;
	}
	ret = rtd_pcie_phy_reset_deassert(pcie->crt, pcie->slot);
	if (ret) {
		dev_err(dev, "Failed to deassert PHY resets: %d\n", ret);
		return ret;
	}

	/* Power on PHY */
	debug("PCIe%d: Powering on PHY\n", pcie->slot);
	ret = generic_phy_power_on(&pcie->phy);
	if (ret) {
		dev_err(dev, "Failed to power on PHY: %d\n", ret);
		return ret;
	}

	/* Enable clocks (after PHY power on) */
	debug("PCIe%d: Enabling clocks\n", pcie->slot);
	ret = rtd_pcie_clk_enable(pcie->crt, pcie->slot);
	if (ret) {
		dev_err(dev, "Failed to enable clocks: %d\n", ret);
		return ret;
	}

	/* Verify clock is actually enabled */
	{
		u32 clk_val;
		uint clk_reg = (pcie->slot == 1) ? 0x5c : 0x8c;
		uint clk_bit = (pcie->slot == 1) ? 18 : 0;
		regmap_read(pcie->crt, clk_reg, &clk_val);
		debug("PCIe%d: Clock register 0x%x = 0x%08x (bit %d = %d)\n",
		       pcie->slot, clk_reg, clk_val, clk_bit,
		       !!(clk_val & BIT(clk_bit)));
	}

	/* Initialize link */
	ret = rtd_pcie_link_init(pcie);
	if (ret) {
		dev_err(dev, "Link initialization failed: %d\n", ret);
		return ret;
	}

	dev_info(dev, "PCIe%d initialization complete\n", pcie->slot);

	return 0;
}

static const struct dm_pci_ops rtd_pcie_ops = {
	.read_config	= rtd_pcie_read_config,
	.write_config	= rtd_pcie_write_config,
};

static const struct udevice_id rtd_pcie_ids[] = {
	{ .compatible = "realtek,rtd16xxb-pcie-slot1" },
	{ .compatible = "realtek,rtd16xxb-pcie-slot2" },
	{ }
};

U_BOOT_DRIVER(rtd_pcie) = {
	.name		= "rtd_pcie",
	.id		= UCLASS_PCI,
	.of_match	= rtd_pcie_ids,
	.probe		= rtd_pcie_probe,
	.ops		= &rtd_pcie_ops,
	.priv_auto	= sizeof(struct rtd_pcie),
};
