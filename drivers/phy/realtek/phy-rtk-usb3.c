// SPDX-License-Identifier: GPL-2.0
/*
 *  phy-rtk-usb3.c RTK usb3.0 phy driver
 *
 * copyright (c) 2023 realtek semiconductor corporation
 *
 */

#include <common.h>
#include <dm.h>
#include <log.h>
#include <dm/device.h>
#include <dm/device_compat.h>
//#include <dm/of_access.h>
#include <generic-phy.h>
#include <asm/io.h>
#include <linux/bitops.h>
#include <linux/delay.h>
#include <linux/compat.h>
#include <linux/iopoll.h>
#include <linux/ioport.h>

#define USB_MDIO_CTRL_PHY_BUSY BIT(7)
#define USB_MDIO_CTRL_PHY_WRITE BIT(0)
#define USB_MDIO_CTRL_PHY_ADDR_SHIFT 8
#define USB_MDIO_CTRL_PHY_DATA_SHIFT 16

#define MAX_USB_PHY_DATA_SIZE 0x30
#define PHY_ADDR_0X09 0x09
#define PHY_ADDR_0X0B 0x0b
#define PHY_ADDR_0X0D 0x0d
#define PHY_ADDR_0X10 0x10
#define PHY_ADDR_0X1F 0x1f
#define PHY_ADDR_0X20 0x20
#define PHY_ADDR_0X21 0x21
#define PHY_ADDR_0X30 0x30

#define REG_0X09_FORCE_CALIBRATION BIT(9)
#define REG_0X0B_RX_OFFSET_RANGE_MASK 0xc
#define REG_0X0D_RX_DEBUG_TEST_EN BIT(6)
#define REG_0X10_DEBUG_MODE_SETTING 0x3c0
#define REG_0X10_DEBUG_MODE_SETTING_MASK 0x3f8
#define REG_0X1F_RX_OFFSET_CODE_MASK 0x1e

#define USB_U3_TX_LFPS_SWING_TRIM_SHIFT 4
#define USB_U3_TX_LFPS_SWING_TRIM_MASK 0xf
#define AMPLITUDE_CONTROL_COARSE_MASK 0xff
#define AMPLITUDE_CONTROL_FINE_MASK 0xffff
#define AMPLITUDE_CONTROL_COARSE_DEFAULT 0xff
#define AMPLITUDE_CONTROL_FINE_DEFAULT 0xffff

#define PHY_ADDR_MAP_ARRAY_INDEX(addr) (addr)
#define ARRAY_INDEX_MAP_PHY_ADDR(index) (index)

struct phy_reg {
	void __iomem *reg_mdio_ctl;
};

struct phy_data {
	u8 addr;
	u16 data;
};

struct phy_cfg {
	int param_size;
	struct phy_data param[MAX_USB_PHY_DATA_SIZE];

	bool check_efuse;
	bool do_toggle;
	bool do_toggle_once;
	bool use_default_parameter;
	bool check_rx_front_end_offset;
};

struct phy_parameter {
	struct phy_reg phy_reg;

	/* Get from efuse */
	u8 efuse_usb_u3_tx_lfps_swing_trim;

	/* Get from dts */
	u32 amplitude_control_coarse;
	u32 amplitude_control_fine;
};

struct rtk_phy {
	struct udevice *dev;

	struct phy_cfg *phy_cfg;
	int num_phy;
	struct phy_parameter *phy_parameter;

	struct dentry *debug_dir;
};

#define PHY_IO_TIMEOUT_USEC		(50000)
#define PHY_IO_DELAY_US			(100)

static inline int utmi_wait_register(void __iomem *reg, u32 mask, u32 result)
{
	int ret;
	unsigned int val;

	ret = read_poll_timeout(readl, val, ((val & mask) == result),
				PHY_IO_DELAY_US, PHY_IO_TIMEOUT_USEC, reg);
	if (ret) {
		pr_err("%s can't program USB phy\n", __func__);
		return -ETIMEDOUT;
	}

	return 0;
}

static int rtk_phy3_wait_vbusy(struct phy_reg *phy_reg)
{
	return utmi_wait_register(phy_reg->reg_mdio_ctl, USB_MDIO_CTRL_PHY_BUSY, 0);
}

