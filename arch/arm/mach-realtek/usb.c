
#include <common.h>
#include <log.h>
#include <dm.h>
#include <dm/device-internal.h>
#include <dm/device_compat.h>
#include <dm/lists.h>
#include <dm/device.h>
#include <linux/bitops.h>
#include <linux/delay.h>

#include <malloc.h>
#include <asm/io.h>
#include <asm/arch/platform_lib/board/gpio.h>
#include <asm/arch/usb.h>
#include <asm/arch/usb-phy.h>
#include <asm/arch/cpu.h>

#define USB_PORT_NUM 3
struct rtk_usb_port {
	//int gpio_type;
	int enable;
	int gpio_num;
	int gpio_active;
};

struct rtk_usb_config {
	struct udevice *dev;
	bool disable_usb;
	bool disable_type_c;
	struct rtk_usb_port usb_port[USB_PORT_NUM];
};

#define USB_CTRL 0x98007FB0
#define ISO_USB_U2PHY_REG_LDO_PW (BIT(20) | BIT(21) | BIT(22) | BIT(23))

static void enable_phy_ldo_power(void)
{
	u32 reg = USB_CTRL;
	volatile unsigned int val;

	val = readl(reg);
	val |= ISO_USB_U2PHY_REG_LDO_PW;
	writel(val, reg);
	mdelay(1);

	printf("Enable phy ldo power! USB_CTRL val=0x%x\n", val);
}

// CRT_SOFT_RESET usb part
#define rtk_usb_u3phy1_mdio	(0x1 << 25) // u3drd u3phy
#define rtk_usb_u3phy1 		(0x1 << 24) // u3drd u3phy
#define rtk_usb_u3phy0_mdio	(0x1 << 23) // unused
#define rtk_usb_u3phy0 		(0x1 << 22) // unused
#define rtk_usb_u3host_mac 	(0x1 << 21) // u3drd mac
#define rstn_type_c 		(0x1 << 20) // type_c module
#define rstn_usb 			(0x1 << 19) // all
#define rstn_usb_phy2 		(0x1 << 18) // u3drd u2phy
#define rstn_usb_phy1 		(0x1 << 17) // u2host u2phy
#define rstn_usb_phy0 		(0x1 << 16) // u2drd u2phy
#define rstn_usb_u2host_mac	(0x1 << 15) // u2host mac
#define rstn_usb_drd_mac 	(0x1 << 14) // u2drd mac
// CRT_CLOCK_ENABLE usb part
#define clk_en_usb 			(0x1 << 16) // all
#define clk_en_usb_u3host 	(0x1 << 15) // u3drd
#define clk_en_usb_u2host 	(0x1 << 14) // u2host
#define clk_en_usb_drd 		(0x1 << 13) // u2drd

