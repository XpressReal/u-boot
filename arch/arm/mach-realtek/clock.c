/*
 * This file is subject to the terms and conditions of the GNU General Public
 * License.  See the file "COPYING" in the main directory of this archive
 * for more details.
 *
 * Copyright (C) 2015 by Tom Ting <tom_ting@realtek.com>
 *
 * Time initialization.
 */

#include <common.h>
#include <init.h>
#include <time.h>
#include <asm/global_data.h>
#include <asm/arch/rbus/crt_sys_reg.h>
#include <asm/arch/io.h>
#include <asm/arch/ddr.h>

DECLARE_GLOBAL_DATA_PTR;

#ifdef CONFIG_CLOCKS

#define set_SYS_PLL_reg(reg,data)   (*((volatile unsigned int*) reg)=data)
#define get_SYS_PLL_reg(reg)   (*((volatile unsigned int*) reg))
#define PLLSCPU		0x9801D700

unsigned int get_scpu_pll_frequency(unsigned int reg)
{
	int wait_count = 0; /*For getting pll frequency*/
	unsigned int val, freq_pll = 0;
	set_SYS_PLL_reg((uintptr_t)reg,0x0); //reset all
	set_SYS_PLL_reg((uintptr_t)reg,0x20000); //reg_rstn_b=1
	set_SYS_PLL_reg((uintptr_t)reg,0x30000); //reg_rstn_counter_b=1
	while(wait_count < 10000){
		val = get_SYS_PLL_reg((uintptr_t)(reg+0x8));
		/*Wait (reg+8)[0] =1*/
		if ( (val & 0x1) == 0x1){
			freq_pll = ( (val >> 1) & 0x0001FFFF ) / 10;
			/*Read (reg+8)[17:1] value and divide 10*/
			wait_count = 0;
			break;
		}
		wait_count++;
	}
	return freq_pll;
}

int get_scpu_divide_freq(unsigned int pll_div)
{
	unsigned int tmp_pll_div = (pll_div & 0xFC);
	switch(tmp_pll_div)
	{
		case 0xB4:
			return 13;
		case 0xA8:
			return 10;
		case 0xA0:
			return 8;
		case 0x9C:
			return 7;
		case 0x98:
			return 6;
		case 0x94:
			return 5;
		case 0x8C:
			return 3;
		default:
			tmp_pll_div = (pll_div & 0x83);
			if (tmp_pll_div == 0x3)
				return 4;
			else if (tmp_pll_div == 0x2)
				return 2;
			else if (tmp_pll_div == 0)
				return 1;
			else
				return 1;
	}
}

#define DC0_DPI_DLL_CRT_SSC3_reg	0x9801B0E4
#define DC1_DPI_DLL_CRT_SSC3_reg	0x9800F028
#define DCPHY_DPI_DLL_CRT_SSC3_get_DPI_N_CODE_T(data)   ((0xFF000000&(data))>>24)

static unsigned long do_bdinfo_ddr_speed(void)
{
	unsigned long ddr_speed = 0;
	int i;

	for( i = 0; i < MAX_DC_COUNT; i++){
		uint ddr_speed_setting;
		uint ddr_mt = 0;
		if( i == 0 )
			ddr_speed_setting = DCPHY_DPI_DLL_CRT_SSC3_get_DPI_N_CODE_T(rtd_inl(DC0_DPI_DLL_CRT_SSC3_reg));
		else
			ddr_speed_setting = DCPHY_DPI_DLL_CRT_SSC3_get_DPI_N_CODE_T(rtd_inl(DC1_DPI_DLL_CRT_SSC3_reg));

		switch (ddr_speed_setting) {
			case 0x39 ... 0xff:
				ddr_mt = 3201;
				break;
			case 0x34 ... 0x38:
				ddr_mt = 3200;
				break;
			case 0x2f ... 0x33:
				ddr_mt = 2933;
				break;
			case 0x2a ... 0x2e:
				ddr_mt = 2666;
				break;
			case 0x25 ... 0x29:
				ddr_mt = 2400;
				break;
			case 0x20 ... 0x24:
				ddr_mt = 2133;
				break;
			case 0x1b ... 0x1f:
				ddr_mt = 1866;
				break;
			case 0x16 ... 0x1a:
				ddr_mt = 1600;
				break;
			case 0x15:
				ddr_mt = 1333;
				break;
			case 0x0 ... 0x14 :
				ddr_mt = 1332;
				break;
			default:	ddr_mt = 0;	break;
		}
		printf("DC%d DDR: %u MT/s\n", i, ddr_mt);
                ddr_speed = (i == 0) ? ddr_mt : (ddr_mt < ddr_speed) ? ddr_mt : ddr_speed;
	}

	return ddr_speed;
}

int set_cpu_clk_info(void)
{
	int divide_freg;
	unsigned int freq;

	freq = get_scpu_pll_frequency(PLLSCPU);
	divide_freg = get_scpu_divide_freq(((get_SYS_PLL_reg(SYS_PLL_DIV_reg) >> 6) & 0xFF));

	gd->bd->bi_arm_freq = freq / divide_freg;
	gd->bd->bi_ddr_freq = do_bdinfo_ddr_speed();
	gd->bd->bi_dsp_freq = 0;

	return 0;
}
#endif