static u16 rtk_phy_read(struct phy_reg *phy_reg, char addr)
{
	unsigned int tmp;
	u32 value;

	tmp = (addr << USB_MDIO_CTRL_PHY_ADDR_SHIFT);

	writel(tmp, phy_reg->reg_mdio_ctl);

	rtk_phy3_wait_vbusy(phy_reg);

	value = readl(phy_reg->reg_mdio_ctl);
	value = value >> USB_MDIO_CTRL_PHY_DATA_SHIFT;

	return (u16)value;
}

static int rtk_phy_write(struct phy_reg *phy_reg, char addr, u16 data)
{
	unsigned int val;

	val = USB_MDIO_CTRL_PHY_WRITE |
		    (addr << USB_MDIO_CTRL_PHY_ADDR_SHIFT) |
		    (data << USB_MDIO_CTRL_PHY_DATA_SHIFT);

	writel(val, phy_reg->reg_mdio_ctl);

	rtk_phy3_wait_vbusy(phy_reg);

	return 0;
}

static void do_rtk_usb3_phy_toggle(struct rtk_phy *rtk_phy, int index, bool connect)
{
	struct phy_cfg *phy_cfg = rtk_phy->phy_cfg;
	struct phy_reg *phy_reg;
	struct phy_parameter *phy_parameter;
	struct phy_data *phy_data;
	u8 addr;
	u16 data;
	int i;

	phy_parameter = &((struct phy_parameter *)rtk_phy->phy_parameter)[index];
	phy_reg = &phy_parameter->phy_reg;

	if (!phy_cfg->do_toggle)
		return;

	i = PHY_ADDR_MAP_ARRAY_INDEX(PHY_ADDR_0X09);
	phy_data = phy_cfg->param + i;
	addr = phy_data->addr;
	data = phy_data->data;

	if (!addr && !data) {
		addr = PHY_ADDR_0X09;
		data = rtk_phy_read(phy_reg, addr);
		phy_data->addr = addr;
		phy_data->data = data;
	}

	rtk_phy_write(phy_reg, addr, data & (~REG_0X09_FORCE_CALIBRATION));
	mdelay(1);
	rtk_phy_write(phy_reg, addr, data | REG_0X09_FORCE_CALIBRATION);
}

static int do_rtk_phy_init(struct rtk_phy *rtk_phy, int index)
{
	struct phy_cfg *phy_cfg;
	struct phy_reg *phy_reg;
	struct phy_parameter *phy_parameter;
	int i = 0;

	phy_cfg = rtk_phy->phy_cfg;
	phy_parameter = &((struct phy_parameter *)rtk_phy->phy_parameter)[index];
	phy_reg = &phy_parameter->phy_reg;

	if (phy_cfg->use_default_parameter)
		goto do_toggle;

	for (i = 0; i < phy_cfg->param_size; i++) {
		struct phy_data *phy_data = phy_cfg->param + i;
		u8 addr = phy_data->addr;
		u16 data = phy_data->data;

		if (!addr && !data)
			continue;

		rtk_phy_write(phy_reg, addr, data);
	}

do_toggle:
	if (phy_cfg->do_toggle_once)
		phy_cfg->do_toggle = true;

	do_rtk_usb3_phy_toggle(rtk_phy, index, false);

	if (phy_cfg->do_toggle_once) {
		u16 check_value = 0;
		int count = 10;
		u16 value_0x0d, value_0x10;

		/* Enable Debug mode by set 0x0D and 0x10 */
		value_0x0d = rtk_phy_read(phy_reg, PHY_ADDR_0X0D);
		value_0x10 = rtk_phy_read(phy_reg, PHY_ADDR_0X10);

		rtk_phy_write(phy_reg, PHY_ADDR_0X0D,
			      value_0x0d | REG_0X0D_RX_DEBUG_TEST_EN);
		rtk_phy_write(phy_reg, PHY_ADDR_0X10,
			      (value_0x10 & ~REG_0X10_DEBUG_MODE_SETTING_MASK) |
			      REG_0X10_DEBUG_MODE_SETTING);

		check_value = rtk_phy_read(phy_reg, PHY_ADDR_0X30);

		while (!(check_value & BIT(15))) {
			check_value = rtk_phy_read(phy_reg, PHY_ADDR_0X30);
			mdelay(1);
			if (count-- < 0)
				break;
		}

		if (!(check_value & BIT(15)))
			dev_info(rtk_phy->dev, "toggle fail addr=0x%02x, data=0x%04x\n",
				 PHY_ADDR_0X30, check_value);

		/* Disable Debug mode by set 0x0D and 0x10 to default*/
		rtk_phy_write(phy_reg, PHY_ADDR_0X0D, value_0x0d);
		rtk_phy_write(phy_reg, PHY_ADDR_0X10, value_0x10);

		phy_cfg->do_toggle = false;
	}

	if (phy_cfg->check_rx_front_end_offset) {
		u16 rx_offset_code, rx_offset_range;
		u16 code_mask = REG_0X1F_RX_OFFSET_CODE_MASK;
		u16 range_mask = REG_0X0B_RX_OFFSET_RANGE_MASK;
		bool do_update = false;

		rx_offset_code = rtk_phy_read(phy_reg, PHY_ADDR_0X1F);
		if (((rx_offset_code & code_mask) == 0x0) ||
		    ((rx_offset_code & code_mask) == code_mask))
			do_update = true;

		rx_offset_range = rtk_phy_read(phy_reg, PHY_ADDR_0X0B);
		if (((rx_offset_range & range_mask) == range_mask) && do_update) {
			dev_warn(rtk_phy->dev, "Don't update rx_offset_range (rx_offset_code=0x%x, rx_offset_range=0x%x)\n",
				 rx_offset_code, rx_offset_range);
			do_update = false;
		}

		if (do_update) {
			u16 tmp1, tmp2;

			tmp1 = rx_offset_range & (~range_mask);
			tmp2 = rx_offset_range & range_mask;
			tmp2 += (1 << 2);
			rx_offset_range = tmp1 | (tmp2 & range_mask);
			rtk_phy_write(phy_reg, PHY_ADDR_0X0B, rx_offset_range);
			goto do_toggle;
		}
	}

	return 0;
}

