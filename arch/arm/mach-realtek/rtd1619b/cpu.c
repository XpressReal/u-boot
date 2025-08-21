/*
 * This file is subject to the terms and conditions of the GNU General Public
 * License.  See the file "COPYING" in the main directory of this archive
 * for more details.
 *
 * Copyright (C) 2021 by YH_HSIEH <yh_hsieh@realtek.com>
 *
 * Time initialization.
 */
#include <common.h>
#include <asm/arch/cpu.h>

/* Release the secondary core from holdoff state and kick it */
/* Skip CPU kick before setting jump address
 * Otherwise, CPU 1/2/3 will wake up from ROM code without getting correct jump address
 */
void smp_kick_all_cpus(void)
{
}
