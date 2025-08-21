/*
 * This file is subject to the terms and conditions of the GNU General Public
 * License.  See the file "COPYING" in the main directory of this archive
 * for more details.
 *
 * Copyright (C) 2015 by Tom Ting <tom_ting@realtek.com>
 *
 */

#include <common.h>
#include <linux/sizes.h>
#include <asm/cache.h>
#include <asm/global_data.h>
#include <asm/armv8/mmu.h>
#include <asm/io.h>
#include <rtk/rtk_type.h>
#include <asm/arch/ddr.h>
#include <asm/arch/system.h>
#include <init.h>

#define DC_SECURE_MISC_reg		0x98008740

DECLARE_GLOBAL_DATA_PTR;

static struct mm_region rtd161xb_mem_map[] = {
	{
		/* RAM */
		.virt = 0,
		.phys = 0,
		.size = SZ_512M,
		.attrs = PTE_BLOCK_MEMTYPE(MT_NORMAL) |
			 PTE_BLOCK_INNER_SHARE
	}, {
#ifdef CONFIG_SPI_RTK_SFC
		/* SPI RBUS, NOR 32MB */
		.virt = 0x88100000,
		.phys = 0x88100000,
		.size = SZ_32M,
		.attrs = PTE_BLOCK_MEMTYPE(MT_DEVICE_NGNRE) |
			 PTE_BLOCK_NON_SHARE |
			 PTE_BLOCK_PXN | PTE_BLOCK_UXN
	}, {
#endif
		/* RBUS */
		.virt = RBUS_ADDR,
		.phys = RBUS_ADDR,
		//.size = RBUS_SIZE,
		.size = MMU_SECTION_SIZE,
		.attrs = PTE_BLOCK_MEMTYPE(MT_DEVICE_NGNRE) |
			 PTE_BLOCK_NON_SHARE |
			 PTE_BLOCK_PXN | PTE_BLOCK_UXN
	}, {
		/* List terminator */
		0,
	}
};

struct mm_region *mem_map = rtd161xb_mem_map;

/**
 * @brief dram_init_banksize
 *
 * @return 0
 */
int dram_init_banksize(void)
{
	int sizeGB = get_total_ddr_size()/8;

	gd->bd->bi_dram[0].start = gd->ram_base;
	gd->bd->bi_dram[0].size = get_effective_memsize();

	if (sizeGB >= 3) {
		gd->bd->bi_dram[1].start = 0x80020000;
		gd->bd->bi_dram[1].size = 0x080e0000;
		gd->bd->bi_dram[2].start = 0x8a100000;
		gd->bd->bi_dram[2].size = 0x0defc000;
		gd->bd->bi_dram[3].start = 0x98200000;
		gd->bd->bi_dram[3].size = 0x07e00000;
		gd->bd->bi_dram[4].start = 0xa0300000;
		gd->bd->bi_dram[4].size = 0x1fd00000;
	}
	if (sizeGB >= 4) {
		//gd->bd->bi_dram[3].start = 0x98200000;
		gd->bd->bi_dram[3].size = 0x00df0000;
		gd->bd->bi_dram[4].start = 0x99000000;
		gd->bd->bi_dram[4].size = 0x07000000;
		gd->bd->bi_dram[5].start = 0xa0200000;
		gd->bd->bi_dram[5].size = 0x5ee00000;
	}

	return 0;
}

int dram_init(void)
{
	phys_size_t size = (get_total_ddr_size() * 1024UL) / 8 * SZ_1M;

	/* linear range in [0, 2GB] */
	if (size > (phys_size_t)SZ_2G)
		size = SZ_2G;

	gd->ram_base = 0x00040000;
	gd->ram_size = size - gd->ram_base;

#if !CONFIG_IS_ENABLED(SYS_DCACHE_OFF)
	/* Update RAM mapping */
	rtd161xb_mem_map[0].virt = gd->ram_base;
	rtd161xb_mem_map[0].phys = gd->ram_base;
	rtd161xb_mem_map[0].size = gd->ram_size;
#endif	/* !CONFIG_IS_ENABLED(SYS_DCACHE_OFF) */

	return 0;
}

phys_size_t get_effective_memsize(void)
{
/* In Realtek design, 0xFF000000 is starting reserved for cpu register. */
#define MAX_EFFECTIVE_SIZE 0xFF000000
	if(gd->ram_size > MAX_EFFECTIVE_SIZE)
		return MAX_EFFECTIVE_SIZE;
	else
		return gd->ram_size;
}