static int rtk_phy_init(struct phy *phy)
{
	struct rtk_phy *rtk_phy = dev_get_priv(phy->dev);
	int ret = 0;
	int i;

	for (i = 0; i < rtk_phy->num_phy; i++)
		ret = do_rtk_phy_init(rtk_phy, i);

	dev_dbg(rtk_phy->dev, "Initialized RTK USB 3.0 PHY\n");

	return ret;
}

static int rtk_phy_exit(struct phy *phy)
{
	return 0;
}

static const struct phy_ops ops = {
	.init		= rtk_phy_init,
	.exit		= rtk_phy_exit,
};

#if 0
static void rtk_phy_toggle(struct usb_phy *usb3_phy, bool connect, int port)
{
	int index = port;
	struct rtk_phy *rtk_phy = NULL;

	rtk_phy = dev_get_drvdata(usb3_phy->dev);

	if (index > rtk_phy->num_phy) {
		dev_err(rtk_phy->dev, "%s: The port=%d is not in usb phy (num_phy=%d)\n",
			__func__, index, rtk_phy->num_phy);
		return;
	}

	do_rtk_usb3_phy_toggle(rtk_phy, index, connect);
}

static int rtk_phy_notify_port_status(struct usb_phy *x, int port,
				      u16 portstatus, u16 portchange)
{
	bool connect = false;

	pr_debug("%s port=%d portstatus=0x%x portchange=0x%x\n",
		 __func__, port, (int)portstatus, (int)portchange);
	if (portstatus & USB_PORT_STAT_CONNECTION)
		connect = true;

	if (portchange & USB_PORT_STAT_C_CONNECTION)
		rtk_phy_toggle(x, connect, port);

	return 0;
}
#endif

static int get_phy_data_by_efuse(struct rtk_phy *rtk_phy,
				 struct phy_parameter *phy_parameter, int index)
{
#if 0
	struct phy_cfg *phy_cfg = rtk_phy->phy_cfg;
	u8 value = 0;
	struct nvmem_cell *cell;

	if (!phy_cfg->check_efuse)
		goto out;

	cell = nvmem_cell_get(rtk_phy->dev, "usb_u3_tx_lfps_swing_trim");
	if (IS_ERR(cell)) {
		dev_dbg(rtk_phy->dev, "%s no usb_u3_tx_lfps_swing_trim: %ld\n",
			__func__, PTR_ERR(cell));
	} else {
		unsigned char *buf;
		size_t buf_size;

		buf = nvmem_cell_read(cell, &buf_size);
		value = buf[0] & USB_U3_TX_LFPS_SWING_TRIM_MASK;

		kfree(buf);
		nvmem_cell_put(cell);
	}

	if (value > 0 && value < 0x8)
		phy_parameter->efuse_usb_u3_tx_lfps_swing_trim = 0x8;
	else
		phy_parameter->efuse_usb_u3_tx_lfps_swing_trim = (u8)value;

out:
#endif
	return 0;
}

