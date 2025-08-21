#include <pmic.h>
#include <sy8824c.h>

static const char *mode_str[] = { "auto", "force_pwm" };

#define ENTRY_VOLT(_name, _gid, _sta, _ste, _num)           \
{                                                           \
        .name = _name,                                      \
	.reg  = SY8824C_REG_VSEL ## _gid,                   \
	.mask = SY8824C_NSEL ## _gid ## _MASK,              \
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
	.reg  = SY8824C_REG_VSEL ## _gid,                   \
	.mask = SY8824C_MODE ## _gid ## _MASK,              \
	.type = PMIC_ENTRY_VALMAP_STRING_ARRAY,             \
	.size = ARRAY_SIZE(_tbl),                           \
	.string_array = {                                   \
		.array = _tbl,                              \
	},                                                  \
}

static const struct pmic_entry entries[] = {
	ENTRY_VOLT("volt", 0, 762500, 12500, 64),
	ENTRY_MODE("mode", 0, mode_str),
};

static int sy8824c_probe(struct pmic_device *dev)
{
	unsigned char val;
	int ret;

	ret = pmic_read_reg(dev, SY8824C_REG_VSEL0, &val);
	if (ret)
		goto error;
	return 0;
error:
	return PMIC_ERR;
}
DEFINE_PMIC_DRIVER(sy8824c, sy8824c_probe, entries);
