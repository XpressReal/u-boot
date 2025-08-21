/* Define US phy parameter */
#pragma once

#include <usb-phy.h>

struct rtk_usb;

/* XHCI u2drd*/
static struct u2phy_data port0_u2phy_page0_setting[] = {
	{0xe0, 0xA3},
//	{0xe1, 0x19},
//	{0xe2, 0x79},
//	{0xe3, 0x8D},
	{0xe4, 0xB2},
	{0xe5, 0x4F},
	{0xe6, 0x42},
//	{0xe7, 0x71},
//	{0xf0, 0xFC},
//	{0xf1, 0x8C},
//	{0xf2, 0x00},
//	{0xf3, 0x11},
//	{0xf4, 0x9B},
//	{0xf5, 0x00},
//	{0xf6, 0x00},
//	{0xf7, 0x0A},
	{0xff, 0x0}, /* No write any value to phy*/
};

static struct u2phy_data port0_u2phy_page1_setting[] = {
//	{0xe0, 0x25},
//	{0xe1, 0xef},
//	{0xe2, 0x60},
	{0xe3, 0x64},
//	{0xe4, 0x00},
//	{0xe5, 0x12},
//	{0xe6, 0xc0},
//	{0xe7, 0x00},
	{0xe0, 0x25},
	{0xe0, 0x21}, /* for toggle */
	{0xe0, 0x25},
	{0xff, 0x0}, /* No write any value to phy*/
};

static struct u2phy_data port0_u2phy_page2_setting[] = {
	{0xe7, 0x45},
	{0xff, 0x0}, /* No write any value to phy*/
};

/* XHCI u2host*/
static struct u2phy_data port1_u2phy_page0_setting[] = {
//	{0xe0, 0xe0},
//	{0xe1, 0x30},
//	{0xe2, 0x79},
//	{0xe3, 0x8d},
	{0xe4, 0xB1},
//	{0xe5, 0x65},
	{0xe6, 0x02},
//	{0xe7, 0x71},
//	{0xf0, 0xFC},
//	{0xf1, 0x8C},
//	{0xf2, 0x00},
//	{0xf3, 0x11},
//	{0xf4, 0x9B},
//	{0xf5, 0x00},
//	{0xf6, 0x00},
//	{0xf7, 0x0A},
	{0xff, 0x0}, /* No write any value to phy*/
};

static struct u2phy_data port1_u2phy_page1_setting[] = {
//	{0xe0, 0x25},
//	{0xe1, 0xef},
//	{0xe2, 0x60},
//	{0xe3, 0x44},
//	{0xe4, 0x00},
//	{0xe5, 0x0f},
//	{0xe6, 0x18},
//	{0xe7, 0xe3},
	{0xe0, 0x25},
	{0xe0, 0x21}, /* for toggle */
	{0xe0, 0x25},
	{0xff, 0x0}, /* No write any value to phy*/
};

static struct u2phy_data port1_u2phy_page2_setting[] = {
	{0xe7, 0x45},
	{0xff, 0x0}, /* No write any value to phy*/
};
/* XHCI u3drd*/
static struct u2phy_data port2_u2phy_page0_setting[] = {
//	{0xe0, 0xe0},
//	{0xe1, 0x30},
//	{0xe2, 0x79},
//	{0xe3, 0x8d},
	{0xe4, 0xB1},
//	{0xe5, 0x65},
	{0xe6, 0x02},
//	{0xe7, 0x71},
//	{0xf0, 0xFC},
//	{0xf1, 0x8C},
//	{0xf2, 0x00},
//	{0xf3, 0x11},
//	{0xf4, 0x9B},
//	{0xf5, 0x00},
//	{0xf6, 0x00},
//	{0xf7, 0x0A},
	{0xff, 0x0}, /* No write any value to phy*/
};

