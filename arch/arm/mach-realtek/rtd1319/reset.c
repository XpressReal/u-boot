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
#include <asm/arch/rbus/mis_reg.h>
#include <asm/arch/io.h>

void  __attribute__((weak)) board_reset(void)
{
}

void reset_cpu()
{
	// trigger watchdog reset
	rtd_outl(MIS_TCWCR,0xa5);
	rtd_outl(MIS_TCWTR,0x1);
	rtd_outl(MIS_TCWOV,0);
	rtd_outl(MIS_TCWCR,0);

	while (1);
}
