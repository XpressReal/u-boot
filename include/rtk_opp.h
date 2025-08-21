#ifndef __RTK_OPP_H
#define __RTK_OPP_H

#define RTK_OPP_OKAY     (0)
#define RTK_OPP_ERROR    (-1)

#define RTK_OPP_MAX_ENTRIES  (12)

struct rtk_opp_entry {
	int f;
	int v;
	int c;
};

struct rtk_opp_param {
	int max;
	int min;
	int round;
	int step;
	int num_correct;
	int correct[RTK_OPP_MAX_ENTRIES];
};

struct rtk_opp_data {
	struct rtk_opp_entry  entries[RTK_OPP_MAX_ENTRIES];
	int                   num_entries;
	struct rtk_opp_param  param;
};

int rtk_opp_mark_fdt_updated(void *fdt, const char *name);
int rtk_opp_update_fdt_table(void *fdt, const char *name, struct rtk_opp_data *data);
int rtk_opp_get_fdt_param(void *fdt,  const char *name, struct rtk_opp_param *param);

int rtk_opp_init_data(struct rtk_opp_data *data);
int rtk_opp_add_entry(struct rtk_opp_data *data, int f, int v);
int rtk_opp_evaluate_voltage(struct rtk_opp_data *data, int freq);

#endif