static void update_amplitude_control_value(struct rtk_phy *rtk_phy,
					   struct phy_parameter *phy_parameter)
{
	struct phy_cfg *phy_cfg;
	struct phy_reg *phy_reg;

	phy_reg = &phy_parameter->phy_reg;
	phy_cfg = rtk_phy->phy_cfg;

	if (phy_parameter->amplitude_control_coarse != AMPLITUDE_CONTROL_COARSE_DEFAULT) {
		u16 val_mask = AMPLITUDE_CONTROL_COARSE_MASK;
		u16 data;

		if (!phy_cfg->param[PHY_ADDR_0X20].addr && !phy_cfg->param[PHY_ADDR_0X20].data) {
			phy_cfg->param[PHY_ADDR_0X20].addr = PHY_ADDR_0X20;
			data = rtk_phy_read(phy_reg, PHY_ADDR_0X20);
		} else {
			data = phy_cfg->param[PHY_ADDR_0X20].data;
		}

		data &= (~val_mask);
		data |= (phy_parameter->amplitude_control_coarse & val_mask);

		phy_cfg->param[PHY_ADDR_0X20].data = data;
	}

	if (phy_parameter->efuse_usb_u3_tx_lfps_swing_trim) {
		u8 efuse_val = phy_parameter->efuse_usb_u3_tx_lfps_swing_trim;
		u16 val_mask = USB_U3_TX_LFPS_SWING_TRIM_MASK;
		int val_shift = USB_U3_TX_LFPS_SWING_TRIM_SHIFT;
		u16 data;

		if (!phy_cfg->param[PHY_ADDR_0X20].addr && !phy_cfg->param[PHY_ADDR_0X20].data) {
			phy_cfg->param[PHY_ADDR_0X20].addr = PHY_ADDR_0X20;
			data = rtk_phy_read(phy_reg, PHY_ADDR_0X20);
		} else {
			data = phy_cfg->param[PHY_ADDR_0X20].data;
		}

		data &= ~(val_mask << val_shift);
		data |= ((efuse_val & val_mask) << val_shift);

		phy_cfg->param[PHY_ADDR_0X20].data = data;
	}

	if (phy_parameter->amplitude_control_fine != AMPLITUDE_CONTROL_FINE_DEFAULT) {
		u16 val_mask = AMPLITUDE_CONTROL_FINE_MASK;

		if (!phy_cfg->param[PHY_ADDR_0X21].addr && !phy_cfg->param[PHY_ADDR_0X21].data)
			phy_cfg->param[PHY_ADDR_0X21].addr = PHY_ADDR_0X21;

		phy_cfg->param[PHY_ADDR_0X21].data =
			    phy_parameter->amplitude_control_fine & val_mask;
	}
}

static int parse_phy_data(struct rtk_phy *rtk_phy)
{
	struct udevice *dev = rtk_phy->dev;
	//struct device_node *np = dev_np(dev);
	struct phy_parameter *phy_parameter;
	int ret = 0;
	int index;
	const __be32 *prop;
	int psize, onesize;
	int na, ns, num_regs;

	na = dev_read_addr_cells(dev);
	ns = dev_read_size_cells(dev);
	//num_regs = of_property_count_elems_of_size(np, "reg", (na + ns) * 4);
	prop = dev_read_prop(dev, "reg", &psize);
	psize /= 4;
	onesize = na + na;
	num_regs = psize / onesize;

	rtk_phy->num_phy = num_regs;

	rtk_phy->phy_parameter = kzalloc(sizeof(struct phy_parameter) *
					      rtk_phy->num_phy, GFP_KERNEL);
	if (!rtk_phy->phy_parameter)
		return -ENOMEM;

	for (index = 0; index < rtk_phy->num_phy; index++) {
		phy_parameter = &((struct phy_parameter *)rtk_phy->phy_parameter)[index];

		phy_parameter->phy_reg.reg_mdio_ctl = dev_remap_addr_index(dev, index);

		/* Amplitude control address 0x20 bit 0 to bit 7 */
		if (dev_read_u32(dev, "realtek,amplitude-control-coarse-tuning",
					 &phy_parameter->amplitude_control_coarse))
			phy_parameter->amplitude_control_coarse = AMPLITUDE_CONTROL_COARSE_DEFAULT;

		/* Amplitude control address 0x21 bit 0 to bit 16 */
		if (dev_read_u32(dev, "realtek,amplitude-control-fine-tuning",
					 &phy_parameter->amplitude_control_fine))
			phy_parameter->amplitude_control_fine = AMPLITUDE_CONTROL_FINE_DEFAULT;

		get_phy_data_by_efuse(rtk_phy, phy_parameter, index);

		update_amplitude_control_value(rtk_phy, phy_parameter);
	}

	return ret;
}

