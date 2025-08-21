#include <common.h>
#include <command.h>
#include "bsv.h"

static int do_bsv(struct cmd_tbl *cmdtp, int flag, int argc,
                  char *const argv[])
{
	return bsv_opp_apply() ? CMD_RET_FAILURE : CMD_RET_SUCCESS;
}

U_BOOT_CMD(bsv, 1, 0, do_bsv, "fill voltage of BIST shmoo to OPP table", "");
