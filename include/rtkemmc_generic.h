#ifndef __RTKEMMC_GENERIC_H__
#define __RTKEMMC_GENERIC_H__

#include <asm/arch/rtkemmc.h>
void rtk_mmc_config_setup(const char *name, struct mmc_config *cfg);

#ifdef __RTKEMMC_GENERIC_C__
	#define EXTERN_CALL
#else
	#define EXTERN_CALL extern
#endif

#endif // end of CONFIG_GENERIC_EMMC
