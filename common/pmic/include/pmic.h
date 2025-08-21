#ifndef __PMIC_H
#define __PMIC_H

#include "pmic-platform.h"

#define PMIC_OK      0
#define PMIC_ERR    -1

enum {
	PMIC_ENTRY_VALMAP_U32_ARRAY,
	PMIC_ENTRY_VALMAP_U32_STEP,
	PMIC_ENTRY_VALMAP_STRING_ARRAY,
};

struct pmic_valmap_u32_array {
	const unsigned int *array;
};

struct pmic_valmap_u32_step {
	unsigned int start;
	unsigned int step;
};

struct pmic_valmap_string_array {
	const char **array;
};

struct pmic_entry {
	const char *name;
	unsigned int reg;
	unsigned int mask;
	unsigned int type;
	int size;
	union {
		struct pmic_valmap_u32_array    u32_array;
		struct pmic_valmap_u32_step     u32_step;
		struct pmic_valmap_string_array string_array;
	};
};

#define PMIC_NAME_LENGTH                40
#define PMIC_DEVICE_TYPE_I2C            0
#define PMIC_DEVICE_TYPE_PWM            1

struct pmic_device;

struct type_ops {
	int (*write)(struct pmic_device *, unsigned char, unsigned char);
	int (*read)(struct pmic_device *, unsigned char, unsigned char *);
};

struct pmic_device {
	char name[PMIC_NAME_LENGTH];
	char full_name[PMIC_NAME_LENGTH];
	const struct pmic_entry *ents;
	int n_ents;
	const struct type_ops *ops;
	void *private;

	/* pmic i2c */
	int i2c_ch;
	int i2c_addr;
};

int pmic_write_reg(struct pmic_device *dev, unsigned char reg, unsigned char val);
int pmic_read_reg(struct pmic_device *dev, unsigned char reg, unsigned char *val);

int pmic_entry_read(struct pmic_device *dev, const struct pmic_entry *ent, unsigned char *val);
int pmic_entry_write(struct pmic_device *dev, const struct pmic_entry *ent, unsigned char val);
int pmic_entry_val_to_str(const struct pmic_entry *ent, unsigned char val, char *str, int size);
int pmic_entry_str_to_val(const struct pmic_entry *ent, const char *str, unsigned char *val);

struct pmic_driver {
	const char *name;
	int (*probe)(struct pmic_device *);
	const struct pmic_entry *ents;
	int n_ents;
	int dev_cnt;
	int type;
};

int pmic_driver_register(struct pmic_driver *drv);
int pmic_device_probe(const char *name, int arg0, int arg1);

#define DEFINE_PMIC_DRIVER_WEAK(_name)            \
__WEAK void pmic_add_ ## _name(void)              \
{}

#define ADD_PMIC_DRIVER(_name)                    \
pmic_add_ ## _name()

#define DEFINE_PMIC_DRIVER(_name, _probe, _ents)  \
static struct pmic_driver _ ## _name ## _drv = {  \
	.name = # _name,                          \
	.probe = _probe,                          \
	.ents  = _ents,                           \
	.n_ents = ARRAY_SIZE(_ents),              \
};                                                \
void pmic_add_ ## _name(void)                     \
{                                                 \
	int ret;                                  \
	ret = pmic_driver_register(& _ ## _name ## _drv); \
	if (ret)                                  \
		printf("%s: failed to add driver %s\n", __func__, #_name); \
}

#define DEFINE_PMIC_PWM_DRIVER(_name, _probe)     \
static struct pmic_driver _ ## _name ## _drv = {  \
	.name = # _name,                          \
	.probe = _probe,                          \
	.type = PMIC_DEVICE_TYPE_PWM,             \
};                                                \
void pmic_add_ ## _name(void)                     \
{                                                 \
	int ret;                                  \
	ret = pmic_driver_register(& _ ## _name ## _drv); \
	if (ret)                                  \
		printf("%s: failed to add driver %s\n", __func__, #_name); \
}

void pmic_cmd_device_add(struct pmic_device *dev);
struct pmic_device *pmic_cmd_device_find(const char *name);
struct pmic_device *pmic_cmd_device_find_by_index(int i);

struct pmic_device *i2c_create_device(struct pmic_driver *drv, int ch, int addr);
struct pmic_device *pwm_dt_create_device(struct pmic_driver *drv);

#endif