static int rtk_usb3phy_probe(struct udevice *dev)
{
	struct rtk_phy *rtk_phy;
	struct phy *generic_phy;
	const struct phy_cfg *phy_cfg;
	int ret;

	phy_cfg = (struct phy_cfg *)dev_get_driver_data(dev);
	if (!phy_cfg) {
		dev_err(dev, "phy config are not assigned!\n");
		return -EINVAL;
	}

	rtk_phy = dev_get_priv(dev);

	rtk_phy->dev = dev;

	rtk_phy->phy_cfg = kzalloc(sizeof(*phy_cfg), GFP_KERNEL);

	memcpy(rtk_phy->phy_cfg, phy_cfg, sizeof(*phy_cfg));

	ret = parse_phy_data(rtk_phy);

	return ret;
}

static int rtk_usb3phy_remove(struct udevice *dev)
{
	struct rtk_phy *rtk_phy = dev_get_priv(dev);
	int index;

	for (index = 0; index < rtk_phy->num_phy; index++) {
		struct phy_parameter *phy_parameter;

		phy_parameter = &((struct phy_parameter *)rtk_phy->phy_parameter)[index];

		unmap_physmem(phy_parameter->phy_reg.reg_mdio_ctl, MAP_NOCACHE);
	}

	kfree(rtk_phy->phy_parameter);
	kfree(rtk_phy->phy_cfg);

	return 0;
}

static const struct phy_cfg rtd1295_phy_cfg = {
	.param_size = MAX_USB_PHY_DATA_SIZE,
	.param = {  [0] = {0x01, 0x4008},  [1] = {0x01, 0xe046},
		    [2] = {0x02, 0x6046},  [3] = {0x03, 0x2779},
		    [4] = {0x04, 0x72f5},  [5] = {0x05, 0x2ad3},
		    [6] = {0x06, 0x000e},  [7] = {0x07, 0x2e00},
		    [8] = {0x08, 0x3591},  [9] = {0x09, 0x525c},
		   [10] = {0x0a, 0xa600}, [11] = {0x0b, 0xa904},
		   [12] = {0x0c, 0xc000}, [13] = {0x0d, 0xef1c},
		   [14] = {0x0e, 0x2000}, [15] = {0x0f, 0x0000},
		   [16] = {0x10, 0x000c}, [17] = {0x11, 0x4c00},
		   [18] = {0x12, 0xfc00}, [19] = {0x13, 0x0c81},
		   [20] = {0x14, 0xde01}, [21] = {0x15, 0x0000},
		   [22] = {0x16, 0x0000}, [23] = {0x17, 0x0000},
		   [24] = {0x18, 0x0000}, [25] = {0x19, 0x4004},
		   [26] = {0x1a, 0x1260}, [27] = {0x1b, 0xff00},
		   [28] = {0x1c, 0xcb00}, [29] = {0x1d, 0xa03f},
		   [30] = {0x1e, 0xc2e0}, [31] = {0x1f, 0x2807},
		   [32] = {0x20, 0x947a}, [33] = {0x21, 0x88aa},
		   [34] = {0x22, 0x0057}, [35] = {0x23, 0xab66},
		   [36] = {0x24, 0x0800}, [37] = {0x25, 0x0000},
		   [38] = {0x26, 0x040a}, [39] = {0x27, 0x01d6},
		   [40] = {0x28, 0xf8c2}, [41] = {0x29, 0x3080},
		   [42] = {0x2a, 0x3082}, [43] = {0x2b, 0x2078},
		   [44] = {0x2c, 0xffff}, [45] = {0x2d, 0xffff},
		   [46] = {0x2e, 0x0000}, [47] = {0x2f, 0x0040}, },
	.check_efuse = false,
	.do_toggle = true,
	.do_toggle_once = false,
	.use_default_parameter = false,
	.check_rx_front_end_offset = false,
};

