/*
 * (C) Copyright 2002
 * Sysgo Real-Time Solutions, GmbH <www.elinos.com>
 * Marius Groeger <mgroeger@sysgo.de>
 *
 * (C) Copyright 2002
 * Sysgo Real-Time Solutions, GmbH <www.elinos.com>
 * Alex Zuepke <azu@sysgo.de>
 *
 * (C) Copyright 2002
 * Gary Jennejohn, DENX Software Engineering, <garyj@denx.de>
 *
 * (C) Copyright 2004
 * DAVE Srl
 * http://www.dave-tech.it
 * http://www.wawnet.biz
 * mailto:info@wawnet.biz
 *
 * (C) Copyright 2004 Texas Insturments
 *
 * SPDX-License-Identifier:	GPL-2.0+
 */

#include <common.h>
#include <asm/system.h>
#include <asm/arch/rbus/iso_reg.h>
#include <asm/arch/io.h>

void  __attribute__((weak)) board_reset(void)
{
}

void reset_cpu()
{
	unsigned int el;

	el = current_el();
	if (el == 3) {
		// trigger watchdog 0 scpu swc reset
		rtd_outl(ISO_TCW0CR_reg,0xa5);
		rtd_outl(ISO_TCW0TR_reg,0x1);
		rtd_outl(ISO_TCW0OV_reg,1);
		rtd_outl(ISO_TCW0CR_reg,0);
	} else {
		// trigger watchdog 1 scpu nwc reset
		rtd_outl(ISO_TCW1CR_reg,0xa5);
		rtd_outl(ISO_TCW1TR_reg,0x1);
		rtd_outl(ISO_TCW1OV_reg,1);
		rtd_outl(ISO_TCW1CR_reg,0);
	}
	while (1);
}