static void _rtk_usb_clock_init(void)
{
	void *reset = (void *)ISO_CRT_BASE + 0x0;
	void *clk_en = (void *)ISO_CRT_BASE + 0x4;

	int reset_pll_flag = rstn_usb_phy0 | rstn_usb_phy1 | rstn_usb_phy2 |
		    rtk_usb_u3phy0 | rtk_usb_u3phy0_mdio |
		    rtk_usb_u3phy1 | rtk_usb_u3phy1_mdio;
	int reset_usb_flag = rstn_usb_drd_mac | rstn_usb_u2host_mac |
		                 rtk_usb_u3host_mac |
		                 rstn_type_c | rstn_usb;

	int clk_en_domain_flag = clk_en_usb;

	int clk_en_mac_flag = clk_en_usb_u3host | clk_en_usb_u2host | clk_en_usb_drd;

	debug("Realtek-usb: init start soft_reset=%x, clock_enable=%x\n",
		    (uint32_t)(readl((volatile u32*)reset)),
		    (uint32_t)(readl((volatile u32*)clk_en)));

	//Enable PLL
	writel(reset_pll_flag | readl((volatile u32*)reset), (volatile u32*) reset);

	mdelay(200);
	debug("Realtek-usb: Enable PLL soft_reset=%x, clock_enable=%x\n",
		    (uint32_t)(readl((volatile u32*)reset)),
		    (uint32_t)(readl((volatile u32*)clk_en)));

	//Turn on USB clk
	writel(clk_en_domain_flag | readl((volatile u32*)clk_en),
		    (volatile u32*) clk_en);
	writel(~clk_en_domain_flag & readl((volatile u32*)clk_en),
		    (volatile u32*) clk_en);
	mdelay(10);

	debug("Realtek-usb: trigger CLK_EN_USB soft_reset=%x, clock_enable=%x\n",
		    (uint32_t)(readl((volatile u32*)reset)),
		    (uint32_t)(readl((volatile u32*)clk_en)));

	writel(reset_usb_flag | readl((volatile u32*)reset), (volatile u32*) reset);
	mdelay(10);
	debug("Realtek-usb: Turn on all RSTN_USB soft_reset=%x, clock_enable=%x\n",
		    (uint32_t)(readl((volatile u32*)reset)),
		    (uint32_t)(readl((volatile u32*)clk_en)));

	writel(clk_en_domain_flag | readl((volatile u32*)clk_en),
		    (volatile u32*) clk_en);
	mdelay(10);
	writel(clk_en_mac_flag | readl((volatile u32*)clk_en),
		    (volatile u32*) clk_en);
	debug("Realtek-usb: Turn on CLK_EN_USB soft_reset=%x, clock_enable=%x\n",
		(uint32_t)(readl((volatile u32*)reset)),
		(uint32_t)(readl((volatile u32*)clk_en)));

	printf("\nRealtek-usb: Turn on USB OK (soft_reset=%x, clock_enable=%x)\n",
		(uint32_t)(readl((volatile u32*)reset)),
		(uint32_t)(readl((volatile u32*)clk_en)));

	/* Add workaround to enable u3phy selective */
	writel(0x1 << 6 | readl(0x9800705c), 0x9800705c);

	enable_phy_ldo_power();
}

static void _rtk_usb_clock_stop(void)
{
	void *reset = (void *)ISO_CRT_BASE + 0x0;
	void *clk_en = (void *)ISO_CRT_BASE + 0x4;

	int reset_pll_flag = rstn_usb_phy0 | rstn_usb_phy1 | rstn_usb_phy2 |
		    rtk_usb_u3phy0 | rtk_usb_u3phy0_mdio |
		    rtk_usb_u3phy1 | rtk_usb_u3phy1_mdio;
	int reset_usb_flag = rstn_type_c | rstn_usb | rstn_usb_drd_mac |
		    rstn_usb_u2host_mac | rtk_usb_u3host_mac;

	int clk_en_domain_flag = clk_en_usb;

	int clk_en_mac_flag = clk_en_usb_u3host | clk_en_usb_u2host | clk_en_usb_drd;

	//Stop PLL
	writel(~(reset_pll_flag | reset_usb_flag) &
		    readl((volatile u32*)reset), (volatile u32*) reset);

	writel(~(clk_en_domain_flag | clk_en_mac_flag) &
		    readl((volatile u32*)clk_en), (volatile u32*) clk_en);

	printf("Realtek-usb: stop soft_reset=%x, clock_enable=%x\n",
		(uint32_t)(readl((volatile u32*)reset)),
		(uint32_t)(readl((volatile u32*)clk_en)));
}

static void _rtk_usb_port_power(struct rtk_usb_port *usb_port, int port, int enable)
{
	if ((port >= USB_PORT_NUM) || (port < 0))
		return;
	if (!usb_port[port].enable)
		return;
	printf("Port%d power %s\n", port, enable ? "on" : "off");
	enable = usb_port[port].gpio_active ? enable : !enable;
	debug("SET num=%d enable=%d\n", usb_port[port].gpio_num, enable);
	setISOGPIO(usb_port[port].gpio_num, enable);
}

