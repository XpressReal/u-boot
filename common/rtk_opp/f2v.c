#include <common.h>
#include <rtk_opp.h>

int rtk_opp_init_data(struct rtk_opp_data *data)
{
	data->num_entries = 0;
	struct rtk_opp_param *param = &data->param;

	*param = (struct rtk_opp_param){0};
	return 0;
}

int rtk_opp_add_entry(struct rtk_opp_data *data, int f, int v)
{
	int i = data->num_entries;

	if (i >= RTK_OPP_MAX_ENTRIES)
		return RTK_OPP_ERROR;

	/* frequency should be descending */
	if (i != 0 && data->entries[i - 1].f >= f)
		return RTK_OPP_ERROR;

	data->entries[i].f = f;
	data->entries[i].v = v;
	data->num_entries += 1;
	return 0;
}

__maybe_unused
static struct rtk_opp_entry *rtk_opp_entry_lookup_eq(struct rtk_opp_data *data, int freq)
{
	int i;

	for (i = 0; i < data->num_entries; i++)
		if (data->entries[i].f == freq)
			return  &data->entries[i];
	return NULL;
}

static struct rtk_opp_entry *rtk_opp_entry_lookup_ge(struct rtk_opp_data *data, int freq)
{
	int i;

	for (i = 0; i < data->num_entries; i++)
		if (data->entries[i].f >= freq)
			return  &data->entries[i];
	return NULL;
}

static int rtk_opp_entry_is_head(struct rtk_opp_data *data, const struct rtk_opp_entry *entry)
{
	return entry == data->entries;
}

static int rtk_opp_entry_index(struct rtk_opp_data *data, const struct rtk_opp_entry *entry)
{
	return (entry - data->entries);
}

static int __volt(struct rtk_opp_data *data, const struct rtk_opp_entry *entry)
{
	struct rtk_opp_param *param = &data->param;
	int sel = param->num_correct == 1 ? 0 : rtk_opp_entry_index(data, entry);

	return entry->v + param->correct[sel];
}

static int __rtk_opp_evaluate_voltage(struct rtk_opp_data *data, int f)
{
	struct rtk_opp_param *param = &data->param;
	struct rtk_opp_entry *centry, *pentry;
	int cv, pv;

	centry = rtk_opp_entry_lookup_ge(data, f);
	if (!centry) {
		centry = &data->entries[data->num_entries - 1];
		cv = __volt(data, centry);

		return cv + (f - centry->f) * param->step / 100;
	}

	cv = __volt(data, centry);

	if (centry->f == f)
		return cv;

	if (rtk_opp_entry_is_head(data, centry))
		return cv - (centry->f - f) * param->step / 100;

	pentry = centry - 1;
	pv = __volt(data, pentry);
	return (cv - pv) * (f - pentry->f) / (centry->f - pentry->f) + pv;
}

#define ROUND_UP(_a, _b) (((_a) + (_b) - 1) / (_b) * (_b))

int rtk_opp_evaluate_voltage(struct rtk_opp_data *data, int f)
{
	struct rtk_opp_param *param = &data->param;
	int v = __rtk_opp_evaluate_voltage(data, f);

	if (v == 0)
		return 0;

	if (v > param->max)
		return 0;

	if (v <= param->min)
		return param->min;

	v = ROUND_UP(v - param->min, param->round) + param->min;
	return v;
}