static const struct phy_cfg rtd1619_phy_cfg = {
	.param_size = MAX_USB_PHY_DATA_SIZE,
	.param = {  [8] = {0x08, 0x3591},
		   [38] = {0x26, 0x840b},
		   [40] = {0x28, 0xf842}, },
	.check_efuse = false,
	.do_toggle = true,
	.do_toggle_once = false,
	.use_default_parameter = false,
	.check_rx_front_end_offset = false,
};

static const struct phy_cfg rtd1319_phy_cfg = {
	.param_size = MAX_USB_PHY_DATA_SIZE,
	.param = {  [1] = {0x01, 0xac86},
		    [6] = {0x06, 0x0003},
		    [9] = {0x09, 0x924c},
		   [10] = {0x0a, 0xa608},
		   [11] = {0x0b, 0xb905},
		   [14] = {0x0e, 0x2010},
		   [32] = {0x20, 0x705a},
		   [33] = {0x21, 0xf645},
		   [34] = {0x22, 0x0013},
		   [35] = {0x23, 0xcb66},
		   [41] = {0x29, 0xff00}, },
	.check_efuse = true,
	.do_toggle = true,
	.do_toggle_once = false,
	.use_default_parameter = false,
	.check_rx_front_end_offset = false,
};

static const struct phy_cfg rtd1619b_phy_cfg = {
	.param_size = MAX_USB_PHY_DATA_SIZE,
	.param = {  [1] = {0x01, 0xac8c},
		    [6] = {0x06, 0x0017},
		    [9] = {0x09, 0x724c},
		   [10] = {0x0a, 0xb610},
		   [11] = {0x0b, 0xb90d},
		   [13] = {0x0d, 0xef2a},
		   [15] = {0x0f, 0x9050},
		   [16] = {0x10, 0x000c},
		   [32] = {0x20, 0x70ff},
		   [34] = {0x22, 0x0013},
		   [35] = {0x23, 0xdb66},
		   [38] = {0x26, 0x8609},
		   [41] = {0x29, 0xff13},
		   [42] = {0x2a, 0x3070}, },
	.check_efuse = true,
	.do_toggle = false,
	.do_toggle_once = true,
	.use_default_parameter = false,
	.check_rx_front_end_offset = false,
};

static const  struct phy_cfg rtd1319d_phy_cfg = {
	.param_size = MAX_USB_PHY_DATA_SIZE,
	.param = {  [1] = {0x01, 0xac89},
		    [4] = {0x04, 0xf2f5},
		    [6] = {0x06, 0x0017},
		    [9] = {0x09, 0x424c},
		   [10] = {0x0a, 0x9610},
		   [11] = {0x0b, 0x9901},
		   [12] = {0x0c, 0xf000},
		   [13] = {0x0d, 0xef2a},
		   [14] = {0x0e, 0x1000},
		   [15] = {0x0f, 0x9050},
		   [32] = {0x20, 0x7077},
		   [35] = {0x23, 0x0b62},
		   [37] = {0x25, 0x10ec},
		   [42] = {0x2a, 0x3070}, },
	.check_efuse = true,
	.do_toggle = false,
	.do_toggle_once = true,
	.use_default_parameter = false,
	.check_rx_front_end_offset = true,
};

static const struct udevice_id usbphy_rtk_dt_match[] = {
#if 0
	{ .compatible = "realtek,rtd1295-usb3phy", .data = &rtd1295_phy_cfg },
	{ .compatible = "realtek,rtd1319-usb3phy", .data = &rtd1319_phy_cfg },
	{ .compatible = "realtek,rtd1319d-usb3phy", .data = &rtd1319d_phy_cfg },
	{ .compatible = "realtek,rtd1619-usb3phy", .data = &rtd1619_phy_cfg },
#endif
	{ .compatible = "realtek,rtd1619b-usb3phy", .data = &rtd1619b_phy_cfg },
	{},
};

U_BOOT_DRIVER(rtk_usb3phy) = {
	.name	= "rtk-usb3phy",
	.id	= UCLASS_PHY,
	.of_match = usbphy_rtk_dt_match,
	.ops = &ops,
	.probe = rtk_usb3phy_probe,
	.remove = rtk_usb3phy_remove,
	.priv_auto	= sizeof(struct rtk_phy),
};

//MODULE_LICENSE("GPL");
//MODULE_ALIAS("platform: rtk-usb3phy");
//MODULE_AUTHOR("Stanley Chang <stanley_chang@realtek.com>");
//MODULE_DESCRIPTION("Realtek usb 3.0 phy driver");
