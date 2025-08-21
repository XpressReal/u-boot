#include <common.h>
#include <env.h>
#include <rtk_opp.h>
#include "bsv.h"

#define rtd_inl(_a)   (*((volatile unsigned int *)(unsigned long)(_a)))

static void *get_fdt_loadaddr(void)
{
	unsigned int val =  env_get_ulong("fdt_loadaddr", 10, 0);

        return (void *)(long)val;
}

static int bsv_volt(int val)
{
	return val * 10 * 1000;
}

static int bsv_opp_load_from_otp(struct rtk_opp_data *data)
{
	unsigned int val;

	val = rtd_inl(0x98032ADC);
	if (val == 0)
		return BSV_ERROR;

	rtk_opp_add_entry(data, 1400, bsv_volt((val >> 8) & 0xff));
	rtk_opp_add_entry(data, 1800, bsv_volt((val >> 24) & 0xff));

	return 0;
}

int bsv_opp_main(void *fdt)
{
	struct rtk_opp_data data;
	struct rtk_opp_param *param = &data.param;

	if (!fdt)
                return BSV_ERROR;

	rtk_opp_init_data(&data);

	if (rtk_opp_get_fdt_param(fdt, "bsv", param)) {
		printf("bsv: failed to get param from fdt\n");
		return BSV_ERROR;
	}

	if (bsv_opp_load_from_otp(&data)) {
		printf("bsv: failed to load opp from otp\n");
		return BSV_ERROR;
	}

	if (rtk_opp_update_fdt_table(fdt, "bsv", &data)) {
		printf("bsv: warning: opp table not updated\n");
		return 0;
	}

	if (rtk_opp_mark_fdt_updated(fdt, "bsv")) {
		printf("bsv: warning: opp not enabled\n");
		return 0;
	}

	return 0;
}

int bsv_opp_apply(void) {
	void *fdt = get_fdt_loadaddr();
	return bsv_opp_main(fdt);
}