#define Rd_EN (1<<1)
#define DFP_3A_MODE (1<<5 | 1<<6)
#define DFP_1p5A_MODE (1<<6)
#define DFP_USB_MODE (1<<5)
#define Rp4p7k_EN (1<<4)
#define Rp36k_EN (1<<3)
#define Rp12k_EN (1<<2)
#define CC_DET (1<<0)
#define UFP_CC1_CODE 0x03DEF780
#define UFP_CC2_CODE 0x03DEF780
#define UFP_CC1_COMP 0x77777777
#define UFP_CC2_COMP 0x77777777
#define DFP_3A_CC1_CODE 0x03DEF780
#define DFP_3A_CC2_CODE 0x03DEF780
#define DFP_3A_CC1_COMP 0x77777777
#define DFP_3A_CC2_COMP 0x77777777
#define DFP_1p5A_CC1_CODE 0x03DEF780
#define DFP_1p5A_CC2_CODE 0x03DEF780
#define DFP_1p5A_CC1_COMP 0x77777777
#define DFP_1p5A_CC2_COMP 0x77777777
#define DFP_USB_CC1_CODE 0x03DEF780
#define DFP_USB_CC2_CODE 0x03DEF780
#define DFP_USB_CC1_COMP 0x77777777
#define DFP_USB_CC2_COMP 0x77777777

static void _rtk_usb_power_on(struct udevice *dev, int port)
{
	struct rtk_usb_config *rtk_usb;
	int check, type_c_have_device = 0;

	rtk_usb = dev_get_priv(dev);
	if (rtk_usb->disable_usb)
		return;

	//Usb2 5V
	if (port == 0) {
		_rtk_usb_port_power(rtk_usb->usb_port, 0, 1);
		return;
	} else if (port == 1) {
		_rtk_usb_port_power(rtk_usb->usb_port, 1, 1);
		return;
	}

	if (!rtk_usb->disable_type_c) {
	writel(UFP_CC1_COMP, (volatile u32*) USB_TYPEC_CTRL_CC1_1);
	writel(UFP_CC2_COMP, (volatile u32*) USB_TYPEC_CTRL_CC2_1);
	writel(UFP_CC1_CODE | Rd_EN | CC_DET, (volatile u32*) USB_TYPEC_CTRL_CC1_0);
	writel(UFP_CC2_CODE | Rd_EN | CC_DET, (volatile u32*) USB_TYPEC_CTRL_CC2_0);

	mdelay(10);

	check = readl((volatile u32*)USB_TYPEC_STS);
		debug("Realtek-usb: UFP check type_c cc status=0x%x\n", check);
	if ((check & 0x7) != 0x0) {
		printf("Realtek-usb: UFP cc1 detect type_c have power (status=0x%x)\n",
			    check);
		goto out;
	} else if ((check & 0x38) != 0x0) {
		printf("Realtek-usb: UFP cc2 detect type_c have power (status=0x%x)\n",
			    check);
		goto out;
	}

	/* Set Host Mode */
	writel(DFP_1p5A_CC1_COMP, (volatile u32*) USB_TYPEC_CTRL_CC1_1);
	writel(DFP_1p5A_CC2_COMP, (volatile u32*) USB_TYPEC_CTRL_CC2_1);
	writel(DFP_1p5A_CC1_CODE | DFP_1p5A_MODE | Rp12k_EN | CC_DET,
		    (volatile u32*) USB_TYPEC_CTRL_CC1_0);
	writel(DFP_1p5A_CC2_CODE | DFP_1p5A_MODE | Rp12k_EN | CC_DET,
		    (volatile u32*) USB_TYPEC_CTRL_CC2_0);

	mdelay(1);

	check = readl((volatile u32*)USB_TYPEC_STS);
	debug("Realtek-usb: DFP check type_c cc status=0x%x\n", check);
	if ((check & 0x7) != 0x7) {
		debug("Realtek-usb: DFP cc1 detect (status=0x%x)\n", check);
		type_c_have_device = 1;
		debug("Realtek-usb: Set ISOGPIO 53 to high (cc1)\n");
		setISOGPIO(53, 1);
	} else if ((check & 0x38) != 0x38) {
		debug("Realtek-usb: DFP cc2 detect (status=0x%x)\n", check);
		type_c_have_device = 1;
		debug("Realtek-usb: Set ISOGPIO 53 to low (cc2)\n");
		setISOGPIO(53, 0);
	}
	} // !rtk_usb->disable_type_c

	//Type C 5V
	if (rtk_usb->disable_type_c || type_c_have_device) {
		printf("Realtek-usb: Turn on Type C port power (port2)\n");
		_rtk_usb_port_power(rtk_usb->usb_port, 2, 1);
	}

out:
	return;
}

