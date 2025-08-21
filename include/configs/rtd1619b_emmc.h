/*
 * Configuration settings for the Realtek 1619b QA board.
 *
 * Won't include this file.
 * Just type "make <board_name>_config" and will be included in source tree.
 */

#ifndef __CONFIG_RTK_RTD1619B_EMMC_H
#define __CONFIG_RTK_RTD1619B_EMMC_H

/*
 * Include the common settings of RTD1619B platform.
 */
#include <configs/rtd161xb_common.h>

/* ========================================================== */

#ifdef CONFIG_SPL
#define CONFIG_SPL_LIBDISK_SUPPORT

#ifdef CONFIG_SPL_BUILD
#undef CONFIG_EXTRA_ENV_SETTINGS
#endif
#endif

#define ARM_ROMCODE_SIZE			(188*1024)
/* #define CONFIG_FT_RESCUE */

#undef  V_NS16550_CLK
#define V_NS16550_CLK				432000000

#define COUNTER_FREQUENCY			27000000 // FIXME, need to know what impact it will cause

#endif /* __CONFIG_RTK_RTD1619B_EMMC_H */
