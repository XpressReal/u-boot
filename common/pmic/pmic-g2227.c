/*
 * pmic-g2227.c
 *
 * Copyright (C) 2017 Realtek Semiconductor Corporation
 * Copyright (C) 2017 Cheng-Yu Lee <cylee12@realtek.com>
 *
 */
#include <pmic.h>
#include <g2227.h>

static const char *dc_nrmmode[] = { "auto", NULL, "force_pwm", "eco", };
static const char *dc_slpmode[] = { "auto", NULL, "eco", "shutdown", };
static const char *ldo_nrmmode[] = { "normal", NULL, "eco", NULL, };
static const char *ldo_slpmode[] = { "normal_n", "normal_s", "eco", "shutdown", };
static const unsigned int ldo_vtbl[] = {
	800000,  850000,  900000,  950000, 1000000, 1100000, 1200000, 1300000,
	1500000, 1600000, 1800000, 1900000, 2500000, 2600000, 3000000, 3100000,
};


#define ENTRY_NRMVOLT_S(_name, _id, _sta, _ste, _num)         \
{                                                           \
	.name = _name,                                      \
	.reg  = G2227_REG_ ## _id ## _NRMVOLT,              \
	.mask = G2227_ ## _id ## _NRMVOLT_MASK,             \
	.type = PMIC_ENTRY_VALMAP_U32_STEP,                 \
	.size = _num,                                       \
	.u32_step = {                                       \
		.start = _sta,                              \
		.step  = _ste,                              \
	},                                                  \
}
#define ENTRY_NRMVOLT_A(_name, _id, _tbl)                   \
{                                                           \
	.name = _name,                                      \
	.reg  = G2227_REG_ ## _id ## _NRMVOLT,              \
	.mask = G2227_ ## _id ## _NRMVOLT_MASK,             \
	.type = PMIC_ENTRY_VALMAP_U32_ARRAY,                \
	.size = ARRAY_SIZE(_tbl),                           \
	.u32_array = {                                      \
		.array = _tbl,                              \
	},                                                  \
}
#define ENTRY_MODE(_name, _id, _x, _tbl)                   \
{                                                           \
	.name = _name,                                      \
	.reg  = G2227_REG_ ## _id ## _MODE,                 \
	.mask = G2227_ ## _id ## _ ## _x ## MODE_MASK,      \
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

static const struct pmic_entry entries[] = {
	ENTRY_NRMVOLT_S("dc1_volt",  DC1,  3000000, 100000, 4),
	ENTRY_NRMVOLT_S("dc2_volt",  DC2,   800000,  12500, 32),
	ENTRY_NRMVOLT_S("dc3_volt",  DC3,   800000,  12500, 32),
	ENTRY_NRMVOLT_S("dc5_volt",  DC5,   800000,  12500, 32),
	ENTRY_NRMVOLT_S("dc6_volt",  DC6,   800000,  12500, 32),
	ENTRY_NRMVOLT_A("ldo2_volt", LDO2, ldo_vtbl),
	ENTRY_NRMVOLT_A("ldo3_volt", LDO3, ldo_vtbl),
	ENTRY_NRMMODE("dc1_mode",  DC1,  dc_nrmmode),
	ENTRY_NRMMODE("dc2_mode",  DC2,  dc_nrmmode),
	ENTRY_NRMMODE("dc3_mode",  DC3,  dc_nrmmode),
	ENTRY_NRMMODE("dc4_mode",  DC4,  dc_nrmmode),
	ENTRY_NRMMODE("dc5_mode",  DC5,  dc_nrmmode),
	ENTRY_NRMMODE("dc6_mode",  DC6,  dc_nrmmode),
	ENTRY_NRMMODE("ldo2_mode", LDO2, ldo_nrmmode),
	ENTRY_NRMMODE("ldo3_mode", LDO3, ldo_nrmmode),
	ENTRY_SLPMODE("dc1_slpmode",  DC1,  dc_slpmode),
	ENTRY_SLPMODE("dc2_slpmode",  DC2,  dc_slpmode),
	ENTRY_SLPMODE("dc3_slpmode",  DC3,  dc_slpmode),
	ENTRY_SLPMODE("dc4_slpmode",  DC4,  dc_slpmode),
	ENTRY_SLPMODE("dc5_slpmode",  DC5,  dc_slpmode),
	ENTRY_SLPMODE("dc6_slpmode",  DC6,  dc_slpmode),
	ENTRY_SLPMODE("ldo2_slpmode", LDO2, ldo_slpmode),
	ENTRY_SLPMODE("ldo3_slpmode", LDO3, ldo_slpmode),
};

static int g2227_probe(struct pmic_device *dev)
{
	unsigned char val;

	/* workaround */
	pmic_read_reg(dev, G2227_REG_VERSION, &val);

	return 0;
}
DEFINE_PMIC_DRIVER(g2227, g2227_probe, entries);
