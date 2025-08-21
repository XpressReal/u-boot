/*
 * Configuration settings for the Realtek 131x QA board.
 *
 * Won't include this file.
 * Just type "make <board_name>_config" and will be included in source tree.
 */

#ifndef __CONFIG_RTK_RTD131X_QA_SPI_64_H
#define __CONFIG_RTK_RTD131X_QA_SPI_64_H

/*
 * Include the common settings of RTD1319 platform.
 */
#include <configs/rtd131x_common.h>

/* ========================================================== */
#ifdef CONFIG_SPL
#define CONFIG_BOARD_SIZE_LIMIT		$$((384 * 1024))
#define CONFIG_SPL_LIBDISK_SUPPORT

/* #define CONFIG_SPL_USB_SUPPORT */
/* #define CONFIG_SPL_USB_HOST_SUPPORT */
/* #define CONFIG_OMAP_USB_PHY */
/* #define CONFIG_SPL_GPIO_SUPPORT */

#ifdef CONFIG_SPL_BUILD
#undef CONFIG_EXTRA_ENV_SETTINGS
#endif
#endif

#define ARM_ROMCODE_SIZE			(188*1024)
/* #define CONFIG_FT_RESCUE */

#undef  V_NS16550_CLK
#define V_NS16550_CLK                               432000000

#define COUNTER_FREQUENCY                           27000000 // FIXME, need to know what impact it will cause

#endif /* __CONFIG_RTK_RTD131X_QA_SPI_64_H */
