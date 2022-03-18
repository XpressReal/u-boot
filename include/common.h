/* SPDX-License-Identifier: GPL-2.0+ */
/*
 * Common header file for U-Boot
 *
 * This file still includes quite a few headers that should be included
 * individually as needed. Patches to remove things are welcome.
 *
 * (C) Copyright 2000-2009
 * Wolfgang Denk, DENX Software Engineering, wd@denx.de.
 */

#ifndef __COMMON_H_
#define __COMMON_H_	1

#ifndef __ASSEMBLY__		/* put C only stuff in this section */
#include <config.h>
#include <errno.h>
#include <time.h>
#include <linux/types.h>
#include <linux/string.h>
#include <stdarg.h>
#include <stdio.h>
#include <linux/kernel.h>
#include <asm/u-boot.h> /* boot information for Linux kernel */
#include <vsprintf.h>
#endif	/* __ASSEMBLY__ */

/* Pull in stuff for the build system */
#ifdef DO_DEPS_ONLY
# include <env_internal.h>
#endif

/* quickly print debug line */

#define VT100_NONE          "\033[m"
#define VT100_NONE_NL       "\033[m\n"
#define VT100_RED           "\033[0;32;31m"
#define VT100_LIGHT_RED     "\033[1;31m"
#define VT100_GREEN         "\033[0;32;32m"
#define VT100_LIGHT_GREEN   "\033[1;32m"
#define VT100_BLUE          "\033[0;32;34m"
#define VT100_LIGHT_BLUE    "\033[1;34m"
#define VT100_DARY_GRAY     "\033[1;30m"
#define VT100_CYAN          "\033[0;36m"
#define VT100_LIGHT_CYAN    "\033[1;36m"
#define VT100_PURPLE        "\033[0;35m"
#define VT100_LIGHT_PURPLE  "\033[1;35m"
#define VT100_BROWN         "\033[0;33m"
#define VT100_YELLOW        "\033[1;33m"
#define VT100_LIGHT_GRAY    "\033[0;37m"
#define VT100_WHITE         "\033[1;37m"

#define DDDD(fmt, args...) \
		printf(VT100_YELLOW "%s %d: " fmt VT100_NONE_NL, __FUNCTION__, __LINE__, ##args);

#define DDDDYELLOW(fmt, args...) \
		printf(VT100_YELLOW fmt VT100_NONE, ##args);

#define DDDDRED(fmt, args...) \
		printf(VT100_LIGHT_RED fmt VT100_NONE, ##args);

#define DDDDGREEN(fmt, args...) \
		printf(VT100_LIGHT_GREEN fmt VT100_NONE, ##args);

#endif	/* __COMMON_H_ */