static struct u2phy_data port2_u2phy_page1_setting[] = {
//	{0xe0, 0x25},
//	{0xe1, 0xef},
//	{0xe2, 0x60},
//	{0xe3, 0x44},
//	{0xe4, 0x00},
//	{0xe5, 0x0f},
//	{0xe6, 0x18},
//	{0xe7, 0xe3},
	{0xe0, 0x25},
	{0xe0, 0x21}, /* for toggle */
	{0xe0, 0x25},
	{0xff, 0x0}, /* No write any value to phy*/
};

static struct u2phy_data port2_u2phy_page2_setting[] = {
	{0xe7, 0x45},
	{0xff, 0x0}, /* No write any value to phy*/
};

static struct u3phy_data port0_u3phy_data[] = {
//	{0x00, 0x400C},
	{0x01, 0xAC8C},
//	{0x02, 0x6042},
//	{0x03, 0x2771},
//	{0x04, 0x7555},
//	{0x05, 0x2333},
	{0x06, 0x0017},
//	{0x07, 0x2E00},
//	{0x08, 0x2EE1},
	{0x09, 0x724C},

	{0x0A, 0xB610},
	{0x0B, 0xB90D},
//	{0x0C, 0xC000},
	{0x0D, 0xEF2A},
//	{0x0E, 0x2010},
	{0x0F, 0x9050},

	{0x10, 0x000C},
//	{0x11, 0x4C10},
//	{0x12, 0xFC00},
//	{0x13, 0x0C81},
//	{0x14, 0xDE01},
//	{0x15, 0x0000},
//	{0x16, 0x0000},
//	{0x17, 0x0000},
//	{0x18, 0x0000},
//	{0x19, 0x6000},

//	{0x1A, 0x0085},
//	{0x1B, 0x2014},
//	{0x1C, 0xC900},
//	{0x1D, 0xA03F},
//	{0x1E, 0xC2E0},
//	{0x1F, 0x7E00},

	{0x20, 0x70FF},
	{0x21, 0xCFAA},
	{0x22, 0x0013},
	{0x23, 0xDB66},
//	{0x24, 0x4770},
//	{0x25, 0x126C},
	{0x26, 0x8609},
//	{0x27, 0x028F},
//	{0x28, 0xF802},
	{0x29, 0xFF13},

	{0x2A, 0x3070},
//	{0x2B, 0x8028},
//	{0x2C, 0xFFFF},
//	{0x2D, 0xFFFF},
//	{0x2E, 0x0000},
//	{0x2F, 0x8600},

	{0xff, 0x0}, /* No write any value to phy*/
};

static struct u3phy_data port1_u3phy_data[] = {
//	{0x00, 0x400C},
//	{0x01, 0xAC86},
//	{0x02, 0x6042},
//	{0x03, 0x2771},
//	{0x04, 0x72F5},
//	{0x05, 0x2AD3},
//	{0x06, 0x0006},
//	{0x07, 0x2E00},
//	{0x08, 0x3591},
//	{0x09, 0x925C},

//	{0x0A, 0xA608},
//	{0x0B, 0xA905},
//	{0x0C, 0xC000},
//	{0x0D, 0xEF1E},
//	{0x0E, 0x2010},
//	{0x0F, 0x8D50},

//	{0x10, 0x000C},
//	{0x11, 0x4C10},
//	{0x12, 0xFC00},
//	{0x13, 0x0C81},
//	{0x14, 0xDE01},
//	{0x15, 0x0000},
//	{0x16, 0x0000},
//	{0x17, 0x0000},
//	{0x18, 0x0000},
//	{0x19, 0x6000},

//	{0x1A, 0x0085},
//	{0x1B, 0x2014},
//	{0x1C, 0xC900},
//	{0x1D, 0xA03F},
//	{0x1E, 0xC2E0},
//	{0x1F, 0x7E00},

//	{0x20, 0x7058},
//	{0x21, 0xF645},
//	{0x22, 0x0013},
//	{0x23, 0xCB66},
//	{0x24, 0x4770},
//	{0x25, 0x126C},
//	{0x26, 0x840A},
//	{0x27, 0x01D6},
//	{0x28, 0xF802},
//	{0x29, 0xFF04},

//	{0x2A, 0x3040},
//	{0x2B, 0x8028},
//	{0x2C, 0xFFFF},
//	{0x2D, 0xFFFF},
//	{0x2E, 0x0000},
//	{0x2F, 0x8600},

