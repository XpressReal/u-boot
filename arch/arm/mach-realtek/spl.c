/*
 * This file is subject to the terms and conditions of the GNU General Public
 * License.  See the file "COPYING" in the main directory of this archive
 * for more details.
 *
 * Copyright (C) 2019 by YH Hsieh <yh_hsieh@realtek.com>
 *
 * Time initialization.
 */
#include <common.h>
#include <linux/arm-smccc.h>
#include <linux/libfdt.h>
#include <cpu_func.h>
#include <spl.h>
#include <mmc.h>
#include <asm/sections.h>
#include <asm/arch/platform_lib/board/gpio.h>
#include <asm/arch/cpu.h>
#include <image.h>
#include <boot_fit.h>
#include <malloc.h>

#ifdef CONFIG_SPL_BUILD

DECLARE_GLOBAL_DATA_PTR;

void spl_board_init(void)
{
	/* Prepare console output */
	preloader_console_init();
}

#if defined(CONFIG_OF_BOARD)
void *board_fdt_blob_setup(int *err)
{
	void *fdt_blob = NULL;

	*err = 0;
	/* FDT is at end of BSS unless it is in a different memory region */
	if (IS_ENABLED(CONFIG_SPL_SEPARATE_BSS))
		fdt_blob = (ulong *)&_image_binary_end;
	else
		fdt_blob = (ulong *)&__bss_end;

	return fdt_blob;
}
#endif


void board_boot_order(u32 *spl_boot_list)
{
	int index = 0;
#if CONFIG_IS_ENABLED(USB_STORAGE)
#if CONFIG_USB_BOOT_GPIO_NUM
	setISOGPIO_pullsel(CONFIG_USB_BOOT_GPIO_NUM, PULL_UP);
	if (!getISOGPIO(CONFIG_USB_BOOT_GPIO_NUM))
#endif
	spl_boot_list[index++] = BOOT_DEVICE_USB;
#endif /* USB_STORAGE */

#if defined(CONFIG_SPL_MMC)
#if CONFIG_IS_ENABLED(RTK_SD_DRIVER)
#if defined(CONFIG_RTK_MMC_DRIVER)
	spl_boot_list[index++] = BOOT_DEVICE_MMC2;
#else
	spl_boot_list[index++] = BOOT_DEVICE_MMC1;
#endif /* RTK_MMC_DRIVER */
#endif /* RTK_SD_DRIVER */

#ifdef CONFIG_RTK_MMC_DRIVER
	spl_boot_list[index++] = BOOT_DEVICE_MMC1;
	spl_boot_list[index++] = BOOT_DEVICE_MMC1;
	spl_boot_list[index++] = BOOT_DEVICE_MMC1;
#endif /* RTK_MMC_DRIVER */
#endif /* SPL_MMC */

#ifdef CONFIG_SPI_RTK_SFC
	spl_boot_list[index++] = BOOT_DEVICE_SPI;
#endif /* SPI_RTK_SFC */
	spl_boot_list[index++] = BOOT_DEVICE_NONE;
}

#if defined(CONFIG_SPL_MMC)
#if defined(CONFIG_SPL_FS_FAT) || defined(CONFIG_SPL_FS_EXT4)
static const u32 mmcsd_mode = MMCSD_MODE_FS;
#elif defined(CONFIG_SUPPORT_EMMC_BOOT)
static const u32 mmcsd_mode = MMCSD_MODE_EMMCBOOT;
#else
static const u32 mmcsd_mode = MMCSD_MODE_RAW;
#endif
/* MMCSD_MODE_RAW:1, MMCSD_MODE_FS:2, MMCSD_MODE_EMMCBOOT:3 */
u32 spl_mmc_boot_mode(struct mmc *mmc, const u32 boot_device)
{
	static u32 mode_offset = 0;
	u32 __maybe_unused mode = mmcsd_mode + mode_offset;

#if defined(CONFIG_SPL_FS_FAT)
	if (mode == MMCSD_MODE_FS)
		spl_fat_force_reregister();
#endif /* SPL_FS_FAT */

#ifdef CONFIG_RTK_MMC_DRIVER
	/* Try FS first, then EMMCBOOT at specific raw offset */
	if (boot_device == BOOT_DEVICE_MMC1) {
		if (mode >= MMCSD_MODE_EMMCBOOT)
			return mode;
		return mmcsd_mode + mode_offset++;
	}
#endif /* RTK_MMC_DRIVER */
	return mmcsd_mode;
}

