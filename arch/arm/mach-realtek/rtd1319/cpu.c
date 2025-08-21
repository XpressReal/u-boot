/*
 * This file is subject to the terms and conditions of the GNU General Public
 * License.  See the file "COPYING" in the main directory of this archive
 * for more details.
 *
 * Copyright (C) 2018 by YH_HSIEH <yh_hsieh@realtek.com>
 *
 * Time initialization.
 */
#include <common.h>
#include <command.h>
#include <asm/io.h>
#include <asm/arch/cpu.h>

#ifdef CONFIG_RTK_SLAVE_CPU_BOOT
void bootup_slave_cpu(void)
{
	int ret;

	printf("PSCI_BOOT is enabled, Skip bringing up slave CPUs\n");

	/* Setup fss scan */
	ret = run_command_list("fss_scan main", -1, 0);
	if (ret)
		printf("failed to run fss scan main\n");
}
#endif //CONFIG_RTK_SLAVE_CPU_BOOT:
