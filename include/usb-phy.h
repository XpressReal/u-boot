/* Define USB PHY API */

#pragma once

struct u2phy_data {
	char addr;
	char data;
};

enum interface_type {
	GUSB2PHYACC = 1,
	INSNREG05 = 2,
};

struct phy_switch {
	u32 switch_addr;
	u32 reset_addr;
	u32 otg_enable_addr;
};

struct usb2_phy {
	u32 vstatus_addr;
	u32 write_phy_addr;
	enum interface_type type;

	int ctrl_id;

	struct u2phy_data *phy_page0_setting;
	struct u2phy_data *phy_page1_setting;
	struct u2phy_data *phy_page2_setting;
	size_t phy_page0_size;
	size_t phy_page1_size;
	size_t phy_page2_size;

	struct phy_switch phy_switch;

	int phy_id;
	int phy_count;

	bool check_ldo_power;
};

struct u3phy_data {
	char addr;
	u16 data;
};

struct usb3_phy {
	u32 write_phy_addr;

	int ctrl_id;

	struct u3phy_data *phy_data;
	size_t phy_data_size;

	int phy_id;
	int phy_count;
};

int usb2_phy_init(struct usb2_phy *usb2phy);
int usb3_phy_init(struct usb3_phy *usb3phy);

int usb2_phy_switch_to_device(struct usb2_phy *usb2phy);
int usb2_phy_switch_to_host(struct usb2_phy *usb2phy);
