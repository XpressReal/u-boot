/*
 * common API
 *
 * Copyright (C) 2016-2017 Realtek Semiconductor Corporation
 * Copyright (C) 2016-2017 Cheng-Yu Lee <cylee12@realtek.com>
 */
#ifndef __PMIC_PLATFORM_H
#define __PMIC_PLATFORM_H

#define UBOOT

#include <common.h>

#define _BIT(_n)      (1 << (_n))

#define __WEAK        __weak

#define strtoul simple_strtoul

#define atoi(_s) simple_strtoul(_s, NULL, 10)

#define _A(_a) (_a)

#endif
