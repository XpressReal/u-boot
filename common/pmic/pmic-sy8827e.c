#include <pmic.h>
#include <sy8827e.h>

static const char *mode_str[] = { "auto", "force_pwm" };

#define ENTRY_VOLT(_name, _gid, _sta, _ste, _num)           \
{                                                           \
        .name = _name,                                      \
	.reg  = SY8827E_REG_VSEL ## _gid,                   \
	.mask = SY8827E_NSEL ## _gid ## _MASK,              \
	.type = PMIC_ENTRY_VALMAP_U32_STEP,                 \
	.size = _num,                                       \
	.u32_step = {                                       \
		.start = _sta,                              \
		.step  = _ste,                              \
	},                                                  \
}
#define ENTRY_MODE(_name, _gid, _tbl)                       \
{                                                           \
	.name = _name,                                      \
	.reg  = SY8827E_REG_VSEL ## _gid,                   \
	.mask = SY8827E_MODE ## _gid ## _MASK,              \
	.type = PMIC_ENTRY_VALMAP_STRING_ARRAY,             \
	.size = ARRAY_SIZE(_tbl),                           \
	.string_array = {                                   \
		.array = _tbl,                              \
	},                                                  \
}

static const struct pmic_entry entries[] = {
	ENTRY_VOLT("volt", 0, 712500, 12500, 64),
	ENTRY_MODE("mode", 0, mode_str),
	ENTRY_VOLT("slpvolt", 1, 712500, 12500, 64),
	ENTRY_MODE("slpmode", 1, mode_str),
};

static int sy8827e_probe(struct pmic_device *dev)
{
	unsigned char val;
	int ret;

	ret = pmic_read_reg(dev, SY8827E_REG_ID0, &val);
	if (ret || val != 0x88)
		goto error;
	ret = pmic_read_reg(dev, SY8827E_REG_ID1, &val);
	if (ret)
		goto error;

	printf("sy8827e rev%d\n", val & 0xF);
	return 0;
error:
	return PMIC_ERR;
}
DEFINE_PMIC_DRIVER(sy8827e, sy8827e_probe, entries);
