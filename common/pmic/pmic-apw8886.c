/*
 * pmic-apw8886.c
 *
 * Copyright (C) 2018 Realtek Semiconductor Corporation
 * Copyright (C) 2018 Cheng-Yu Lee <cylee12@realtek.com>
 *
 */
#include <pmic.h>
#include <apw8886.h>

static const char *dc_nrmmode[] = { "auto", NULL, "force_pwm", "eco", };
static const char *dc_slpmode[] = { "auto", NULL, "eco", "shutdown", };
static const char *ldo_nrmmode[] = { "normal", NULL, "eco", NULL, };
static const char *ldo_slpmode[] = { "normal", NULL, "eco", "shutdown", };
static const char *dc_clamp[] = { "disable", "enable" };
static const char *lpoff_to_do[] = { "keep", "restart" };
static const char *enlpoff[] = { "disable", "enable" };
static const char *time_lpoff[] = { "6s", "7s", "8s", "10s" };

#define ENTRY_VOLT(_name, _id, _x, _sta, _ste, _num)        \
{                                                           \
	.name = _name,                                      \
	.reg  = APW8886_REG_ ## _id ## _ ## _x ## VOLT,     \
	.mask = APW8886_ ## _id ## _ ## _x ## VOLT_MASK,    \
	.type = PMIC_ENTRY_VALMAP_U32_STEP,                 \
	.size = _num,                                       \
	.u32_step = {                                       \
		.start = _sta,                              \
		.step  = _ste,                              \
	},                                                  \
}
#define ENTRY_NRMVOLT(_name, _id, _sta, _ste, _num) \
	ENTRY_VOLT(_name, _id, NRM, _sta, _ste, _num)
#define ENTRY_SLPVOLT(_name, _id, _sta, _ste, _num) \
	ENTRY_VOLT(_name, _id, SLP, _sta, _ste, _num)
#define ENTRY_MODE(_name, _id, _x, _tbl)                    \
{                                                           \
	.name = _name,                                      \
	.reg  = APW8886_REG_ ## _id ## _MODE,               \
	.mask = APW8886_ ## _id ## _ ## _x ## MODE_MASK,    \
	.type = PMIC_ENTRY_VALMAP_STRING_ARRAY,             \
	.size = ARRAY_SIZE(_tbl),                           \
	.string_array = {                                   \
		.array = _tbl,                              \
	},                                                  \
}
#define ENTRY_NRMMODE(_name, _id, _tbl) \
        ENTRY_MODE(_name, _id, NRM, _tbl)
#define ENTRY_SLPMODE(_name, _id, _tbl) \
        ENTRY_MODE(_name, _id, SLP, _tbl)

#define ENTRY_CLAMP(_name, _id, _tbl)                       \
{                                                           \
	.name = _name,                                      \
	.reg  = APW8886_REG_CLAMP,                          \
	.mask = APW8886_ ## _id ## _CLAMP_MASK,             \
	.type = PMIC_ENTRY_VALMAP_STRING_ARRAY,             \
	.size = ARRAY_SIZE(_tbl),                           \
	.string_array = {                                   \
		.array = _tbl,                              \
	},                                                  \
}

#define ENTRY_OPT(_name, _reg, _mask, _tbl)                  \
{                                                           \
	.name = _name,                                      \
	.reg  = _reg,                                       \
	.mask = _mask,                                      \
	.type = PMIC_ENTRY_VALMAP_STRING_ARRAY,             \
	.size = ARRAY_SIZE(_tbl),                           \
	.string_array = {                                   \
		.array = _tbl,                              \
	},                                                  \
}

static const struct pmic_entry entries[] = {
	ENTRY_NRMVOLT("dc1_volt",  DC1,  2200000,  25000, 64),
	ENTRY_NRMVOLT("dc2_volt",  DC2,   550000,  12500, 64),
	ENTRY_CLAMP("dc2_clamp", DC2, dc_clamp),
	ENTRY_NRMVOLT("dc3_volt",  DC3,   550000,  12500, 64),
	ENTRY_CLAMP("dc3_clamp", DC3, dc_clamp),
	ENTRY_NRMVOLT("dc4_volt",  DC4,   800000,  20000, 64),
	ENTRY_NRMVOLT("ldo_volt",  LDO1, 1780000,  40000, 32),
	ENTRY_NRMMODE("dc1_mode",  DC1,  dc_nrmmode),
	ENTRY_NRMMODE("dc2_mode",  DC2,  dc_nrmmode),
	ENTRY_NRMMODE("dc3_mode",  DC3,  dc_nrmmode),
	ENTRY_NRMMODE("dc4_mode",  DC4,  dc_nrmmode),
	ENTRY_NRMMODE("dc5_mode",  DC5,  dc_nrmmode),
	ENTRY_NRMMODE("ldo1_mode", LDO1, ldo_nrmmode),
	ENTRY_SLPMODE("dc1_slpmode",  DC1,  dc_slpmode),
	ENTRY_SLPMODE("dc2_slpmode",  DC2,  dc_slpmode),
	ENTRY_SLPMODE("dc3_slpmode",  DC3,  dc_slpmode),
	ENTRY_SLPMODE("dc4_slpmode",  DC4,  dc_slpmode),
	ENTRY_SLPMODE("dc5_slpmode",  DC5,  dc_slpmode),
	ENTRY_SLPMODE("ldo1_slpmode", LDO1, ldo_slpmode),
	{
		.name = "vfb5",
		.reg  = APW8886_REG_VFB5_REF_VOLT_DAC,
		.mask = 0xf,
		.type = PMIC_ENTRY_VALMAP_U32_STEP,
		.size = 16,
		.u32_step = {
			.start = 512500,
			.step  = 12500,
		},
	},
	ENTRY_OPT("lpoff_to_do", APW8886_REG_PWRKEY, 0x80, lpoff_to_do),
	ENTRY_OPT("enlpoff",     APW8886_REG_PWRKEY, 0x40, enlpoff),
	ENTRY_OPT("time_lpoff",  APW8886_REG_PWRKEY, 0x03, time_lpoff),
};

static int apw8886_probe(struct pmic_device *dev)
{
	unsigned char val;
	int ret;

	ret = pmic_read_reg(dev, APW8886_REG_CHIP_ID, &val);
	if (ret)
		goto error;
	printf("apw8886 chip_id=%02x\n", val);

	ret = pmic_read_reg(dev, APW8886_REG_VERSION, &val);
	if (ret)
		goto error;
	printf("apw8886 rev%d\n", val & 0xF);
	return 0;
error:
	return PMIC_ERR;
}
DEFINE_PMIC_DRIVER(apw8886, apw8886_probe, entries);