int spl_mmc_emmc_boot_partition(struct mmc *mmc)
{
	/* 1 and 2 match up to boot0 / boot1 */
	static int part = 1;
	return part++;
}
#endif

static const char *cfg_board_name = NULL;
#if defined(CONFIG_TARGET_RTD1319)
int cmd_bl31_pcpu(void)
{
	struct arm_smccc_res res;

	//process pcpu
	arm_smccc_smc(0x8400ff09, 0, 0, 0, 0, 0, 0, 0, &res);

	return res.a0;
}

#else
static volatile uint FW_INFO[][3] = {
	{ 0xE, 0, 0 },
	{ 0x12, 0, 0 },
	};

#if CONFIG_IS_ENABLED(LOAD_FIT) || CONFIG_IS_ENABLED(LOAD_FIT_FULL)
void spl_perform_fixups(struct spl_image_info *spl_image) {
	void *blob = spl_image->fdt_addr;
	void *dtbo;
	int offset;
	const char *pcpu_name[] = {"pcpu_cert", "pcpu"};
	int node;
	const void *p;
	int i;

	if (!blob || (fdt_magic(blob) != FDT_MAGIC))
		return;

# if CONFIG_IS_ENABLED(MULTI_DTB_FIT) && defined(CONFIG_OF_LIBFDT_OVERLAY)
	cfg_board_name = "cipher";
	dtbo = locate_dtb_in_fit(gd->multi_dtb_fit);
	cfg_board_name = NULL;
	/* empty DTBO size: 72, one AES-128 key without IV: 224 */
	if (dtbo && fdt_magic(dtbo) == FDT_MAGIC && (fdt_totalsize(dtbo) > 100))
		fdt_overlay_apply_verbose((void *)blob, dtbo);

	if(rtk_is_secure_boot()) {
		cfg_board_name = "tee";
		dtbo = locate_dtb_in_fit(gd->multi_dtb_fit);
		cfg_board_name = NULL;
		if (dtbo && fdt_magic(dtbo) == FDT_MAGIC)
			fdt_overlay_apply_verbose((void *)blob, dtbo);
	}
#endif

	offset = fdt_subnode_offset(blob, 0, "fit-images");
	if(offset < 0)
		return;

	for (i=0; i<2; i++) {
		node = fdt_subnode_offset(blob, offset, pcpu_name[i]);
		if(node >=0) {
			uint load = fdtdec_get_uint64(blob, node, "load", 0x0);
			if (!FW_INFO[0][2]) {
				FW_INFO[0][2] = load;
			} else if (load != (FW_INFO[0][2]+FW_INFO[0][1])) {
				FW_INFO[0][2] = 0;
			}
			p = fdt_getprop(blob, node, "size", NULL);
			if (p) FW_INFO[0][1] += fdt32_to_cpu(*(const uint32_t *)p);
		}
	}

	node = fdt_subnode_offset(blob, offset, "tee");
	if(node >= 0) {
		p = fdt_getprop(blob, node, "size", NULL);
		if (p) FW_INFO[1][1] = fdt32_to_cpu(*(const uint32_t *)p);
		FW_INFO[1][2] = fdtdec_get_uint64(blob, node, "load", 0x0);
	}
}
#endif

int cmd_bl31_fw(uint idx)
{
	struct arm_smccc_res res;
	uint type, fw_size, fw_addr;

	if(idx > sizeof(FW_INFO)/(sizeof(uint)*3)) return -1;

	type = FW_INFO[idx][0];
	fw_size = FW_INFO[idx][1];
	fw_addr = FW_INFO[idx][2];
	//printf("%s: (%x, %x, %x)\n", __func__, type, fw_size, fw_addr);

	if (!fw_size || !fw_addr) return -1;
	flush_cache(fw_addr, fw_size);
	arm_smccc_smc(0x8400ff39, fw_addr, fw_size, type, 0, 0, 0, 0, &res);

	return res.a0;
}
#endif