	{0x09, 0x704C}, /* for toggle */
	{0x09, 0x724C},

	{0xff, 0x0}, /* No write any value to phy*/
};

#define USB2_PHY_NUM 3 // port0-port2

#define PORT0_USB2_PHY_ENABLE 1
#define PORT0_USB2_PHY_CTRL_ID 0
#define PORT0_USB2_PHY_WRITE_ADDR 0x98028280 // GUSB2PHYACC
#define PORT0_USB2_PHY_VSTATUS_ADDR 0x98013214 // VSTATUS_reg
#define PORT0_USB2_PHY_SWITCH_ADDR 0x98013270 // USB2_PHY_reg
#define PORT0_USB2_PHY_RESET_ADDR -1
#define PORT0_USB2_PHY_OTG_SWITCH_ADDR -1
#define PORT0_USB2_PHY_CHECK_LDO_POWER 1

#define PORT1_USB2_PHY_ENABLE 1
#define PORT1_USB2_PHY_CTRL_ID 1
#define PORT1_USB2_PHY_WRITE_ADDR 0x98031280 // GUSB2PHYACC
#define PORT1_USB2_PHY_VSTATUS_ADDR 0x98013C14 // VSTATUS_reg
#define PORT1_USB2_PHY_SWITCH_ADDR 0x98013C70 // USB2_PHY_reg
#define PORT1_USB2_PHY_RESET_ADDR -1
#define PORT1_USB2_PHY_OTG_SWITCH_ADDR -1
#define PORT1_USB2_PHY_CHECK_LDO_POWER 0

#define PORT2_USB2_PHY_ENABLE 1
#define PORT2_USB2_PHY_CTRL_ID 2
#define PORT2_USB2_PHY_WRITE_ADDR 0x98058280 // GUSB2PHYACC
#define PORT2_USB2_PHY_VSTATUS_ADDR 0x98013E14 // VSTATUS_reg
#define PORT2_USB2_PHY_SWITCH_ADDR 0x98013E70 //USB2_PHY_reg
#define PORT2_USB2_PHY_RESET_ADDR -1
#define PORT2_USB2_PHY_OTG_SWITCH_ADDR -1
#define PORT2_USB2_PHY_CHECK_LDO_POWER 1

#define DEFINE_USB2_PHY_PROP(str) \
static inline u32 get_usb2_phy_##str(int index) \
{ \
	switch (index) { \
	case 0: return PORT0_USB2_PHY_##str; \
	case 1: return PORT1_USB2_PHY_##str; \
	case 2: return PORT2_USB2_PHY_##str; \
	default: printf("ERROR port index %d\n", index); \
	} \
	return -1; \
}

#define GET_USB2_PHY_PROP(str, index) get_usb2_phy_##str(index)

DEFINE_USB2_PHY_PROP(ENABLE)
DEFINE_USB2_PHY_PROP(CTRL_ID)
DEFINE_USB2_PHY_PROP(WRITE_ADDR)
DEFINE_USB2_PHY_PROP(VSTATUS_ADDR)
DEFINE_USB2_PHY_PROP(SWITCH_ADDR)
DEFINE_USB2_PHY_PROP(RESET_ADDR)
DEFINE_USB2_PHY_PROP(OTG_SWITCH_ADDR)
DEFINE_USB2_PHY_PROP(CHECK_LDO_POWER)

