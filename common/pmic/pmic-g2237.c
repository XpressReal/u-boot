/*
 * pmic-g2237.c
 *
 * Copyright (C) 2018 Realtek Semiconductor Corporation
 * Copyright (C) 2018 Cheng-Yu Lee <cylee12@realtek.com>
 *
 */
#include <pmic.h>
#include <g2237.h>

static const char *dc_nrmmode[] = { "auto", NULL, "force_pwm", "eco", };
static const char *dc_slpmode[] = { "auto", NULL, "eco", "shutdown", };
static const char *ldo_nrmmode[] = { "normal", NULL, "eco", NULL, };
static const char *ldo_slpmode[] = { "normal", NULL, "eco", "shutdown", };
static const unsigned int dc5_vtbl[] = {
	 800000,  850000,  900000,  950000,
	1000000, 1050000, 1100000, 1200000,
	1300000, 1500000, 1600000, 1700000,
	1800000, 1900000, 2000000, 2500000,
};

#define ENTRY_NRMVOLT_S(_name, _id, _sta, _ste, _num)       \
{                                                           \
	.name = _name,                                      \
	.reg  = G2237_REG_ ## _id ## _NRMVOLT,              \
	.mask = G2237_ ## _id ## _NRMVOLT_MASK,             \
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
	.reg  = G2237_REG_ ## _id ## _NRMVOLT,              \
	.mask = G2237_ ## _id ## _NRMVOLT_MASK,             \
	.type = PMIC_ENTRY_VALMAP_U32_ARRAY,                \
	.size = ARRAY_SIZE(_tbl),                           \
	.u32_array = {                                      \
		.array = _tbl,                              \
	},                                                  \
}
#define ENTRY_MODE(_name, _id, _x, _tbl)                    \
{                                                           \
	.name = _name,                                      \
	.reg  = G2237_REG_ ## _id ## _MODE,                 \
	.mask = G2237_ ## _id ## _ ## _x ## MODE_MASK,      \
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
	ENTRY_NRMVOLT_S("dc1_volt",  DC1,  2200000, 100000, 16),
	ENTRY_NRMVOLT_S("dc2_volt",  DC2,   800000,  12500, 32),
	ENTRY_NRMVOLT_S("dc3_volt",  DC3,   800000,  12500, 32),
	ENTRY_NRMVOLT_A("dc5_volt",  DC5, dc5_vtbl),
	ENTRY_NRMVOLT_S("ldo1_volt", LDO1, 2200000, 100000, 7),
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
};

static int g2237_probe(struct pmic_device *dev)
{
	unsigned char val;
	int ret;

	ret = pmic_read_reg(dev, G2237_REG_CHIP_ID, &val);
	if (ret || val != 0x25)
		goto error;
	ret = pmic_read_reg(dev, G2237_REG_VERSION, &val);
	if (ret)
		goto error;

	printf("g2237 rev%d\n", val & 0xF);
	return 0;
error:
	return PMIC_ERR;
}
DEFINE_PMIC_DRIVER(g2237, g2237_probe, entries);