void spl_board_prepare_for_boot(void)
{
#if defined(CONFIG_TARGET_RTD1319)
	cmd_bl31_pcpu();
#else
	cmd_bl31_fw(0);
	cmd_bl31_fw(1);
#endif
}

#if defined(CONFIG_TARGET_RTD1619B)
int fdtdec_board_setup(const void *fdt_blob) {
	int ret = 0;
#if defined(CONFIG_RTK_MMC_DRIVER) && !defined(CONFIG_SPI_RTK_SFC)
	const bool secure = rtk_is_secure_boot();
	int node = fdt_node_offset_by_compatible(fdt_blob, -1, "realtek,rtd1619b-usb3dwc3");

	if(FIT_IMAGE_ENABLE_VERIFY && secure)
		if(fdt_find_and_setprop((void *)fdt_blob, "/"FIT_SIG_NODENAME"/key-uboot", FIT_KEY_REQUIRED, "conf", 5, 0))
		ret = fdt_find_and_setprop((void *)fdt_blob, "/"FIT_SIG_NODENAME"/key-dev", FIT_KEY_REQUIRED, "conf", 5, 0);

	/* disable USB3.0 */
	if (node >= 0) {
		static char data[32] __aligned(4);
		const void *ptmp;
		int len;
		fdt_setprop(fdt_blob, node, "phy-names", "usb2-phy", 9);
		/* Update PHY list, by keeping the first entry */
		ptmp = fdt_getprop(fdt_blob, node, "phys", &len);
		if (ptmp) {
			len = min_t(unsigned int, sizeof(u32), len);
			memcpy(data, ptmp, len);
			fdt_setprop(fdt_blob, node, "phys", data, len);
		}
		ptmp = fdt_getprop(fdt_blob, node, "usb-phy", &len);
		if (ptmp) {
			len = min_t(unsigned int, sizeof(u32), len);
			memcpy(data, ptmp, len);
			fdt_setprop(fdt_blob, node, "usb-phy", data, len);
		}
	}
#endif

#if CONFIG_IS_ENABLED(MULTI_DTB_FIT) && defined(CONFIG_OF_LIBFDT_OVERLAY)
	const void *dtbo;
	void *dtbo_copy, *fdt_copy;
	u32 dtbo_size, fdt_size;
	cfg_board_name = "cipher";
	dtbo = locate_dtb_in_fit(gd->multi_dtb_fit);
	cfg_board_name = NULL;
	/* empty DTBO size: 72, one AES-128 key without IV: 224 */
	if (dtbo && fdt_magic(dtbo) == FDT_MAGIC && (fdt_totalsize(dtbo) > 100)) {
		dtbo_size = fdt_totalsize(dtbo);
		fdt_size = fdt_totalsize(fdt_blob);
		dtbo_copy = malloc(dtbo_size);
		memcpy(dtbo_copy, dtbo, dtbo_size);
		fdt_copy = malloc(dtbo_size + fdt_size);
		memcpy(fdt_copy, fdt_blob, dtbo_size + fdt_size);
		fdt_increase_size((void *)fdt_copy, dtbo_size);
		fdt_overlay_apply_verbose((void *)fdt_copy, dtbo_copy);
		gd->fdt_blob = fdt_copy;
	}
#endif

	return ret;
}
#endif

int __maybe_unused board_fit_config_name_match(const char *name)
{
	const char *board_name = CONFIG_BOARD_FIT_CONFIG_NAME;
	const u32 select = get_rtd1xxx_gpio_select();
	const bool secure = rtk_is_secure_boot();

	if (!strlen(CONFIG_BOARD_FIT_CONFIG_NAME))
		board_name = (select & 0x1) ? "rtd1619b-bleedingedge-emmc" : "rtd1619b-backinblack";
	if (cfg_board_name)
		board_name = cfg_board_name;
	//preloader_console_init();
	debug("%s: Check %s, default %s, select %d\n", __func__, name, board_name, select);

	if (!strncmp(name, board_name, strlen(board_name))) {
		/* prefer TEE one on secure boot, which comes before normal one */
		if (secure) {
			return 0;
		/* require exact match on normal boot */
		} else if(!strcmp(name, board_name)) {
			return 0;
		}
	}

	return -1;
}
#endif