static int rtk_usb_bind(struct udevice *dev)
{
	int ret = 0;

#if CONFIG_IS_ENABLED(DM)
	ret = dm_scan_fdt_dev(dev);
#endif /* CONFIG_IS_ENABLED(DM) */
	return ret;
}

static void parse_usb_data(struct rtk_usb_config *rtk_usb)
{
	struct udevice *dev = rtk_usb->dev;
	ofnode node;
	int usb_enabled = 0;

	rtk_usb->disable_type_c = true;
	rtk_usb->disable_usb = true;

	if (!dev_read_enabled(dev))
		return;

	node = dev_read_subnode(dev, "type-c@7220");
        if (ofnode_valid(node) && ofnode_is_enabled(node))
		rtk_usb->disable_type_c = false;

	dev_for_each_subnode(node, dev) {
		const char port_name[] = "usb-port";
		const char *name = ofnode_get_name(node);
		int port;
		u32 val;

		if (!name || strncmp(name, port_name, strlen(port_name)))
			continue;
		port = name[strlen(port_name)] - '0';
		if ((port >= USB_PORT_NUM) || (port < 0))
			continue;
		rtk_usb->usb_port[port].enable = 0;
		if (!ofnode_valid(node) || !ofnode_is_enabled(node))
			continue;
		dev_dbg(dev, "usb node: %s\n", ofnode_get_name(node));

		if (!ofnode_read_u32_index(node, "realtek,power-gpio", 0, &val) &&
			!ofnode_read_u32_index(node, "realtek,power-gpio", 1, &rtk_usb->usb_port[port].gpio_num) &&
			!ofnode_read_u32_index(node, "realtek,power-gpio", 2, &val)) {
			// GPIO_ACTIVE_HIGH: 0x0, GPIO_ACTIVE_LOW: 0x1
			rtk_usb->usb_port[port].gpio_active = !val;
			rtk_usb->usb_port[port].enable = 1;
			usb_enabled++;
		}
		else
			continue;
		debug("%s: port:%d, gpio_num:%d, active:%d\n", __func__, port, rtk_usb->usb_port[port].gpio_num, rtk_usb->usb_port[port].gpio_active);
	}
	rtk_usb->disable_usb = !usb_enabled;
	debug("%s: disable_usb: %d, disable_type_c: %d\n", __func__, rtk_usb->disable_usb, rtk_usb->disable_type_c);
}

static int rtk_usb_probe(struct udevice *dev)
{
	struct rtk_usb_config *rtk_usb;
	int port;

	rtk_usb = dev_get_priv(dev);
	rtk_usb->dev = dev;

	parse_usb_data(rtk_usb);

	_rtk_usb_clock_init();

	for (port = 0; port < USB_PORT_NUM; port++)
		_rtk_usb_power_on(dev, port);

	return 0;
}

static int rtk_usb_remove(struct udevice *dev)
{
	return 0;
}

static const struct udevice_id rtk_usb_ids[] = {
	{ .compatible = "realtek,usb-manager" },
	{ }
};

U_BOOT_DRIVER(rtk_usb_manager) = {
	.name	= "rtk-usb-manager",
	.id	= UCLASS_NOP,
	.of_match = rtk_usb_ids,
	.bind = rtk_usb_bind,
	.probe = rtk_usb_probe,
	.remove = rtk_usb_remove,
	.priv_auto = sizeof(struct rtk_usb_config),
};
