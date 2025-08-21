#include <common.h>
#include <linux/libfdt.h>
#include <rtk_opp.h>

int rtk_opp_mark_fdt_updated(void *fdt, const char *name)
{
	int offset;
	char buf[40];
	const void *p;
	int len;

	offset = fdt_path_offset(fdt, "/cpu-dvfs");
	if (offset < 0) {
		printf("%s: invalid fdt path '/cpu-dvfs'\n", __func__);
		return RTK_OPP_ERROR;
	}

	snprintf(buf, sizeof(buf), "%s,opp-updated", name);

	p = fdt_getprop(fdt, offset, buf, &len);
	if (!p || len == 0)
		return RTK_OPP_ERROR;

	fdt_setprop_u32(fdt, offset, buf, 1);
	return 0;
}

int rtk_opp_update_fdt_table(void *fdt, const char *name, struct rtk_opp_data *data)
{
	int table_offset;
	int offset;
	const void *p;
	int len;
	char buf[40];

	table_offset = fdt_path_offset(fdt, "/cpu-dvfs/cpu-opp-table");
	if (table_offset < 0) {
		table_offset = fdt_path_offset(fdt, "/cpu-dvfs/opp-table-cpu");
	}
	if (table_offset < 0) {
		printf("%s: invalid fdt path '/cpu-dvfs/cpu-opp-table'\n", __func__);
		return RTK_OPP_ERROR;
	}

	for (offset = fdt_first_subnode(fdt, table_offset); offset > 0;
	     offset = fdt_next_subnode(fdt, offset)) {
		uint64_t freq_hz;
		uint32_t freq_mhz;
		uint32_t volt_uv;
		uint32_t freq_adj;
		const char *node_name = fdt_get_name(fdt, offset, NULL);

		p = fdt_getprop(fdt, offset, "opp-hz", &len);
		if (len < 0) {
			printf("%s: %s: invalid 'opp-hz'\n", __func__, node_name);
			continue;
		}

		freq_hz = fdt64_to_cpu(*(const uint64_t *)p);
		freq_mhz = freq_hz / 1000000;

		/* sepcial case */
		freq_adj = (freq_hz / 1000) % 10;
		if (!freq_adj)
			freq_adj = freq_hz % 10;
		switch (freq_adj) {
		case 1:
			freq_mhz -= 100;
			break;
		case 2:
			freq_mhz -= 75;
			break;
		case 3:
			freq_mhz -= 66;
			break;
		case 4:
			freq_mhz -= 50;
			break;
		case 5:
			freq_mhz -= 33;
			break;
		case 6:
			freq_mhz -= 25;
		default:
			break;
		}

		snprintf(buf, sizeof(buf), "opp-microvolt-%s", name);
		p = fdt_getprop(fdt, offset, buf, &len);
		if (len < 0) {
			printf("%s: %s: no prop '%s'\n", __func__, node_name, buf);
			continue;
		}

		volt_uv = rtk_opp_evaluate_voltage(data, freq_mhz);

		printf("%s: %s: apply '%s' with %d\n", __func__, node_name, buf, volt_uv);
		fdt_setprop_u32(fdt, offset, buf, volt_uv);
	}

	return 0;
}

int rtk_opp_get_fdt_param(void *fdt, const char *name, struct rtk_opp_param *param)
{
	int offset;
	const void *p;
	int len;
	char buf[40];

	offset = fdt_path_offset(fdt, "/cpu-dvfs");
	if (offset < 0) {
		printf("%s: invalid fdt path '/cpu-dvfs'\n", __func__);
		return RTK_OPP_ERROR;
	}

	snprintf(buf, sizeof(buf), "%s,volt-correct", name);
	p = fdt_getprop(fdt, offset, buf, &len);
	if (p && len > 0) {
		int size = len / 4;
		int i;

		if (size > RTK_OPP_MAX_ENTRIES) {
			printf("%s: size of fss,volt-correct is greater than %d\n", __func__, RTK_OPP_MAX_ENTRIES);
			size = RTK_OPP_MAX_ENTRIES;
		}
		param->num_correct = size;
		for (i = 0; i < size; i++) {
			param->correct[i] = fdt32_to_cpu(*(const uint32_t *)(p + i * 4));
			printf("%s: fss,volt-correct[%d] = %d\n", __func__, i, param->correct[i]);
		}
	}

	param->step = 37500;
	snprintf(buf, sizeof(buf), "%s,volt-step", name);
	p = fdt_getprop(fdt, offset, buf, &len);
	if (p && len > 0)
		param->step = fdt32_to_cpu(*(const uint32_t *)p);

	snprintf(buf, sizeof(buf), "%s,volt-min", name);
	p = fdt_getprop(fdt, offset, buf, &len);
	if (p && len > 0)
		param->min = fdt32_to_cpu(*(const uint32_t *)p);

	snprintf(buf, sizeof(buf), "%s,volt-max", name);
	p = fdt_getprop(fdt, offset, buf, &len);
	if (p && len > 0)
		param->max = fdt32_to_cpu(*(const uint32_t *)p);

	param->round = 12500;
	snprintf(buf, sizeof(buf), "%s,volt-round", name);
	p = fdt_getprop(fdt, offset, buf, &len);
	if (p && len > 0)
		param->round = fdt32_to_cpu(*(const uint32_t *)p);

	printf("%s: step=%d, min=%d, max=%d, round=%d\n", __func__,
		param->step, param->min, param->max, param->round);

	return 0;
}
