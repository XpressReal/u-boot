#include <command.h>
#include <pmic.h>

#define IDENTV(_ident) (is-ident)
const char __is[] = "            ";
const char *is = __is + 12;

static void show_usage(void)
{
	printf("PMIC command help\n");
	printf("   pmic                              - show this message\n");
	printf("   pmic probe <name> <i2c> <addr>    - probe a device\n");
	printf("   pmic list                         - list available devices\n");
	printf("   pmic *                            - show (entry, value) of all devices\n");
	printf("   pmic <dev>                        - test <dev>\n");
	printf("   pmic <dev> list                   - list available items\n");
	printf("   pmic <dev> *                      - show (entry, value) of <dev>\n");
	printf("   pmic <dev> <item>                 - test <item> of <dev>\n");
	printf("   pmic <dev> <item> list            - list available options\n");
	printf("   pmic <dev> <item> get             - get item value\n");
	printf("   pmic <dev> <item> set <option>    - set item to value\n");
	printf("--\n");
}

static void list_available_devices(void)
{
	int i;
	struct pmic_device *dev;

	printf("available device:\n");
	for (i = 0; (dev = pmic_cmd_device_find_by_index(i)) != NULL; i++)
		printf("   %-9s -> %s\n", dev->name, dev->full_name);
	printf("--\n");
}

static void list_available_entries(struct pmic_device *dev)
{
	int i;

	printf("available items:\n");
	for (i = 0; i < dev->n_ents; i++)
		printf("   %s\n",  dev->ents[i].name);
	printf("--\n");
}

static const struct pmic_entry *find_pmic_entry(struct pmic_device *dev, const char *name)
{
	int i;

	for (i = 0; i < dev->n_ents; i++)
		if (!strcmp(dev->ents[i].name, name))
			return &dev->ents[i];
	return NULL;
}

static void list_available_options(const struct pmic_entry *ent)
{
	int i;
	char buf[40];
	int ret;

	printf("available options:\n");
	for (i = 0; i < ent->size; i++) {
		ret = pmic_entry_val_to_str(ent, i, buf, sizeof(buf));

		if (!ret)
			printf("   %d: %s\n", i,  buf);
#ifdef DEBUG
		if (!ret) {
			unsigned char val;
			ret = pmic_entry_str_to_val(ent, buf, &val);
			printf("[D]%d: val=%d\n", i, ret ? -1 : val);
		}
#endif
	}
}

static void show_one_entry(int ident, struct pmic_device *dev, const struct pmic_entry *ent);

static void set_one_entry(struct pmic_device *dev, const struct pmic_entry *ent, const char *str)
{
	unsigned char val;
	int ret;

	ret = pmic_entry_str_to_val(ent, str, &val);
	if (ret) {
		printf("%s: failed to parse '%s'\n", ent->name, str);
		return;
	}

	ret = pmic_entry_write(dev, ent, val);
	if (ret)
		printf("%s: faile to write %s[%d]\n", ent->name, str, val);
	else {
		printf("%s: set to %s[%d]\n", ent->name, str, val);
		show_one_entry(0, dev, ent);
	}
}

static void show_one_entry(int ident, struct pmic_device *dev, const struct pmic_entry *ent)
{
	int ret;
	char buf[40] = ".";
	unsigned char val = 0xff;

	ret = pmic_entry_read(dev, ent, &val);
	if (!ret)
		ret = pmic_entry_val_to_str(ent, val, buf, sizeof(buf));
	printf("%s%s: %s[%d]\n", IDENTV(ident), ent->name, buf, val);
}

static void show_one_device(int ident, struct pmic_device *dev)
{
	int i;

	printf("%sdevice: %s\n", IDENTV(ident), dev->name);
	for (i = 0; i < dev->n_ents; i++) {
		show_one_entry(ident + 3, dev, &dev->ents[i]);
	}
}

static void show_all_devices(int ident)
{
	int i;
	struct pmic_device *dev;

	printf("%sall devices:\n", IDENTV(ident));
	for (i = 0; (dev = pmic_cmd_device_find_by_index(i)) != NULL; i++) {
		show_one_device(ident+3, dev);
	}
	printf("--\n");
}

static int probe_device(char * const argv[])
{
	const char *name = _A(argv[0]);
	int arg0;
	int arg1;

	arg0 = (int)strtoul(_A(argv[1]), NULL, 16);
	arg1 = (int)strtoul(_A(argv[2]), NULL, 16);

	printf("probe device name=%s,arg0=%d,arg1=%d\n", name, arg0, arg1);
	return pmic_device_probe(name, arg0, arg1) ? CMD_RET_FAILURE : 0;
}

int do_pmic(struct cmd_tbl *cmdtp, int flag, int argc, char * const argv[])
{
	struct pmic_device *dev;
	const struct pmic_entry *ent;

	if (argc < 2) {
		show_usage();
		list_available_devices();
		return 0;
	}

	if (_A(argv[1])[0] == '*') {
		show_all_devices(0);
		return 0;
	}

	if (!strcmp(_A(argv[1]), "probe")) {
		if (argc != 5) {
			show_usage();
			return 0;
		}
		return probe_device(argv + 2);
	}

	if (!strcmp(_A(argv[1]), "list")) {
		list_available_devices();
		return 0;
	}

	dev = pmic_cmd_device_find(_A(argv[1]));
	if (!dev)
		return CMD_RET_FAILURE;
	if (argc < 3)
		return 0;

	if (_A(argv[2])[0] ==  '*') {
		show_one_device(0, dev);
		return 0;
	}

	if (!strcmp(_A(argv[2]), "list")) {
		list_available_entries(dev);
		return 0;
	}

	ent = find_pmic_entry(dev, _A(argv[2]));
	if (!ent)
		return CMD_RET_FAILURE;
	if (argc < 4)
		return 0;

	switch (_A(argv[3])[0]) {
	case 'l':
		list_available_options(ent);
		return 0;
	case 's':
		if (argc < 5)
			break;
		set_one_entry(dev, ent, _A(argv[4]));
		return 0;
	case 'g':
		show_one_entry(0, dev, ent);
		return 0;
	}
	show_usage();
	return 0;
}

U_BOOT_CMD(pmic, 6, 0, do_pmic, "pmic command", "");