#define DEFINE_USB2_PHY_DATA(str) \
static inline struct u2phy_data *get_usb2_phy_##str##_data(int index) \
{ \
	switch (index) { \
	case 0: return &port0_u2phy_##str##_setting[0]; \
	case 1: return &port1_u2phy_##str##_setting[0]; \
	case 2: return &port2_u2phy_##str##_setting[0]; \
	default: printf("ERROR port index %d\n", index); \
	} \
	return NULL; \
}

#define GET_USB2_PHY_DATA(str, index) get_usb2_phy_##str##_data(index)

DEFINE_USB2_PHY_DATA(page0)
DEFINE_USB2_PHY_DATA(page1)
DEFINE_USB2_PHY_DATA(page2)

//#define ARRAY_SIZE(arr)    (sizeof(arr) / sizeof((arr)[0]))

#define DEFINE_USB2_PHY_DATA_SIZE(str) \
static inline size_t get_usb2_phy_##str##_data_size(int index) \
{ \
	switch (index) { \
	case 0: return ARRAY_SIZE(port0_u2phy_##str##_setting); \
	case 1: return ARRAY_SIZE(port1_u2phy_##str##_setting); \
	case 2: return ARRAY_SIZE(port2_u2phy_##str##_setting); \
	default: printf("%s ERROR port index %d\n", __func__, index); \
	} \
	return 0; \
}

#define GET_USB2_PHY_DATA_SIZE(str, index) get_usb2_phy_##str##_data_size(index)

DEFINE_USB2_PHY_DATA_SIZE(page0)
DEFINE_USB2_PHY_DATA_SIZE(page1)
DEFINE_USB2_PHY_DATA_SIZE(page2)

/* bootcode not support usb 3.0*/
#define USB3_PHY_NUM 1 // port0-port1

#define PORT0_USB3_PHY_ENABLE 1
#define PORT0_USB3_PHY_CTRL_ID 2 // host/device id
#define PORT0_USB3_PHY_WRITE_ADDR 0x98013E10

#define PORT1_USB3_PHY_ENABLE 0
#define PORT1_USB3_PHY_CTRL_ID -1 // host/device id
#define PORT1_USB3_PHY_WRITE_ADDR -1

#define DEFINE_USB3_PHY_PROP(str) \
static inline u32 GET_USB3_PHY_##str(int index) \
{ \
	switch (index) { \
	case 0: return PORT0_USB3_PHY_##str; \
	case 1: return PORT1_USB3_PHY_##str; \
	default: printf("ERROR port index %d\n", index); \
	} \
	return -1; \
}

#define GET_USB3_PHY_PROP(str, index) GET_USB3_PHY_##str(index)

DEFINE_USB3_PHY_PROP(ENABLE)
DEFINE_USB3_PHY_PROP(CTRL_ID)
DEFINE_USB3_PHY_PROP(WRITE_ADDR)

#define DEFINE_USB3_PHY_DATA() \
static inline struct u3phy_data *get_usb3_phy_data(int index) \
{ \
	switch (index) { \
	case 0: return port0_u3phy_data; \
	case 1: return port1_u3phy_data; \
	default: printf("%s ERROR port index %d\n", __func__, index); \
	} \
	return NULL; \
}

#define GET_USB3_PHY_DATA(index) get_usb3_phy_data(index)

DEFINE_USB3_PHY_DATA()

#define DEFINE_USB3_PHY_DATA_SIZE() \
static inline size_t get_usb3_phy_data_size(int index) \
{ \
	switch (index) { \
	case 0: return ARRAY_SIZE(port0_u3phy_data); \
	case 1: return ARRAY_SIZE(port1_u3phy_data); \
	default: printf("%s ERROR port index %d\n", __func__, index); \
	} \
	return 0; \
}

#define GET_USB3_PHY_DATA_SIZE(index) get_usb3_phy_data_size(index)

DEFINE_USB3_PHY_DATA_SIZE()

int get_rtk_usb_phy2(struct rtk_usb *rtk_usb);
int get_rtk_usb_phy3(struct rtk_usb *rtk_usb);
