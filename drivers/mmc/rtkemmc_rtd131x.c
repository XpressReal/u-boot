/*
 * This is a driver for the eMMC controller found in Realtek RTD1319
 * SoCs.
 *
 * Copyright (C) 2013 Realtek Semiconductors, All Rights Reserved.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *
 */

#include <asm/arch/rtkemmc.h>
#include <asm/arch/platform_lib/board/gpio.h>
#include <asm/arch/rbus/crt_reg.h>
#include <asm/arch/cpu.h>
#define __RTKEMMC_C__

unsigned char *dummy_512B;
unsigned char *ext_csd = NULL;
unsigned int emmc_cid[4]={0};
char *mmc_name = "RTD131x eMMC";
unsigned int *rw_descriptor;
unsigned int wait_for_complete(unsigned int cmd, unsigned int bIgnore);
void phase(u32 VP0, u32 VP1);

void wait_done(volatile UINT32 *addr, UINT32 mask, UINT32 value, unsigned int cmd){
	int n = 0;
	while (1)
	{
		if(((*addr) &mask) == value)
			break;

		if((cr_readw(EMMC_NORMAL_INT_STAT_R) & 0x8000)!=0) {
                        break;
                }

		if(n++ > 3000)
		{
			printf("Time out \n");
			printf("%s: cmd_idx=%d, addr=0x%08x, mask=0x%08x, value=0x%08x, pad_mux=0x%x, 0x98012030=0x%x, 0x98012032=0x%x\n",
                                __func__, cmd, PTR_U32(addr), mask, value, cr_readl(0x9804e000), cr_readw(EMMC_NORMAL_INT_STAT_R), cr_readw(EMMC_ERROR_INT_STAT_R));
			return;
		}
		mdelay(1);
	}
}

UINT32 rtkemmc_swap_endian(UINT32 input)
{
	UINT32 output;
	output = (input & 0xff000000)>>24|
			(input & 0x00ff0000)>>8|
			(input & 0x0000ff00)<<8|
			(input & 0x000000ff)<<24;
	return output;
}
/* mmc spec definition */
const unsigned int tran_exp[] = {
    10000, 100000, 1000000, 10000000,
    0,     0,      0,       0
};

const unsigned char tran_mant[] = {
    0,  10, 12, 13, 15, 20, 25, 30,
    35, 40, 45, 50, 55, 60, 70, 80,
};

const unsigned int tacc_exp[] = {
    1, 10, 100, 1000, 10000, 100000, 1000000, 10000000,
};

const unsigned int tacc_mant[] = {
    0,  10, 12, 13, 15, 20, 25, 30,
    35, 40, 45, 50, 55, 60, 70, 80,
};

const char *const state_tlb[11] = {
	"STATE_IDLE",
        "STATE_READY",
        "STATE_IDENT",
        "STATE_STBY",
        "STATE_TRAN",
        "STATE_DATA",
        "STATE_RCV",
        "STATE_PRG",
        "STATE_DIS",
        "STATE_BTST",
        "STATE_SLEEP"
};
const char *const bit_tlb[4] = {
    "1bit",
    "4bits",
    "8bits",
    "unknow"
};

const char *const clk_tlb[8] = {
    "30MHz",
    "40MHz",
    "49MHz",
    "49MHz",
    "15MHz",
    "20MHz",
    "24MHz",
    "24MHz"
};

/************************************************************************
 *  global variable
 ************************************************************************/
unsigned cr_int_status_reg;
unsigned char * sys_ext_csd_addr;
unsigned int rtkemmc_debug_msg;
unsigned int rtkemmc_off_error_msg_in_init_flow;
static e_device_type emmc_card;
static unsigned int sys_rsp17_addr;
static unsigned char* ptr_ext_csd;
extern int mmc_ready_to_use;
int swap_endian(UINT32 input);
int gCurrentBootDeviceType=0;
volatile unsigned int gCurrentBootMode=MODE_SD20;

void rtkemmc_host_reset(void);
int rtkemmc_stop_transmission( e_device_type * card , int bIgnore);
int rtkemmc_Stream( unsigned int cmd_idx,UINT32 blk_addr, UINT32 dma_addr, UINT32 dma_length, int bIgnore);
int rtkemmc_SendCMDGetRSP( struct rtk_cmd_info * cmd_info, unsigned int bIgnore);
int sample_ctl_switch(int cmd_idx);
void rtkemmc_set_pad_driving(unsigned int clk_drv, unsigned int cmd_drv, unsigned int data_drv, unsigned int ds_drv);


/**************************************************************************************
 * Hank mmc driver
 **************************************************************************************/
struct emmc_error_entry {
	unsigned int error;
	const char *msg;
};

struct emmc_error_entry emmc_error_table[] =  {
        {.error = EMMC_VENDOR_ERR3,  .msg = "VENDOR_ERR3"},
        {.error = EMMC_VENDOR_ERR2,  .msg = "VENDOR_ERR2"},
        {.error = EMMC_VENDOR_ERR1,  .msg = "VENDOR_ERR1"},
        {.error = EMMC_BOOT_ACK_ERR, .msg = "BOOT_ACK_ERR"},
        {.error = EMMC_RESP_ERR,  .msg = "RESP_ERR"},
        {.error = EMMC_TUNING_ERR, .msg = "TINING_ERR"},
        {.error = EMMC_ADMA_ERR,  .msg = "ADMA_ERR"},
        {.error = EMMC_AUTO_CMD_ERR, .msg = "AUTO_CMD_ERR"},
        {.error = EMMC_CUR_LMT_ERR, .msg = "Current Limit error"},
        {.error = EMMC_DATA_END_BIT_ERR,   .msg = "Data End bit error"},
        {.error = EMMC_DATA_CRC_ERR,   .msg = "Data CRC error"},
        {.error = EMMC_DATA_TOUT_ERR,   .msg = "Data Timeout error"},
        {.error = EMMC_CMD_IDX_ERR,   .msg = "Command Index error"},
        {.error = EMMC_CMD_END_BIT_ERR,   .msg = "Command End bit error"},
        {.error = EMMC_CMD_CRC_ERR,   .msg = "Command CRC error"},
        {.error = EMMC_CMD_TOUT_ERR,   .msg = "Command Timeout error"},
        {.error = 0, .msg = NULL}
};

UINT32 check_error(int bIgnore){
	unsigned int error;
	struct emmc_error_entry *p;

	error = cr_readw(EMMC_ERROR_INT_STAT_R);
	CP15ISB;
	sync();

	if(!bIgnore)
		printf("EMMC_ERROR: register 0x98012032= 0x%08x\n", error);

	if(error==0) return 0;

	for (p = emmc_error_table; p->error != 0; p++) {
		if ((error & p->error) == p->error) {
			if(!bIgnore)
				printf("eMMC Error: %s\n", p->msg);
		}
	}
	return error;
}

void setRandomMemory(unsigned long dmaddr, unsigned int length)
{
	int i=0;
	for(i=0;i<(length/4);i++)
	{
		*(u64 *)(dmaddr+(i*4)) = (u64)((i+0x78*0x80)*0x13579bdf);
		sync();
	}
	flush_cache(dmaddr, length);
	sync();
}

#if 0
int rtkemmc_data_sync( struct mmc * mmc )
{
    MMCPRINTF("*** %s %s %d\n", __FILE__, __FUNCTION__, __LINE__);

    mmc->rca = emmc_card.rca;
    mmc->high_capacity = emmc_card.sector_addressing ? 1 : 0;
	return 0;
}
#endif

static void set_cmd_info(e_device_type *card,struct mmc_cmd * cmd,
                         struct rtk_cmd_info * cmd_info,u32 opcode,u32 arg,u8 rsp_len)
{
    memset(cmd, 0, sizeof(struct mmc_cmd));
    memset(cmd_info, 0, sizeof(struct rtk_cmd_info));

    cmd->cmdidx         = opcode;
    cmd->cmdarg         = arg;
    cmd_info->cmd       = cmd;
    cmd_info->rsp_len   = rsp_len;
}


/*******************************************************
 *
 *******************************************************/
int rtkemmc_send_status(u32* resp, uint show_msg, int bIgnore)
{
	int err = 0;
	int bMalloc=0;
	e_device_type * card = NULL;

	MMCPRINTF("*** %s %s %d - rca : %08x, rca addr : %08x\n", __FILE__, __FUNCTION__, __LINE__, emmc_card.rca, &emmc_card.rca);

	sync();

	struct mmc_cmd cmd;
	struct rtk_cmd_info cmd_info;

	memset(&cmd, 0, sizeof(struct mmc_cmd));
	memset(&cmd_info, 0, sizeof(struct rtk_cmd_info));

	if (card == NULL)
	{
		bMalloc=1;
		card = (e_device_type*)malloc(sizeof( e_device_type));
	}

	set_cmd_info(card, &cmd, &cmd_info, MMC_SEND_STATUS, emmc_card.rca, 6);
	err = rtkemmc_SendCMDGetRSP(&cmd_info,bIgnore);

	if (bMalloc)
	{
		free(card);
		card = NULL;
	}

	if(err){
		if((show_msg)&&(!bIgnore))
		printf("MMC_SEND_STATUS fail\n");
	}else{
		UINT8 cur_state = R1_CURRENT_STATE(cmd.response[0]);
		*resp = cmd.response[0];
		if((show_msg)&&(!bIgnore))
		{
			printf("get cur_state= %s\n", state_tlb[cur_state]);
			printf("-------------------------\n");
			printf("R1 response 0x%08x:  \n",*resp);
			if(!((*resp) & (1<<7))){
				printf("switch ok!\n");
			}
			if((*resp) & (1<<8)){
				printf("ready for data!\n");
			}

			printf("-------------------------\n");
			printf("\n");
		}
	}

	return err;
}

#if 0
int rtkemmc_set_wp(u32 blk_addr, uint show_msg, int bIgnore)
{
    int err = 0;
	int bMalloc=0;
	e_device_type * card = NULL;
	u32 resp = 0;

    MMCPRINTF("*** %s %s %d - blk_addr : %08x\n", __FILE__, __FUNCTION__, __LINE__, blk_addr);

    sync();

	struct mmc_cmd cmd;
    struct rtk_cmd_info cmd_info;

    memset(&cmd, 0, sizeof(struct mmc_cmd));
    memset(&cmd_info, 0, sizeof(struct rtk_cmd_info));

    if (card == NULL)
    {
		bMalloc=1;
		card = (e_device_type*)malloc(sizeof( e_device_type));
    }

    set_cmd_info(card,&cmd,&cmd_info,
                 MMC_SET_WRITE_PROT,
                 blk_addr,
                 6);
    err = rtkemmc_SendCMDGetRSP(&cmd_info,bIgnore);

    if (bMalloc)
    {
		free(card);
		card = NULL;
    }
    if(err){
		if((show_msg)&&(!bIgnore))
		printf("MMC_SET_WRITE_PROT fail\n");
    }else{
        UINT8 cur_state = R1_CURRENT_STATE(cmd.response[0]);
		resp = cmd.response[0];
		if((show_msg)&&(!bIgnore))
		{
            printf("get cur_state=");
            printf(state_tlb[cur_state]);
            printf("\n");
		}
    }
	printf("The resp is %d.\n",resp);

    return err;
}

int rtkemmc_clr_wp(u32 blk_addr, uint show_msg, int bIgnore)
{
    int err = 0;
	int bMalloc=0;
	e_device_type * card = NULL;
	u32 resp = 0;

    MMCPRINTF("*** %s %s %d - blk_addr : %08x\n", __FILE__, __FUNCTION__, __LINE__, blk_addr);

    sync();

	struct mmc_cmd cmd;
    struct rtk_cmd_info cmd_info;

    memset(&cmd, 0, sizeof(struct mmc_cmd));
    memset(&cmd_info, 0, sizeof(struct rtk_cmd_info));

    if (card == NULL)
    {
		bMalloc=1;
		card = (e_device_type*)malloc(sizeof( e_device_type));
    }

    set_cmd_info(card,&cmd,&cmd_info,
                 MMC_CLR_WRITE_PROT,
                 blk_addr,
                 6);
    err = rtkemmc_SendCMDGetRSP(&cmd_info,bIgnore);

    if (bMalloc)
    {
		free(card);
		card = NULL;
    }
    if(err){
		if((show_msg)&&(!bIgnore))
		printf("MMC_CLR_WRITE_PROT fail\n");
    }else{
        UINT8 cur_state = R1_CURRENT_STATE(cmd.response[0]);
		resp = cmd.response[0];
		if((show_msg)&&(!bIgnore))
		{
            printf("get cur_state=");
            printf(state_tlb[cur_state]);
            printf("\n");
		}
    }
	printf("The resp is %d.\n",resp);

    return err;
}
#endif

int rtk_eMMC_read( unsigned int blk_addr, unsigned int data_size, unsigned int * buffer )
{
    int ret_err = 0;
    unsigned int total_blk_cont;
    unsigned int curr_xfer_blk_cont;
    unsigned int address;
    e_device_type * card = &emmc_card;
    UINT8 ret_state=0;
    UINT32 bRetry=0, resp = 0;
    UINT32 retry_cnt=0, retry_cnt1=0, retry_cnt2=0, retry_cnt3=0;
    int err1=0;

    RDPRINTF("\nemmc:%s blk_addr=0x%08x, data_size=0x%08x, buffer=0x%08x, addressing=%d\n", __FUNCTION__, blk_addr, data_size, buffer, card->sector_addressing);
    if( !mmc_ready_to_use ) {
         RDPRINTF("emmc: not ready to use\n");
    }

    total_blk_cont = data_size>>9;
    if( data_size & 0x1ff ) {
        total_blk_cont+=1;
    }

#ifndef FORCE_SECTOR_MODE
	if( card->sector_addressing ) {
		address = blk_addr;
	}
	else {
		address = blk_addr << 9;
	}
#else
    address = blk_addr;
#endif
#ifdef EMMC_SPLIT_BLK
	while( total_blk_cont ) {
		if( total_blk_cont > EMMC_MAX_XFER_BLKCNT ) {
			curr_xfer_blk_cont = EMMC_MAX_XFER_BLKCNT;
		}
		else {
			curr_xfer_blk_cont = total_blk_cont;
		}

		flush_cache((unsigned long)buffer, curr_xfer_blk_cont << 9);
        if (IS_SRAM_ADDR((uintptr_t)buffer))
		ret_err = -1; //cannot allow r/w to sram in bootcode
        else{
			if (curr_xfer_blk_cont == 1)
			ret_err = rtkemmc_Stream(MMC_READ_SINGLE_BLOCK, address, (unsigned int)(uintptr_t)buffer, curr_xfer_blk_cont << 9,0);
			else
				ret_err = rtkemmc_Stream(MMC_READ_MULTIPLE_BLOCK, address, (unsigned int)(uintptr_t)buffer, curr_xfer_blk_cont << 9,0);
        }
		if( ret_err ) {
			return 0;
		}
		total_blk_cont -= curr_xfer_blk_cont;
		buffer += (curr_xfer_blk_cont<<(9-2));
		address += curr_xfer_blk_cont;

		CP15ISB;
		sync();

		//EXECUTE_CUSTOMIZE_FUNC(0); // insert execute customer callback at here
	}

	total_blk_cont = data_size>>9;
    if( data_size & 0x1ff ) {
        total_blk_cont+=1;
    }
	return total_blk_cont;
#else
RETRY_RD_CMD:
    flush_cache((unsigned long)buffer, data_size);

	if (IS_SRAM_ADDR((uintptr_t)buffer))
		ret_err = -1; //cannot allow r/w to sram in bootcode
    else{
		if (total_blk_cont == 1)
		ret_err = rtkemmc_Stream(MMC_READ_SINGLE_BLOCK, address, buffer, total_blk_cont << 9,0);
		else
			ret_err = rtkemmc_Stream(MMC_READ_MULTIPLE_BLOCK, address, buffer, total_blk_cont << 9,0);
    }

    if (ret_err)
    {
        if (retry_cnt2++ < MAX_CMD_RETRY_COUNT*2)
	{
            sample_ctl_switch(-1);
            goto RETRY_RD_CMD;
	}
        return ret_err;
    }

    /* To wait status change complete */
    bRetry=0;
    retry_cnt=0;
    retry_cnt1=0;
    while(1)
    {
        err1 = rtkemmc_send_status(&resp,1,0);
        //1. if cmd sent error, try again
        if (err1)
        {
            sample_ctl_switch(-1);
            if (retry_cnt++ > MAX_CMD_RETRY_COUNT*2)
                return ERR_EMMC_SEND_STATUS_RETRY_FAIL;
            mdelay(1);
            continue;
        }
	ret_state = R1_CURRENT_STATE(resp);
        //2. get state
        if (ret_state != STATE_TRAN)
        {
            bRetry = 1;
            if (retry_cnt1++ > MAX_CMD_RETRY_COUNT*2)
                return ERR_EMMC_SEND_RW_CMD_RETRY_FAIL;
            rtkemmc_stop_transmission(&emmc_card,0);
            mdelay(1000);
        }
        else
        {
            //out peaceful
            if (bRetry == 0)
                return !ret_err ?  total_blk_cont : 0;
            else
            {
                retry_cnt2 = 0;
                if (retry_cnt3++ > MAX_CMD_RETRY_COUNT*2)
                    return ERR_EMMC_SEND_RW_CMD_RETRY_FAIL;
                goto RETRY_RD_CMD;
            }
        }
    }
#endif
	printf("The err1 is %d. The retry_cnt is %d. The retry_cnt1 is %d. The retry_cnt2 is %d. The retry_cnt3 is %d.\n",err1,retry_cnt,retry_cnt1,retry_cnt2,retry_cnt3);
	printf("The ret_state, bRetry, resp are %c, %d, %d\n.",ret_state,bRetry,resp);
	printf("The card addressing=%d\n",card->sector_addressing);

    return !ret_err ?  total_blk_cont : 0;
}

struct backupRegs {
	u32                     sdmasa_r;       //0x98012000
	u16                     blocksize_r;    //0x98012004
	u16                     blockcount_r;   //0x98012006
	u16                     xfer_mode_r;    //0x9801200c
	u8                      host_ctrl1_r;   //0x98012028
	u8                      pwr_ctrl_r;     //0x98012029
	u8                      bgap_ctrl_r;    //0x9801202a
	u16                     clk_ctrl_r;     //0x9801202c
	u8                      tout_ctrl_r;    //0x9801202e

	u16                     normal_int_stat_en_r;   //0x98012034
	u16                     error_int_stat_en_r;    //0x98012036
	u16                     normal_int_signal_en_r; //0x98012038
	u16                     error_int_signal_en_r;  //0x9801203a
	u16                     auto_cmd_stat_r;        //0x9801203c
	u16                     host_ctrl2_r;           //0x9801203e
	u32                     adma_sa_low_r;          //0x98012058
	u8                      mshc_ctrl_r;            //0x98012208
	u8                      ctrl_r;                 //0x9801222c
	u32                     other1;                 //0x98012420
	u32                     dummy_sys;              //0x9801242c
	u32                     dqs_ctrl1;              //0x98012498
};

static struct backupRegs backreg;

static void dump_emmc_register(void)
{
	backreg.sdmasa_r = cr_readl(EMMC_SDMASA_R);
	backreg.blocksize_r = cr_readw(EMMC_BLOCKSIZE_R);
	backreg.blockcount_r = cr_readw(EMMC_BLOCKCOUNT_R);
	backreg.xfer_mode_r = cr_readw(EMMC_XFER_MODE_R);
	backreg.host_ctrl1_r = cr_readb(EMMC_HOST_CTRL1_R);
	backreg.pwr_ctrl_r = cr_readb(EMMC_PWR_CTRL_R);
	backreg.bgap_ctrl_r = cr_readb(EMMC_BGAP_CTRL_R);
	backreg.clk_ctrl_r = cr_readw(EMMC_CLK_CTRL_R);
	backreg.tout_ctrl_r = cr_readb(EMMC_TOUT_CTRL_R);
	backreg.normal_int_stat_en_r = cr_readw(EMMC_NORMAL_INT_STAT_EN_R);
	backreg.error_int_stat_en_r = cr_readw(EMMC_ERROR_INT_STAT_EN_R);
	backreg.normal_int_signal_en_r = cr_readw(EMMC_NORMAL_INT_SIGNAL_EN_R);
	backreg.error_int_signal_en_r = cr_readw(EMMC_ERROR_INT_SIGNAL_EN_R);
	backreg.auto_cmd_stat_r = cr_readw(EMMC_AUTO_CMD_STAT_R);
	backreg.host_ctrl2_r = cr_readw(EMMC_HOST_CTRL2_R);
	backreg.adma_sa_low_r = cr_readl(EMMC_ADMA_SA_LOW_R);
	backreg.mshc_ctrl_r = cr_readb(EMMC_MSHC_CTRL_R);
	backreg.ctrl_r = cr_readb(EMMC_CTRL_R);
	backreg.other1 = cr_readl(EMMC_OTHER1);
	backreg.dummy_sys = cr_readl(EMMC_DUMMY_SYS);
	backreg.dqs_ctrl1 = cr_readl(EMMC_DQS_CTRL1);
}

static void restore_emmc_register(void)
{
	cr_writel(backreg.sdmasa_r,EMMC_SDMASA_R);
	cr_writew(backreg.blocksize_r,EMMC_BLOCKSIZE_R);
	cr_writew(backreg.blockcount_r,EMMC_BLOCKCOUNT_R);
	cr_writew(backreg.xfer_mode_r,EMMC_XFER_MODE_R);
	cr_writeb(backreg.host_ctrl1_r,EMMC_HOST_CTRL1_R);
	cr_writeb(backreg.pwr_ctrl_r,EMMC_PWR_CTRL_R);
	cr_writeb(backreg.bgap_ctrl_r,EMMC_BGAP_CTRL_R);
	cr_writew(backreg.clk_ctrl_r,EMMC_CLK_CTRL_R);
	cr_writeb(backreg.tout_ctrl_r,EMMC_TOUT_CTRL_R);
	cr_writew(backreg.normal_int_stat_en_r,EMMC_NORMAL_INT_STAT_EN_R);
	cr_writew(backreg.error_int_stat_en_r,EMMC_ERROR_INT_STAT_EN_R);
	cr_writew(backreg.normal_int_signal_en_r,EMMC_NORMAL_INT_SIGNAL_EN_R);
	cr_writew(backreg.error_int_signal_en_r,EMMC_ERROR_INT_SIGNAL_EN_R);
	cr_writew(backreg.auto_cmd_stat_r,EMMC_AUTO_CMD_STAT_R);
	cr_writew(backreg.host_ctrl2_r,EMMC_HOST_CTRL2_R);
	cr_writel(backreg.adma_sa_low_r,EMMC_ADMA_SA_LOW_R);
	cr_writeb(backreg.mshc_ctrl_r,EMMC_MSHC_CTRL_R);
	cr_writeb(backreg.ctrl_r,EMMC_CTRL_R);
	cr_writel(backreg.dummy_sys,EMMC_DUMMY_SYS);
	cr_writel(0x80|backreg.dqs_ctrl1,EMMC_DQS_CTRL1);
}

static void restore_emmc_l4_register(void)
{
	cr_writel(backreg.other1,EMMC_OTHER1);
}

void frequency(unsigned int  freq, unsigned int  div_ip){
	unsigned int tmp_val=0;

	tmp_val = (cr_readl(SYS_PLL_EMMC4) & 0x06);
	cr_writel(tmp_val, SYS_PLL_EMMC4);
	CP15ISB;
	sync();

	tmp_val = (cr_readl(SYS_PLL_EMMC3) & 0xffff)|(freq<<16);
	cr_writel(tmp_val, SYS_PLL_EMMC3);
	CP15ISB;
	sync();

	tmp_val = (cr_readl(SYS_PLL_EMMC4) | 0x01);
	cr_writel(tmp_val, SYS_PLL_EMMC4);
	CP15ISB;
	sync();

	udelay(100);

#if 1
	/*
	If system wants to change the emmc clock frequency, they should do the crt reset.
	the crt reset will reset the emmc ip registers.
	Threrfore,  user should back up those registers first and then restore.
	After changing the emmc frequency, system should wait until that pll is stable.
	Some system will open the l4 gated by default.
	At this moment, user should not restore the l4 gated value because they need to push out the emmc clock first.
	udelay 100 is to make sure that the emmc clock signal is correct
	finally, system will restore the l4 gated register setting for power saving.
	*/
	dump_emmc_register();
	cr_writel(cr_readl(SYS_PLL_EMMC1)&0xfffffffd, SYS_PLL_EMMC1);
        sync();
	tmp_val = cr_readl(0x98000054)&0xffefefff;
	tmp_val = tmp_val | 0x00202000;
	cr_writel(tmp_val,0x98000054);
	sync();
	cr_writel(cr_readl(0x98000454)&0xfffffffe,0x98000454);
	sync();

	udelay(3);

	cr_writel(cr_readl(0x98000454)|0x1,0x98000454);
	sync();
	cr_writel(cr_readl(0x98000054)|0x00303000,0x98000054);
	sync();

	udelay(3);

	restore_emmc_register();

	if(div_ip!=0) {
		cr_writel((((cr_readl(EMMC_CKGEN_CTL)&0xfffff00)|div_ip)|0x100), EMMC_CKGEN_CTL); //set the enable bit
	}

	cr_writel(cr_readl(SYS_PLL_EMMC1)|0x2, SYS_PLL_EMMC1);
	sync();

	wait_done((UINT32 *)EMMC_PLL_STATUS, 0x1, 0x1, 1000);  //1000 is the invalid command index passed to wait done function

	udelay(100);

	restore_emmc_l4_register();
#else
	cr_writel(cr_readl(EMMC_DUMMY_SYS)|0x00400000, EMMC_DUMMY_SYS); //reset counter
	cr_writel(cr_readl(EMMC_CKGEN_CTL)&0xfffffe00, EMMC_CKGEN_CTL); //reset to the initial value
	if(div_ip!=0) {
		cr_writel((((cr_readl(EMMC_CKGEN_CTL)&0xfffff00)|div_ip)|0x100), EMMC_CKGEN_CTL); //set the enable bit
	}

	cr_writel(cr_readl(SYS_PLL_EMMC1)&0xfffffffd, SYS_PLL_EMMC1);
	sync();

	cr_writel(cr_readl(EMMC_DUMMY_SYS)&0xffbfffff, EMMC_DUMMY_SYS); //release counter
	sync();
	//release reset pll
	cr_writel(cr_readl(SYS_PLL_EMMC1)|0x2, SYS_PLL_EMMC1);
	sync();

	wait_done((UINT32 *)EMMC_PLL_STATUS, 0x1, 0x1, 1000);  //1000 is the invalid command index passed to wait done function
	cr_writeb(0x06, EMMC_SW_RST_R);      //9801202f, Software Reset Register
	sync();

	wait_done((UINT32 *)0x9801202c, 0x6<<24, 0x0, 1000);

	cr_writel(cr_readl(EMMC_OTHER1)&0xffffffff7, EMMC_OTHER1);
	udelay(1);
	cr_writel(cr_readl(EMMC_OTHER1)|0x8, EMMC_OTHER1);
#endif
        printf("switch frequency to 0x%x, divder to 0x%x\n", freq, div_ip);
}

#if 0
/*******************************************************
 *
 *******************************************************/
UINT32 IsAddressSRAM(UINT32 addr)
{
    if ((addr<0x80000000) || (addr > 0x80007FFF))
        //ddr address
        return 0;
    else
        //secureRam addr
        return 1;
}
#endif
void print_ip_desc(void){
	printf("------------------------------>\n");
        printf("EMMC IP_DESC0 = 0x%08x\n", cr_readl(EMMC_IP_DESC0));
        printf("EMMC IP_DESC1 = 0x%08x\n", cr_readl(EMMC_IP_DESC1));
        printf("EMMC IP_DESC2 = 0x%08x\n", cr_readl(EMMC_IP_DESC2));
        printf("EMMC IP_DESC3 = 0x%08x\n", cr_readl(EMMC_IP_DESC3));

        printf("0x98012058 EMMC EMMC_ADMA_SA_LOW_R = 0x%08x\n------------------------------>\n", cr_readl(EMMC_ADMA_SA_LOW_R));
}

void make_ip_des(UINT32 dma_addr, UINT32 dma_length){
	UINT32* des_base = rw_descriptor;
	UINT32  blk_cnt;
	UINT32  blk_cnt2;
	UINT32  remain_blk_cnt;
	UINT32  tmp_val;
	unsigned int b1, b2;

	flush_cache((uintptr_t)rw_descriptor, 2*sizeof(unsigned int)*MAX_DESCRIPTOR_NUM);

	//CMD31 only transfer 8B
	if (dma_length < 512){
		cr_writel(dma_length, EMMC_BLOCKSIZE_R);
		blk_cnt = 1;

	}
	else{
		cr_writel(0x200, EMMC_BLOCKSIZE_R);
		blk_cnt  = dma_length>>9;
	}

	remain_blk_cnt  = blk_cnt;

	CP15ISB;
	sync();

	while(remain_blk_cnt) {
		if(remain_blk_cnt > EMMC_MAX_SCRIPT_BLK) {
			blk_cnt2 = EMMC_MAX_SCRIPT_BLK;
		}
		else {
			blk_cnt2 = remain_blk_cnt;
		}

		//boundary check
		b1 = dma_addr / 0x8000000;              //this eMMC ip dma transfer has 128MB limitation
		b2 = (dma_addr+blk_cnt2*512) / 0x8000000;
		if(b1 != b2) {
			blk_cnt2 = (b2*0x8000000-dma_addr) / 512;
		}

		if(dma_length<512) tmp_val = ((dma_length)<<16)|0x21;
		else tmp_val = ((blk_cnt2&0x7f)<<25)|0x21;

		if(remain_blk_cnt == blk_cnt2) {
			tmp_val |= 0x2;
		}

		des_base[0] = rtkemmc_swap_endian(dma_addr);       /* setting des2; Physical address to DMA to/from */
		des_base[1] = rtkemmc_swap_endian(tmp_val);

		dma_addr = dma_addr+(blk_cnt2<<9);
		remain_blk_cnt -= blk_cnt2;
		des_base += 2;

		CP15ISB;
		sync();
	}
	flush_cache((uintptr_t)rw_descriptor, 2*sizeof(unsigned int)*MAX_DESCRIPTOR_NUM);
	CP15ISB;
	sync();
}

static void rtkemmc_read_rsp(u32 *rsp, int reg_count)
{
	MMCPRINTF("rsp addr=0x%p; rsp_count=%u\n", rsp, reg_count);

	if ( reg_count==6 ){
		rsp[0] = rsp[1] = 0;
		rsp[0] = readl(EMMC_RESP01_R);
		MMCPRINTF("rsp[0]=0x%08x, rsp[1]=0x%08x\n",rsp[0],rsp[1]);
	}else if(reg_count==17){
		/*1. UNSTUFF_BITS uses the reverse order as: const int __off = 3 - ((start) / 32);
		  2. be32_to_cpu is called in mmc_send_csd as csd[i] = be32_to_cpu(csd_tmp[i]);*/
		//in hank eMMC IP, we neeed to rearrange  response in 17 bytes case because they save 8-135 bit instead of 0-127 bit
		u32 rsp_tmp[4]={0};
		rsp_tmp[3] = readl(EMMC_RESP01_R);
		rsp_tmp[2] = readl(EMMC_RESP23_R);
		rsp_tmp[1] = readl(EMMC_RESP45_R);
		rsp_tmp[0] = readl(EMMC_RESP67_R);
		rsp[3] = (rsp_tmp[3]&0x00ffffff)<<8;
		rsp[2] = ((rsp_tmp[2]&0x00ffffff)<<8) | ((rsp_tmp[3]&0xff000000)>>24);
		rsp[1] = ((rsp_tmp[1]&0x00ffffff)<<8) | ((rsp_tmp[2]&0xff000000)>>24);
		rsp[0] = ((rsp_tmp[0]&0x00ffffff)<<8) | ((rsp_tmp[1]&0xff000000)>>24);
		printf("rsp[0]=0x%08x, rsp[1]=0x%08x, rsp[2]=0x%08x, rsp[3]=0x%08x\n",rsp[0],rsp[1],rsp[2],rsp[3]);
	}
	else
		MMCPRINTF("rsp[0]=0x%08x\n",rsp[0]);
}

static int rtkemmc_set_rspparam(struct rtk_cmd_info *cmd_info)
{
	switch(cmd_info->cmd->cmdidx)
	{
	case MMC_GO_IDLE_STATE:
		cmd_info->cmd_para = (EMMC_NO_RESP);
		cmd_info->rsp_len = 6;
		cmd_info->cmd->cmdarg = 0x00000000;
		break;
	case MMC_SEND_OP_COND:
		cmd_info->cmd_para = (EMMC_RESP_LEN_48);
		cmd_info->cmd->cmdarg = MMC_SECTOR_ADDR|MMC_VDD_165_195;
		cmd_info->rsp_len = 6;
		break;
	case MMC_ALL_SEND_CID:
		cmd_info->cmd_para = (EMMC_RESP_LEN_136|EMMC_CMD_CHK_RESP_CRC);
		cmd_info->rsp_len = 17;
		cmd_info->cmd->cmdarg = 0x00000000;
		break;
	case MMC_SET_RELATIVE_ADDR:
		cmd_info->cmd_para = (EMMC_RESP_LEN_48|EMMC_CMD_CHK_RESP_CRC|EMMC_CMD_IDX_CHK_ENABLE);
		cmd_info->cmd->cmdarg = (1<<RCA_SHIFTER);
		cmd_info->rsp_len = 6;
		break;
	case MMC_SEND_CSD:
	case MMC_SEND_CID:
		cmd_info->cmd_para = (EMMC_RESP_LEN_136|EMMC_CMD_CHK_RESP_CRC);
		cmd_info->cmd->cmdarg = (1<<RCA_SHIFTER);
		cmd_info->rsp_len = 17;
		break;
	case MMC_SEND_EXT_CSD:
		cmd_info->cmd_para = (EMMC_RESP_LEN_48|EMMC_CMD_CHK_RESP_CRC|EMMC_CMD_IDX_CHK_ENABLE|EMMC_DATA);
		cmd_info->cmd->cmdarg = 0;
		cmd_info->rsp_len = 6;
		break;
	case MMC_SLEEP_AWAKE:
		cmd_info->cmd_para = (EMMC_RESP_LEN_48B|EMMC_CMD_CHK_RESP_CRC|EMMC_CMD_IDX_CHK_ENABLE);
		cmd_info->rsp_len = 6;
		printf("%s : cmd5 arg=0x%08x\n",__func__,cmd_info->cmd->arg);
		break;
	case MMC_SELECT_CARD:
		cmd_info->cmd->cmdarg = emmc_card.rca;
		printf("%s : cmd7 arg : 0x%08x\n",__func__,cmd_info->cmd->cmdarg);
		if (cmd_info->cmd->resp_type == (MMC_RSP_NONE | MMC_CMD_AC)) {
			printf("%s : cmd7 with rsp none\n",__func__);
			cmd_info->cmd_para = (EMMC_NO_RESP);
		}
		else {
			printf("%s : cmd7 with rsp\n",__func__);
			cmd_info->cmd_para = (EMMC_RESP_LEN_48|EMMC_CMD_CHK_RESP_CRC|EMMC_CMD_IDX_CHK_ENABLE);
		}
		cmd_info->rsp_len = 6;
		break;
	case MMC_SWITCH:
		cmd_info->cmd_para = (EMMC_RESP_LEN_48B|EMMC_CMD_CHK_RESP_CRC|EMMC_CMD_IDX_CHK_ENABLE);
		cmd_info->rsp_len = 6;
		break;
	case MMC_SEND_STATUS:
		cmd_info->cmd_para = (EMMC_RESP_LEN_48|EMMC_CMD_CHK_RESP_CRC|EMMC_CMD_IDX_CHK_ENABLE);
		cmd_info->cmd->cmdarg = (1<<RCA_SHIFTER);
		cmd_info->rsp_len = 6;
		break;
	case MMC_STOP_TRANSMISSION:
		cmd_info->cmd_para = (EMMC_RESP_LEN_48|EMMC_CMD_CHK_RESP_CRC|EMMC_CMD_IDX_CHK_ENABLE|(EMMC_ABORT_CMD<<6));
		cmd_info->rsp_len = 6;
		break;
	case MMC_SEND_TUNING_BLOCK_HS200:
		cmd_info->cmd_para = (EMMC_RESP_LEN_48|EMMC_CMD_CHK_RESP_CRC|EMMC_CMD_IDX_CHK_ENABLE|EMMC_DATA);
		cmd_info->cmd->cmdarg = 0;
		cmd_info->rsp_len = 6;
	case MMC_READ_MULTIPLE_BLOCK:
		cmd_info->cmd_para = (EMMC_RESP_LEN_48|EMMC_CMD_CHK_RESP_CRC|EMMC_CMD_IDX_CHK_ENABLE|EMMC_DATA);
		cmd_info->rsp_len = 6;
		break;
	case MMC_SET_BLOCK_COUNT:
		cmd_info->cmd_para = (EMMC_RESP_LEN_48|EMMC_CMD_CHK_RESP_CRC|EMMC_CMD_IDX_CHK_ENABLE);
		cmd_info->rsp_len = 6;
		break;
	case MMC_WRITE_MULTIPLE_BLOCK:
		cmd_info->cmd_para = (EMMC_RESP_LEN_48|EMMC_CMD_CHK_RESP_CRC|EMMC_CMD_IDX_CHK_ENABLE|EMMC_DATA);
		cmd_info->rsp_len = 6;
		break;
	case MMC_READ_SINGLE_BLOCK:
		cmd_info->cmd_para = (EMMC_RESP_LEN_48|EMMC_CMD_CHK_RESP_CRC|EMMC_CMD_IDX_CHK_ENABLE|EMMC_DATA);
		cmd_info->rsp_len = 6;
		break;
	case MMC_WRITE_BLOCK:
		cmd_info->cmd_para = (EMMC_RESP_LEN_48|EMMC_CMD_CHK_RESP_CRC|EMMC_CMD_IDX_CHK_ENABLE|EMMC_DATA);
		cmd_info->rsp_len = 6;
		break;
	case MMC_ERASE_GROUP_START:
	case MMC_ERASE_GROUP_END:
		cmd_info->cmd_para = (EMMC_RESP_LEN_48|EMMC_CMD_CHK_RESP_CRC|EMMC_CMD_IDX_CHK_ENABLE);
		cmd_info->rsp_len = 6;
		break;
	case MMC_ERASE:
		cmd_info->cmd_para = (EMMC_RESP_LEN_48B|EMMC_CMD_CHK_RESP_CRC|EMMC_CMD_IDX_CHK_ENABLE);
		cmd_info->rsp_len = 6;
		break;
	case MMC_GEN_CMD:
		if(cmd_info->cmd->cmdarg & 0x1) {   //read single block
			cmd_info->cmd->cmdarg = 0x1;
			cmd_info->cmd_para = (EMMC_RESP_LEN_48|EMMC_CMD_CHK_RESP_CRC|EMMC_CMD_IDX_CHK_ENABLE|EMMC_DATA);
			cmd_info->rsp_len = 6;
			break;
		}
		else {      //write single block
			cmd_info->cmd->cmdarg = 0x0;
			cmd_info->cmd_para = (EMMC_RESP_LEN_48|EMMC_CMD_CHK_RESP_CRC|EMMC_CMD_IDX_CHK_ENABLE|EMMC_DATA);
			cmd_info->rsp_len = 6;
			break;
		}
	default:
		printf("Realtek Unrecognized eMMC command!!!\n");
		cmd_info->cmd_para = 0;
		cmd_info->rsp_len = 6;
		break;
	}
	MMCPRINTF( "%s : cmd=0x%02x rsp_len=0x%02x arg=0x%08x para=0x%08x\n","rtkemmc", cmd_info->cmd->cmdidx, cmd_info->rsp_len,cmd_info->cmd->cmdarg,cmd_info->cmd_para);
	return 0;
}

int rtkemmc_wait_status(u32 *resp)
{
	int cnt = 0;
	int err = 0;

	do {
		err = rtkemmc_send_status(resp, 1, 1);
		if(err==0) {
			err = -9999;
			if(((*resp & 0x00001E00)>>9)==4 && (*resp&(1<<8))) {
				err = 0;
				break;
			}
			else
				break;
		}
		cnt++;
		mdelay(1);
	}while(cnt < 300);

	return err;
}

/*******************************************************
 *
 *******************************************************/
void error_handling(unsigned int cmd_idx, unsigned int bIgnore)
{
	int err = 0;
	int rty_cnt=0;
	int rty_cnt2=0;
	u32 resp = 0;

	cr_writew(0, EMMC_ERROR_INT_SIGNAL_EN_R);      //9801203a, enable all error SIGNAL Interrupt register
	CP15ISB;
	sync();

	if((cr_readw(EMMC_ERROR_INT_STAT_R)&
		(EMMC_AUTO_CMD_ERR|EMMC_CMD_IDX_ERR|EMMC_CMD_END_BIT_ERR|EMMC_CMD_CRC_ERR|EMMC_CMD_TOUT_ERR)) !=0){ //check cmd line
#ifdef MMC_DEBUG
		printf("CMD Line error occurs \n");
#endif
		cr_writeb(0x2, EMMC_SW_RST_R); //Perform a software reset
		wait_done((UINT32 *)0x9801202c, (0x2<<24), 0, cmd_idx);
	}
	if((cr_readw(EMMC_ERROR_INT_STAT_R)&
		(EMMC_ADMA_ERR|EMMC_DATA_END_BIT_ERR|EMMC_DATA_CRC_ERR|EMMC_DATA_TOUT_ERR)) !=0){ //check data line
#ifdef MMC_DEBUG
		printf("DAT Line error occurs \n");
#endif
	cr_writeb(0x4, EMMC_SW_RST_R); //Perform a software reset
			wait_done((UINT32 *)0x9801202c, (0x4<<24), 0x0, cmd_idx); //wait for clear 0x2f bit 2
	}

retry_L1:
	//synchronous abort: stop host dma
	cr_writeb(0x1, EMMC_BGAP_CTRL_R); //stop emmc transactions  <-------????????????????????????????
	wait_done((UINT32 *)EMMC_NORMAL_INT_STAT_R, 0x2, 0x2, cmd_idx); //wait for xfer complete
	cr_writew(0x2, EMMC_NORMAL_INT_STAT_R); //clear transfer complete status
retry_L2:
	//synchronous abort: issue abort coomand
	/*from eMMC Spec. stop command cannot be fired after the cmd 21*/
	if(cmd_idx!=MMC_SEND_TUNING_BLOCK_HS200) {
		rtkemmc_stop_transmission(&emmc_card, bIgnore);
		mdelay(1);

		err = rtkemmc_wait_status(&resp);
		if(err) {
			printf("%s: ret = %d, resp=0x%x from wait status !!!\n", __func__, err, resp);
			++rty_cnt2;
			if(rty_cnt2 < 100 && err == -9999)
				goto retry_L2;
			else
				printf("wait cmd13 ready failed...\n");
		}
	}

	cr_writeb(0x6, EMMC_SW_RST_R); //Perform a software reset
	wait_done((UINT32 *)0x9801202c, (0x6<<24), 0x0, cmd_idx); //wait for clear 0x2f bit 1 & 2
	wait_done((UINT32 *)EMMC_PSTATE_REG, 0x3, 0x0, cmd_idx); //wait for cmd and data lines are not in use

	udelay(40);

	if((readl(EMMC_PSTATE_REG) &0xf00000)!=0xf00000 || (readl(EMMC_PSTATE_REG) & 0xf0)!=0xf0) {
		if(rty_cnt%1000==999)
			printf("error_handling: stop command check, EMMC_PSTATE_REG=0x%x\n", readl(EMMC_PSTATE_REG));

		++rty_cnt;
		if(cmd_idx==MMC_SEND_TUNING_BLOCK_HS200)
			mdelay(1);
		if(rty_cnt<5000)
			goto retry_L1;
		else
			printf("wait pstate reg failed...\n");
	}
}

/*******************************************************
 *
 *******************************************************/
void mmc_show_ocr( void * ocr )
{
    struct rtk_mmc_ocr_reg *ocr_ptr;
    struct rtk_mmc_ocr *ptr = (struct rtk_mmc_ocr *) ocr;
    ocr_ptr = (struct rtk_mmc_ocr_reg *)&ptr->reg;

#ifdef THIS_IS_FLASH_WRITE_U_ENV
	prints("emmc: OCR\n");
	prints(" - start bit : ");
	print_val(ptr->start, 1);
	prints("\n");
	prints(" - transmission bit : ");
	print_val(ptr->transmission, 1);
	prints("\n");
	prints(" - check bits : ");
	print_val(ptr->check1, 2);
	prints("\n");
	prints(" - OCR register : \n");
	prints("   - 1.7v to 1.95v : ");
	print_val(ocr_ptr->lowV, 1);
	prints("\n   - 2.0v to 2.6v : ");
	print_val(ocr_ptr->val1, 2);
	prints("\n   - 2.7v to 3.6v : ");
	print_val(ocr_ptr->val2, 2);
	prints("\n   - access mode : ");
	print_val(ocr_ptr->access_mode, 1);
	prints("\n   - power up status : ");
	print_val(ocr_ptr->power_up, 1);
	prints("\n");
	prints(" - check bits : ");
	print_val(ptr->check2, 2);
	prints("\n");
	prints(" - end bit : ");
	print_val(ptr->end, 1);
	prints("\n");
#else
	MMCPRINTF("emmc: OCR\n");
	MMCPRINTF("- start bit : 0x%x\n", ptr->start);
	MMCPRINTF(" - transmission bit : 0x%x\n", ptr->transmission);
	MMCPRINTF(" - check bit : 0x%x\n", ptr->check1);
	MMCPRINTF(" - OCR register : \n");
	MMCPRINTF("   - 1.7v to 1.95v : 0x%x\n", ocr_ptr->lowV);
	MMCPRINTF("   - 2.0v to 2.6v : 0x%x\n", ocr_ptr->val1);
	MMCPRINTF("   - 2.7v to 3.6v : 0x%x\n", ocr_ptr->val2);
	MMCPRINTF("   - access mode : 0x%x\n", ocr_ptr->access_mode);
	MMCPRINTF("   - power up status : 0x%x\n", ocr_ptr->power_up);
	MMCPRINTF(" - check bits : 0x%x\n", ptr->check2);
	MMCPRINTF(" - end bits : 0x%x\n", ptr->end);
#endif

	printf("The power_up of ocr_ptr is 0x%x.\n",ocr_ptr->power_up);
}

/*******************************************************
 *
 *******************************************************/
int rtkemmc_stop_transmission( e_device_type * card , int bIgnore)
{
    struct mmc_cmd cmd;
    struct rtk_cmd_info cmd_info;
    int err = 0;
    int bMalloc=0;

    MMCPRINTF("%s : \n", __func__);

    memset(&cmd, 0, sizeof(struct mmc_cmd));
    memset(&cmd_info, 0, sizeof(struct rtk_cmd_info));

    if (card == NULL)
    {
		bMalloc=1;
		card = (e_device_type*)malloc(sizeof(e_device_type));
    }

    set_cmd_info(card,&cmd,&cmd_info,
                 MMC_STOP_TRANSMISSION,
                 0,
                 6);
    err = rtkemmc_SendCMDGetRSP(&cmd_info,bIgnore);

    if (bMalloc)
    {
		free(card);
		card = NULL;
    }
    if(err){
        MMCPRINTF( "MMC_STOP_TRANSMISSION fail !\n");
    }
    return err;
}

int rtkemmc_send_cmd13(void)
{
	int ret_err=0;
	u32 sts_val=0;

#ifdef MMC_DEBUG
	ret_err = rtkemmc_send_status(&sts_val, 1, 1);
#else
	ret_err = rtkemmc_send_status(&sts_val, 0, 1);
#endif
	if(ret_err) {
#ifdef MMC_DEBUG
		printf("Tuning_cmd13_ERROR: register 0x98012030=0x%x, 0x98012032= 0x%08x\n", cr_readw(EMMC_NORMAL_INT_STAT_R), cr_readw(EMMC_ERROR_INT_STAT_R));
#endif
		if((cr_readw(EMMC_ERROR_INT_STAT_R)&
			(EMMC_AUTO_CMD_ERR|EMMC_CMD_IDX_ERR|EMMC_CMD_END_BIT_ERR|EMMC_CMD_CRC_ERR|EMMC_CMD_TOUT_ERR))!=0){ //check cmd line
#ifdef MMC_DEBUG
			printf("CMD Line error occurs \n");
#endif
			cr_writeb(0x2, EMMC_SW_RST_R); //Perform a software reset
			wait_done((UINT32 *)0x9801202c, (0x2<<24), 0x0, MMC_SEND_STATUS); //wait for clear 0x2f bit 1
		}
		if((cr_readw(EMMC_ERROR_INT_STAT_R)&
			(EMMC_ADMA_ERR|EMMC_DATA_END_BIT_ERR|EMMC_DATA_CRC_ERR|EMMC_DATA_TOUT_ERR)) !=0){ //check data line
#ifdef MMC_DEBUG
			printf("DAT Line error occurs \n");
#endif
			cr_writeb(0x4, EMMC_SW_RST_R); //Perform a software reset
			wait_done((UINT32 *)0x9801202c, (0x4<<24), 0x0, MMC_SEND_STATUS); //wait for clear 0x2f bit 2
		}
	}
	return ret_err;
}

int rtkemmc_send_cmd35(void)
{
	int err = 0;
	int bMalloc=0;
	e_device_type * card = NULL;

	sync();

	struct mmc_cmd cmd;
	struct rtk_cmd_info cmd_info;

	memset(&cmd, 0, sizeof(struct mmc_cmd));
	memset(&cmd_info, 0, sizeof(struct rtk_cmd_info));

	if (card == NULL)
	{
		bMalloc=1;
		card = (e_device_type*)malloc(sizeof( e_device_type));
	}

	set_cmd_info(card, &cmd, &cmd_info, MMC_ERASE_GROUP_START, 0x00020000, 6);
	err = rtkemmc_SendCMDGetRSP(&cmd_info, 1);

	if (bMalloc)
	{
		free(card);
		card = NULL;
	}

	if(err){
#ifdef MMC_DEBUG
		printf("MMC_ERASE_GROUP_START fail, register 0x98012030=0x%x, 0x98012032= 0x%08x\n",
		cr_readw(EMMC_NORMAL_INT_STAT_R), cr_readw(EMMC_ERROR_INT_STAT_R));
#endif
		if((cr_readw(EMMC_ERROR_INT_STAT_R)&
			(EMMC_AUTO_CMD_ERR|EMMC_CMD_IDX_ERR|EMMC_CMD_END_BIT_ERR|EMMC_CMD_CRC_ERR|EMMC_CMD_TOUT_ERR))!=0){ //check cmd line
#ifdef MMC_DEBUG
			printf("CMD Line error occurs \n");
#endif
			cr_writeb(0x2, EMMC_SW_RST_R); //Perform a software reset
			wait_done((UINT32 *)0x9801202c, (0x2<<24), 0x0, MMC_ERASE_GROUP_START); //wait for clear 0x2f bit 1
		}
		if((cr_readw(EMMC_ERROR_INT_STAT_R)&
			(EMMC_ADMA_ERR|EMMC_DATA_END_BIT_ERR|EMMC_DATA_CRC_ERR|EMMC_DATA_TOUT_ERR)) !=0){ //check data line
#ifdef MMC_DEBUG
			printf("DAT Line error occurs \n");
#endif
			cr_writeb(0x4, EMMC_SW_RST_R); //Perform a software reset
			wait_done((UINT32 *)0x9801202c, (0x4<<24), 0x0, MMC_ERASE_GROUP_START); //wait for clear 0x2f bit 2
		}
	}

	return err;
}

int rtkemmc_send_cmd21(void)
{
	int ret_err=0;
	struct rtk_cmd_info cmd_info;
	struct mmc_cmd cmd_val;
	struct mmc_data data;

	MY_CLR_ALIGN_BUFFER();
	MY_ALLOC_CACHE_ALIGN_BUFFER(char, crd_tmp_buffer,  0x80);
	setRandomMemory((unsigned long)crd_tmp_buffer,0x80);

	cmd_info.cmd= &cmd_val;
	cmd_info.data= &data;
	cmd_info.cmd->cmdarg = 0x0;
	cmd_info.cmd->cmdidx = MMC_SEND_TUNING_BLOCK_HS200;
	cmd_info.rsp_len         = 6;
	cmd_info.byte_count  = 0x80;
	cmd_info.block_count = 1;
	cmd_info.dma_buffer = (unsigned char *) crd_tmp_buffer;
	data.blocks = 1;
	data.dest = crd_tmp_buffer;
	cmd_info.xfer_flag       = RTK_MMC_DATA_READ; //dma the result to ddr
	MMCPRINTF("*** %s %s %d ***, arg=0x%08x\n", __FILE__, __FUNCTION__, __LINE__,cmd_info.cmd->cmdarg);
	ret_err = rtkemmc_Stream_Cmd(&cmd_info, 1);

	if (ret_err) {
#ifdef MMC_DEBUG
		printf("Tuning_cmd21_ERROR: register 0x98012030=0x%x, 0x98012032= 0x%08x\n", cr_readw(EMMC_NORMAL_INT_STAT_R), cr_readw(EMMC_ERROR_INT_STAT_R));
#endif
		error_handling(cmd_info.cmd->cmdidx, 1);
	}
	return ret_err;
}

unsigned int wait_for_complete(unsigned int cmd, unsigned int bIgnore){
	int ret_error=0;
	int XFER_flag=0;

	switch(cmd) {
		case MMC_READ_SINGLE_BLOCK:
		case MMC_READ_MULTIPLE_BLOCK:
		case MMC_WRITE_BLOCK:
		case MMC_WRITE_MULTIPLE_BLOCK:
		case MMC_SEND_EXT_CSD:
		case MMC_GEN_CMD:
		case MMC_SLEEP_AWAKE:
		case MMC_SWITCH:
		case MMC_ERASE:
		case MMC_SEND_TUNING_BLOCK_HS200:
			XFER_flag=1;
			break;
	}
	if(XFER_flag==1)
		wait_done((UINT32 *)EMMC_NORMAL_INT_STAT_R, 3, 3, cmd); // Check CMD_COMPLETE and XFER_COMPLETE
	else wait_done((UINT32 *)EMMC_NORMAL_INT_STAT_R, 1, 1, cmd); // Check CMD_COMPLETE

	if((cr_readw(EMMC_NORMAL_INT_STAT_R)&0x8000)!=0)
		ret_error = check_error(bIgnore);

	return ret_error;
}

int rtkemmc_SendCMDGetRSP( struct rtk_cmd_info * cmd_info, unsigned int bIgnore)
{
	volatile int ret_error;
	UINT32 cmd_idx = cmd_info->cmd->cmdidx;
	u32 *rsp = (u32 *)&cmd_info->cmd->response;

	rtkemmc_set_rspparam(cmd_info);
	cr_writew(0xffff, EMMC_ERROR_INT_STAT_R);
	cr_writew(0xffff, EMMC_NORMAL_INT_STAT_R);
	cr_writel(cmd_info->cmd->cmdarg, EMMC_ARGUMENT_R);
	CP15ISB;
	sync();

#ifdef MMC_DEBUG
	printf("[%s:%d] cmd_idx = %d, args=0x%x resp_len=%d, 0x98012030=0x%x\n",
		__FILE__, __LINE__, cmd_idx, cmd_info->cmd->cmdarg, cmd_info->rsp_len, cr_readw(EMMC_NORMAL_INT_STAT_R));
#endif
	cr_writew((cmd_idx<<0x8)| cmd_info->cmd_para, EMMC_CMD_R);   // Command register
	CP15ISB;
	sync();

	ret_error = wait_for_complete(cmd_idx, bIgnore);

	if (ret_error==0)
	{
		rtkemmc_read_rsp(rsp, cmd_info->rsp_len);
	}
	else
	{
		if (!bIgnore) {
			printf("rtkemmc_SendCMDGetRSP error...\n");
			if((cr_readw(EMMC_ERROR_INT_STAT_R)&
				(EMMC_AUTO_CMD_ERR|EMMC_CMD_IDX_ERR|EMMC_CMD_END_BIT_ERR|EMMC_CMD_CRC_ERR|EMMC_CMD_TOUT_ERR))!=0){ //check cmd line
#ifdef MMC_DEBUG
				printf("CMD Line error occurs \n");
#endif
				cr_writeb(0x2, EMMC_SW_RST_R); //Perform a software reset
				wait_done((UINT32 *)0x9801202c, (0x2<<24), 0x0, cmd_idx); //wait for clear 0x2f bit 1
			}
			if((cr_readw(EMMC_ERROR_INT_STAT_R)&
				(EMMC_ADMA_ERR|EMMC_DATA_END_BIT_ERR|EMMC_DATA_CRC_ERR|EMMC_DATA_TOUT_ERR)) !=0){ //check data line
#ifdef MMC_DEBUG
				printf("DAT Line error occurs \n");
#endif
				cr_writeb(0x4, EMMC_SW_RST_R); //Perform a software reset
				wait_done((UINT32 *)0x9801202c, (0x4<<24), 0x0, cmd_idx); //wait for clear 0x2f bit 2
			}
		}
	}
	return ret_error;
}

int rtkemmc_Stream_Cmd( struct rtk_cmd_info * cmd_info, unsigned int bIgnore )
{
	UINT32  EMMC_MAX_MULTI_BLK = EMMC_MAX_XFER_BLKCNT;
	unsigned int cmd_idx      = cmd_info->cmd->cmdidx;
	unsigned int block_count  = cmd_info->data->blocks;
	unsigned int dma_len=0;
	int ret_err=1;

	UINT32  remain_blk_cnt = block_count;
	UINT32  cur_blk_cnt;
	UINT32  cur_blk_addr = cmd_info->cmd->cmdarg;

	u8* data = (unsigned char *) cmd_info->data->dest;

	if (data == NULL){
		ret_err = -1;
	}

	rtkemmc_set_rspparam(cmd_info);
	while (remain_blk_cnt) {
		if (remain_blk_cnt > EMMC_MAX_MULTI_BLK) //one time to transfer
			cur_blk_cnt = EMMC_MAX_MULTI_BLK;
		else
			cur_blk_cnt = remain_blk_cnt;

		remain_blk_cnt -= cur_blk_cnt;

		if(cmd_idx==MMC_SEND_TUNING_BLOCK_HS200) dma_len=0x80;
		else dma_len = (cur_blk_cnt << 9);

		MMCPRINTF("debug : cur_blk_addr = %u, cur_blk_cnt = %u \n", cur_blk_addr, cur_blk_cnt);
		ret_err = rtkemmc_Stream(cmd_idx,
								cur_blk_addr,
								(UINT32)(uintptr_t)data + ((cur_blk_addr - cmd_info->cmd->cmdarg) << 9),
								dma_len,
								bIgnore);

		if ((!ret_err)&&(bIgnore))
		{
			return 0;
		}
		cur_blk_addr += cur_blk_cnt;
		if (ret_err) return -1;
	}
	if (cmd_idx == MMC_SEND_EXT_CSD)
		return 0;
	else
		return block_count;
}

int rtkemmc_Stream( unsigned int cmd_idx,UINT32 blk_addr, UINT32 dma_addr, UINT32 dma_length, int bIgnore) {
	unsigned int read=1;
	unsigned int  mul_blk=0;
	unsigned int ret_error = 0;

	cr_writel(cr_readl(EMMC_SWC_SEL)|0x10, EMMC_SWC_SEL);
	cr_writel(cr_readl(EMMC_SWC_SEL1)&0xffffffef, EMMC_SWC_SEL1);
	cr_writel(cr_readl(EMMC_SWC_SEL2)|0x10, EMMC_SWC_SEL2);
	cr_writel(cr_readl(EMMC_SWC_SEL3)&0xffffffef, EMMC_SWC_SEL3);
	cr_writel(0, EMMC_CP);

	CP15ISB;
	sync();

	make_ip_des(dma_addr, dma_length);
	flush_cache(dma_addr, dma_length);
	CP15ISB;
	sync();

	cr_writew(0xffff, EMMC_ERROR_INT_STAT_R);
	cr_writew(0xffff, EMMC_NORMAL_INT_STAT_R);
	if(cmd_idx==MMC_SEND_TUNING_BLOCK_HS200) cr_writew(0x1, EMMC_BLOCKCOUNT_R);
	else cr_writew((dma_length/0x200), EMMC_BLOCKCOUNT_R);
	cr_writel((uintptr_t)rw_descriptor, EMMC_ADMA_SA_LOW_R);
	cr_writel(blk_addr, EMMC_ARGUMENT_R);
	CP15ISB;
	sync();

#ifdef MMC_DEBUG
	printf("[%s:%d] cmd_idx = %d, blk_addr = 0x%08x, dma_addr = 0x%08x, dma_length = 0x%x, 0x98012030=0x%x\n",
		 __FILE__, __LINE__, cmd_idx, blk_addr, dma_addr, dma_length, cr_readw(EMMC_NORMAL_INT_STAT_R));
#endif
        if(cmd_idx==MMC_WRITE_BLOCK || cmd_idx==MMC_WRITE_MULTIPLE_BLOCK || cmd_idx==MMC_LOCK_UNLOCK ||cmd_idx==47 || cmd_idx==49)
                read=0;

        if(cmd_idx==MMC_WRITE_MULTIPLE_BLOCK || cmd_idx==MMC_READ_MULTIPLE_BLOCK)
                mul_blk=1;

        cr_writew((mul_blk<<EMMC_MULTI_BLK_SEL)|
                       (read<<EMMC_DATA_XFER_DIR)|
                       (mul_blk<<EMMC_AUTO_CMD_ENABLE)|
                        EMMC_BLOCK_COUNT_ENABLE|
                        EMMC_DMA_ENABLE,
                        EMMC_XFER_MODE_R);

	cr_writew((cmd_idx<<8)|0x3a, EMMC_CMD_R);
	CP15ISB;
	sync();

	ret_error = wait_for_complete(cmd_idx, bIgnore);

	if(ret_error) {
		if(!bIgnore)
			printf("SD_Stream_Cmd error..., cmd_idx = %d\n", cmd_idx);
		error_handling(cmd_idx, bIgnore);
	}
	CP15ISB;
	sync();

	return ret_error;
}

/*******************************************************
 *
 *******************************************************/
void rtkemmc_show_cid( e_device_type *card )
{
	int i;
	unsigned int sn;
	unsigned char cid[16];
	for( i = 0; i < 4; i++ ) {
		cid[(i<<2)]   = ( card->raw_cid[i]>>24 ) & 0xFF;
		cid[(i<<2)+1] = ( card->raw_cid[i]>>16 ) & 0xFF;
		cid[(i<<2)+2] = ( card->raw_cid[i]>>8  ) & 0xFF;
		cid[(i<<2)+3] = ( card->raw_cid[i]     ) & 0xFF;
	}
#ifdef EMMC_SHOW_CID

#ifdef THIS_IS_FLASH_WRITE_U_ENV
	prints("emmc:CID");
#else
	MMCPRINTF("emmc:CID");
#endif
	for( i = 0; i < 16; i++ ) {

#ifdef THIS_IS_FLASH_WRITE_U_ENV
		print_hex(cid[i]);
#else
		MMCPRINTF(" %02x", cid[i]);
#endif
	}

#ifdef THIS_IS_FLASH_WRITE_U_ENV
	prints("\n");
#else
	MMCPRINTF("\n");
#endif

#endif

#ifdef THIS_IS_FLASH_WRITE_U_ENV
	prints("CID     0x");
	print_hex(cid[0]);
	prints("\n");
#else
	MMCPRINTF("CID    0x%02x\n", cid[0]);
#endif
	switch( (cid[1] & 0x3) ) {
	case 0:
#ifdef THIS_IS_FLASH_WRITE_U_ENV
		prints("CBX    Card\n");
#else
		MMCPRINTF("CBX    Card\n");
#endif
		break;
	case 1:
#ifdef THIS_IS_FLASH_WRITE_U_ENV
		prints("CBX    BGA\n");
#else
		MMCPRINTF("CBX    BGA\n");
#endif
		break;
	case 2:
#ifdef THIS_IS_FLASH_WRITE_U_ENV
		prints("CBX    POP\n");
#else
		MMCPRINTF("CBX    POP\n");
#endif
		break;
	case 3:
#ifdef THIS_IS_FLASH_WRITE_U_ENV
		prints("CBX    Unknown\n");
#else
		MMCPRINTF("CBX    Unknown\n");
#endif
		break;
	}

#ifdef THIS_IS_FLASH_WRITE_U_ENV
	prints("OID    0x");
	print_val(cid[2],2);
	prints("\n");
	prints("PNM    0x");
	print_val(cid[3],2);
	print_val(cid[4],2);
	print_val(cid[5],2);
	print_val(cid[6],2);
	print_val(cid[7],2);
	print_val(cid[8],2);
	prints("\n");
	prints("PRV    0x");
	print_val((cid[9]>>4)&0xf,2);
	print_val(cid[9]&0xf,2);
	prints("\n");
#else
	MMCPRINTF("OID    0x%02x\n", cid[2]);
	MMCPRINTF("PNM    %c%c%c%c%c%c\n", cid[3], cid[4], cid[5], cid[6], cid[7], cid[8]);
	MMCPRINTF("PRV    %d.%d\n", (cid[9]>>4)&0xf, cid[9]&0xf);
#endif
	sn = cid[13];
	sn = (sn<<8) | cid[12];
	sn = (sn<<8) | cid[11];
	sn = (sn<<8) | cid[10];
#ifdef THIS_IS_FLASH_WRITE_U_ENV
	prints("PSN    ");
	print_hex(sn);
	prints("\n");
#else
	MMCPRINTF("PSN    %u(0x%08x)\n", sn, sn);
#endif
	if( cid[9] ) {
#ifdef THIS_IS_FLASH_WRITE_U_ENV
		prints("MDT    ");
		print_hex((cid[9]>>4)&0xf);
		print_hex((cid[14]&0xf)+1997);
		prints("\n");
#else
		MMCPRINTF("MDT    %02d/%d)", (cid[9]>>4)&0xf, (cid[14]&0xf)+1997);
#endif
	}
	else {
#ifdef THIS_IS_FLASH_WRITE_U_ENV
		prints("MDT    --/----\n");
#else
		MMCPRINTF("MDT    --/----\n", (cid[9]>>4)&0xf, (cid[14]&0xf)+1997);
#endif
	}
}


/*******************************************************
 *
 *******************************************************/
int mmc_decode_csd( struct mmc* mmc )
{
	struct rtk_mmc_csd vcsd;
	struct rtk_mmc_csd *csd = (struct rtk_mmc_csd*)&vcsd;
	unsigned int e, m;
	unsigned int * resp = mmc->csd;
	int err = 0;

	printf("mmc_decode_csd\n");
	memset(&vcsd, 0x00, sizeof(struct rtk_mmc_csd));
	/*
	 * We only understand CSD structure v1.1 and v1.2.
	 * v1.2 has extra information in bits 15, 11 and 10.
	 * We also support eMMC v4.4 & v4.41.
	*/
	csd->csd_ver2 = 0xff;
	csd->csd_ver = UNSTUFF_BITS(resp, 126, 2);

	// 0, CSD Ver. 1.0
	// 1, CSD Ver. 1.1
	// 2, CSD Ver. 1.2, define in spec. 4.1-4.3
	// 3, CSD Ver define in EXT_CSD[194]
	//    EXT_CSD[194] 0, CSD Ver. 1.0
	//                 1, CSD Ver. 1.1
	//                 2, CSD Ver. 1.2, define in spec. 4.1-4.51
	//                 others, RSV

	csd->mmca_vsn       = UNSTUFF_BITS(resp, 122, 4);
	m = UNSTUFF_BITS(resp, 115, 4);
	e = UNSTUFF_BITS(resp, 112, 3);
	csd->tacc_ns        = (tacc_exp[e] * tacc_mant[m] + 9) / 10;
	csd->tacc_clks      = UNSTUFF_BITS(resp, 104, 8) * 100;

	m = UNSTUFF_BITS(resp, 99, 4);
	e = UNSTUFF_BITS(resp, 96, 3);
	csd->max_dtr        = tran_exp[e] * tran_mant[m];
	csd->cmdclass       = UNSTUFF_BITS(resp, 84, 12);

	m = UNSTUFF_BITS(resp, 62, 12);
	e = UNSTUFF_BITS(resp, 47, 3);
	csd->capacity       = (1 + m) << (e + 2);
	csd->read_blkbits   = UNSTUFF_BITS(resp, 80, 4);
	csd->read_partial   = UNSTUFF_BITS(resp, 79, 1);
	csd->write_misalign = UNSTUFF_BITS(resp, 78, 1);
	csd->read_misalign  = UNSTUFF_BITS(resp, 77, 1);
	csd->r2w_factor     = UNSTUFF_BITS(resp, 26, 3);
	csd->write_blkbits  = UNSTUFF_BITS(resp, 22, 4);
	csd->write_partial  = UNSTUFF_BITS(resp, 21, 1);

	printf("CSD_STRUCTURE :%02x\n", csd->csd_ver);
	printf("SPEC_VERS   :%02x\n", csd->mmca_vsn);
	printf("TRAN_SPEED  :%02x\n", UNSTUFF_BITS(resp, 96, 8));
	printf("C_SIZE      :%08x\n",m);
	printf("C_SIZE_MULT :%08x\n",e);
	printf("Block Num   :%08x\n",csd->capacity);
	printf("Block Len   :%08x\n" ,1<<csd->read_blkbits);
	printf("Total Bytes :%08x\n",csd->capacity*(1<<csd->read_blkbits));
//err_out:
	return err;
}

/*******************************************************
 *
 *******************************************************/
int mmc_decode_cid( struct mmc * rcard )
{
	e_device_type card;
	unsigned int * resp = rcard->cid;
	unsigned int * resp1 = rcard->csd;
	uint vsn = UNSTUFF_BITS(resp1, 122, 4);

	printf("mmc_decode_cid\n");
	/*
	 * The selection of the format here is based upon published
	 * specs from sandisk and from what people have reported.
	*/
	switch (vsn) {
	case 0: /* MMC v1.0 - v1.2 */
	case 1: /* MMC v1.4 */
		card.cid.manfid        = UNSTUFF_BITS(resp, 104, 24);
		card.cid.prod_name[0]  = UNSTUFF_BITS(resp, 96, 8);
		card.cid.prod_name[1]  = UNSTUFF_BITS(resp, 88, 8);
		card.cid.prod_name[2]  = UNSTUFF_BITS(resp, 80, 8);
		card.cid.prod_name[3]  = UNSTUFF_BITS(resp, 72, 8);
		card.cid.prod_name[4]  = UNSTUFF_BITS(resp, 64, 8);
		card.cid.prod_name[5]  = UNSTUFF_BITS(resp, 56, 8);
		card.cid.prod_name[6]  = UNSTUFF_BITS(resp, 48, 8);
		card.cid.hwrev         = UNSTUFF_BITS(resp, 44, 4);
		card.cid.fwrev         = UNSTUFF_BITS(resp, 40, 4);
		card.cid.serial        = UNSTUFF_BITS(resp, 16, 24);
		card.cid.month         = UNSTUFF_BITS(resp, 12, 4);
		card.cid.year          = UNSTUFF_BITS(resp, 8,  4) + 1997;
		printf("cid v_1.0-1.4, manfid=%02x\n", card.cid.manfid);
		break;

	case 2: /* MMC v2.0 - v2.2 */
	case 3: /* MMC v3.1 - v3.3 */
	case 4: /* MMC v4 */
		card.cid.manfid        = UNSTUFF_BITS(resp, 120, 8);
		card.cid.oemid         = UNSTUFF_BITS(resp, 104, 16);
		card.cid.prod_name[0]  = UNSTUFF_BITS(resp, 96, 8);
		card.cid.prod_name[1]  = UNSTUFF_BITS(resp, 88, 8);
		card.cid.prod_name[2]  = UNSTUFF_BITS(resp, 80, 8);
		card.cid.prod_name[3]  = UNSTUFF_BITS(resp, 72, 8);
		card.cid.prod_name[4]  = UNSTUFF_BITS(resp, 64, 8);
		card.cid.prod_name[5]  = UNSTUFF_BITS(resp, 56, 8);
		card.cid.serial        = UNSTUFF_BITS(resp, 16, 32);
		card.cid.month         = UNSTUFF_BITS(resp, 12, 4);
		card.cid.year          = UNSTUFF_BITS(resp, 8, 4) + 1997;
		printf("cid v_2.0-4, manfid=%02x, cbx=%02x\n", card.cid.manfid, UNSTUFF_BITS(resp, 112, 2));
		break;

	default:
		//printf("card has unknown MMCA version %d\n",
		//       card.csd.mmca_vsn);
		return -1;
	}

	return 0;
}

/*******************************************************
 *
 *******************************************************/
void mmc_show_csd( struct mmc* card )
{
	int i;
	unsigned char csd[16];
	for( i = 0; i < 4; i++ ) {
		csd[(i<<2)]   = ( card->csd[i]>>24 ) & 0xFF;
		csd[(i<<2)+1] = ( card->csd[i]>>16 ) & 0xFF;
		csd[(i<<2)+2] = ( card->csd[i]>>8  ) & 0xFF;
		csd[(i<<2)+3] = ( card->csd[i]     ) & 0xFF;
	}
#ifdef MMC_DEBUG
	printf("emmc:CSD(hex)");
	for( i = 0; i < 16; i++ ) {
		printf(" %02x", csd[i]);
	}
	printf("\n");
#endif
	printf(" %02x", csd[0]);
	mmc_decode_csd(card);
}

/*******************************************************
 *
 *******************************************************/
void mmc_show_ext_csd( unsigned char * pext_csd )
{
	int i,j,k;
	unsigned int sec_cnt;
	unsigned int boot_size_mult;

	printf("emmc:EXT CSD\n");
	k = 0;
	for( i = 0; i < 32; i++ ) {
		printf("    : %03x", i<<4);
		for( j = 0; j < 16; j++ ) {
			printf(" %02x ", pext_csd[k++]);
		}
		printf("\n");
	}
	printf("    :k=%02x\n",k);

	boot_size_mult = pext_csd[226];

	printf("emmc:BOOT PART %04x ",boot_size_mult<<7);
	printf(" Kbytes(%04x)\n", boot_size_mult);

	sec_cnt = pext_csd[215];
	sec_cnt = (sec_cnt<<8) | pext_csd[214];
	sec_cnt = (sec_cnt<<8) | pext_csd[213];
	sec_cnt = (sec_cnt<<8) | pext_csd[212];

	printf("emmc:SEC_COUNT %04x\n", sec_cnt);
	printf(" emmc:reserve227 %02x\n", pext_csd[227]);
	printf(" emmc:reserve240 %02x\n", pext_csd[240]);
	printf(" emmc:reserve254 %02x\n", pext_csd[254]);
	printf(" emmc:reserve256 %02x\n", pext_csd[256]);
	printf(" emmc:reserve493 %02x\n", pext_csd[493]);
	printf(" emmc:reserve505 %02x\n", pext_csd[505]);
}


/*******************************************************
 *
 *******************************************************/
int mmc_read_ext_csd( e_device_type * card )
{
	struct rtk_cmd_info cmd_info;
	struct mmc_cmd cmd;
	struct mmc_data data;
	int ret_err;

#ifdef THIS_IS_FLASH_WRITE_U_ENV
	prints("mmc_read_ext_csd\n");
#else
	MMCPRINTF("mmc_read_ext_csd\n");
#endif

	cmd_info.cmd= &cmd;
	cmd_info.data= &data;
	cmd.cmdarg     = 0;
	cmd.cmdidx = MMC_SEND_EXT_CSD;
	data.blocks = 1;
	data.dest = (char *) sys_ext_csd_addr;

	MMCPRINTF("ext_csd ptr 0x%p\n",sys_ext_csd_addr);

	cmd_info.rsp_len     = 6;
	cmd_info.byte_count  = 0x200;
	cmd_info.block_count = 1;
	cmd_info.dma_buffer = sys_ext_csd_addr;
	cmd_info.xfer_flag   = RTK_MMC_DATA_READ;

	ret_err = rtkemmc_Stream_Cmd(&cmd_info, 0);

	if(ret_err){
#ifdef THIS_IS_FLASH_WRITE_U_ENV
		prints("unable to read EXT_CSD(ret_err=");
		print_hex(ret_err);
		prints("\n");
#else
		MMCPRINTF("unable to read EXT_CSD(ret_err=%d)", ret_err);
#endif
	}
	else{
		mmc_show_ext_csd(sys_ext_csd_addr);
		card->ext_csd.boot_blk_size = (*(ptr_ext_csd+226)<<8);
		card->csd.csd_ver2 = *(ptr_ext_csd+194);
		card->ext_csd.rev = *(ptr_ext_csd+192);
		card->ext_csd.part_cfg = *(ptr_ext_csd+179);
		card->ext_csd.boot_cfg = *(ptr_ext_csd+178);
		card->ext_csd.boot_wp_sts = *(ptr_ext_csd+174);
		card->ext_csd.boot_wp = *(ptr_ext_csd+173);
		card->curr_part_indx = card->ext_csd.part_cfg & 0x07;

#ifdef THIS_IS_FLASH_WRITE_U_ENV
		prints("emmc:BOOT PART MULTI = 0x");
		print_hex(*(ptr_ext_csd+226));
		prints(", BP_BLKS=0x");
		print_hex(*(ptr_ext_csd+226)<<8);
		prints("\n");
		prints("emmc:BOOT PART CFG = 0x");
		print_hex(card->ext_csd.part_cfg);
		prints("(0x");
		print_hex(card->curr_part_indx);
		prints(")\n");
#else
		MMCPRINTF("emmc:BOOT PART MULTI = 0x%02x, BP_BLKS=0x%08x\n", *(ptr_ext_csd+226), *(ptr_ext_csd+226)<<8);
		MMCPRINTF("emmc:BOOT PART CFG = 0x%02x(%d)\n", card->ext_csd.part_cfg, card->curr_part_indx);
#endif

		if (card->ext_csd.rev > 6) {
#ifdef THIS_IS_FLASH_WRITE_U_ENV
			prints("unrecognized EXT_CSD structure version ");
			print_hex(card->ext_csd.rev);
			prints(", please update fw\n", card->ext_csd.rev);
#else
			MMCPRINTF("unrecognized EXT_CSD structure version %d, please update fw\n", card->ext_csd.rev);
#endif
		}

		if (card->ext_csd.rev >= 2) {
			card->ext_csd.sectors = *((unsigned int *)(ptr_ext_csd+EXT_CSD_SEC_CNT));
		}

		switch (*(ptr_ext_csd+EXT_CSD_CARD_TYPE) & (EXT_CSD_CARD_TYPE_26|EXT_CSD_CARD_TYPE_52)) {
	        case EXT_CSD_CARD_TYPE_52 | EXT_CSD_CARD_TYPE_26:
			card->ext_csd.hs_max_dtr = 52000000;
			break;
	        case EXT_CSD_CARD_TYPE_26:
			card->ext_csd.hs_max_dtr = 26000000;
			break;
	        default:
	            /* MMC v4 spec says this cannot happen */
#ifdef THIS_IS_FLASH_WRITE_U_ENV
			prints("card is mmc v4 but does not support any high-speed modes.\n");
#else
			MMCPRINTF("card is mmc v4 but doesn't " "support any high-speed modes.\n");
#endif
		}

		if (card->ext_csd.rev >= 3) {
			unsigned int sa_shift = *(ptr_ext_csd+EXT_CSD_S_A_TIMEOUT);
			/* Sleep / awake timeout in 100ns units */
			if (sa_shift > 0 && sa_shift <= 0x17){
				card->ext_csd.sa_timeout = 1 << *(ptr_ext_csd+EXT_CSD_S_A_TIMEOUT);
			}
		}
	}

	return ret_err;
}

/*******************************************************
 *
 *******************************************************/
#if 0
int rtkemmc_hw_reset_signal( void )
{
	unsigned int tmp_mux;
	unsigned int tmp_dir;
	unsigned int tmp_dat;

	#define PIN_MUX_REG		(0xb8000814UL)
	#define PIN_DIR_REG		(0xb801bc00UL)
	#define PIN_DAT_REG		(0xb801bc18UL)

	tmp_mux = cr_readl( PIN_MUX_REG );
	tmp_dir = cr_readl( PIN_DIR_REG );
	tmp_dat = cr_readl( PIN_DAT_REG );

	cr_writel( tmp_dat|(1<<20), PIN_DAT_REG );
	cr_writel( tmp_dir|(1<<20), PIN_DIR_REG );
	cr_writel( tmp_dat|(1<<20), PIN_DAT_REG ); // high

	cr_writel( tmp_mux|(0xF<<28), PIN_MUX_REG );

	cr_writel( tmp_dat&~(1<<20), PIN_DAT_REG );  // low
	udelay(10);
	cr_writel( tmp_dat|(1<<20), PIN_DAT_REG ); // high

	cr_writel( tmp_mux, PIN_MUX_REG ); // restore original status

	return 0;
}
#endif

void set_emmc_pin_mux(void)
{
	cr_writel((cr_readl(0x9804E000)&0xff000000)|0x00aaaaaa, 0x9804E000); //pad mux
	sync();
}

int rtkemmc_request(struct mmc * mmc, struct mmc_cmd * cmd, struct mmc_data * data )
{
	int ret_err=0;
	volatile struct rtk_cmd_info cmd_info;

	MMCPRINTF("\n*** %s %s %d, cmdidx=0x%02x(%d), flags=0x%08x -------\n", __FILE__, __FUNCTION__, __LINE__, cmd->cmdidx, cmd->cmdidx, cmd->flags);

	if (data)
	{
		MMCPRINTF("*** %s %s %d, flags=0x%08x(%s), blks=%d, blksize=%d\n",
			__FILE__, __FUNCTION__, __LINE__, data->flags, (data->flags&MMC_DATA_READ) ? "R" : "W", data->blocks, data->blocksize);
		cmd_info.cmd = cmd;
		cmd_info.data = data;
		ret_err = rtkemmc_Stream_Cmd( (struct rtk_cmd_info*) &cmd_info, 0);
	}
	else
	{
		cmd_info.cmd = cmd;
		cmd_info.data = NULL;
		ret_err = rtkemmc_SendCMDGetRSP( (struct rtk_cmd_info*) &cmd_info, 0 );
	}

	return ret_err;
}//0=success !0=fail

void rtkemmc_set_ios(struct mmc * mmc, unsigned int ios_caps)
{
	MMCPRINTF("*** %s %s %d, caps=0x%08x bw=%d, clk=%d\n", __FILE__, __FUNCTION__, __LINE__, ios_caps, mmc->bus_width, mmc->clock);

	// mmc->clk mode
	if(ios_caps & MMC_IOS_CLK) {
		if( mmc->mode_sel & MMC_MODE_HS200 ) {
			frequency(0xa6, 0x0);  //200MHZ
                        cr_writew((cr_readw( EMMC_HOST_CTRL2_R)&0xfff8)|MODE_HS200, EMMC_HOST_CTRL2_R);
			MMCPRINTF("speed to hs200\n");
		}
		else if( mmc->mode_sel & MMC_MODE_HSDDR_52MHz ) {
			frequency(0x57, 0x1);  //50MB
                        cr_writeb((cr_readb(EMMC_HOST_CTRL1_R)&0xfb)|EMMC_HIGH_SPEED_EN, EMMC_HOST_CTRL1_R);
                        cr_writew((cr_readw(EMMC_HOST_CTRL2_R)&0xfff8)|MODE_DDR50, EMMC_HOST_CTRL2_R);
			MMCPRINTF("speed to 80\n");
		}
		else if( mmc->mode_sel & (MMC_MODE_HS|MMC_MODE_HS_52MHz)){
			frequency(0x57, 0x1);  //50Mhz
                        cr_writew((cr_readw(EMMC_HOST_CTRL2_R)&0xfff8)|MODE_SDR, EMMC_HOST_CTRL2_R);
			MMCPRINTF("speed to 100\n");
		}
		else {
			//legacy
			frequency(0x46, 0x80);  //80Mhz
                        cr_writew((cr_readw(EMMC_HOST_CTRL2_R)&0xfff8)|MODE_LEGACY, EMMC_HOST_CTRL2_R);
			MMCPRINTF("speed to \n");
		}

	}

	// mmc->bus_width
	if(ios_caps & MMC_IOS_BUSWIDTH)
	{
		if( mmc->bus_width == 1 ) {
			cr_writeb((cr_readb(EMMC_HOST_CTRL1_R)&0xdd)|EMMC_BUS_WIDTH_1, EMMC_HOST_CTRL1_R);
			sync();
		}
		else if( mmc->bus_width == 4 ) {
			cr_writeb((cr_readb(EMMC_HOST_CTRL1_R)&0xdd)|EMMC_BUS_WIDTH_4, EMMC_HOST_CTRL1_R);
			sync();
		}
		else if( mmc->bus_width == 8 ) {
			cr_writeb((cr_readb(EMMC_HOST_CTRL1_R)&0xdf)|EMMC_BUS_WIDTH_8, EMMC_HOST_CTRL1_R);
			sync();
		}
		else {
			UPRINTF("*** %s %s %d, ERR bw=%d, clk=%d\n", __FILE__, __FUNCTION__, __LINE__, mmc->bus_width, mmc->clock);
		}
	}
}

int rtkemmc_init(void)
{
	//Intialize
	cr_writel(cr_readl(EMMC_OTHER1)&0xffffffff7, EMMC_OTHER1);
        cr_writel(cr_readl(EMMC_OTHER1)|0x8, EMMC_OTHER1);
	CP15ISB;
	sync();

	cr_writeb(0x07, EMMC_SW_RST_R);      //9801202f, Software Reset Register
	CP15ISB;
	sync();
	printf("EMMC_SW_RST_R=0x%x\n",cr_readb(EMMC_SW_RST_R));

	cr_writeb(0x0e, EMMC_TOUT_CTRL_R);      //9801202e, Timeout Control register
	CP15ISB;
	sync();

	cr_writew(0x200, EMMC_BLOCKSIZE_R);      //98012004, block size = 512Byte
	CP15ISB;
	sync();

	cr_writew(0x1008 , EMMC_HOST_CTRL2_R);
	CP15ISB;
	sync();

	cr_writew(0xfeff, EMMC_NORMAL_INT_STAT_EN_R);      //98012034, enable all Normal Interrupt Status register
	CP15ISB;
	sync();

	cr_writew(EMMC_ALL_ERR_STAT_EN, EMMC_ERROR_INT_STAT_EN_R); //98012036, enable all error Interrupt Status register
	CP15ISB;
	sync();

	cr_writew(0xfeff, EMMC_NORMAL_INT_SIGNAL_EN_R);     //98012038, enable all Normal SIGNAL Interrupt  register
	CP15ISB;
	sync();

	cr_writew(EMMC_ALL_ERR_SIGNAL_EN, EMMC_ERROR_INT_SIGNAL_EN_R);      //9801203a, enable all error SIGNAL Interrupt register
	CP15ISB;
	sync();

	cr_writeb(0x0d, EMMC_CTRL_R);      //9801202f, choose is card emmc bit
	CP15ISB;
	sync();

	cr_writel(cr_readl(EMMC_OTHER1)|0x1, EMMC_OTHER1);        //disable L4 gated
	CP15ISB;
	sync();

	cr_writel(cr_readl(EMMC_DUMMY_SYS)|(EMMC_CLK_O_ICG_EN|EMMC_CARD_STOP_ENABLE), EMMC_DUMMY_SYS);        //enable eMMC command clock
	CP15ISB;
	sync();

	cr_writeb((cr_readb(EMMC_HOST_CTRL1_R)&0xe7)|(EMMC_ADMA2_32<<EMMC_DMA_SEL), EMMC_HOST_CTRL1_R);   //ADMA2 32 bit select
	CP15ISB;
	sync();

	cr_writeb((cr_readb(EMMC_MSHC_CTRL_R) & (~EMMC_CMD_CONFLICT_CHECK)), EMMC_MSHC_CTRL_R);      //disable emmc cmd conflict checkout
	CP15ISB;
	sync();

	cr_writew(cr_readw(EMMC_CLK_CTRL_R)|0x1, EMMC_CLK_CTRL_R);   //enable internal clock
	CP15ISB;
	sync();

	cr_writel(0x1, 0x9804F000+EMMC_NAND_DMA_SEL);        // #sram_ctrl, 0 for nf, 1 for emmc
	CP15ISB;
	sync();

	return 0;
}

/*******************************************************
 *
 *******************************************************/
void rtkemmc_init_setup( void )
{
	cr_writel(cr_readl(0x98000454)|0x1, 0x98000454);
	set_emmc_pin_mux();
#ifdef MMC_DEBUG
	printf("[LY] gCurrentBootMode =%d\n",gCurrentBootMode);
#endif
	gCurrentBootMode = MODE_SD20;
	rtkemmc_debug_msg = 1;
	rtkemmc_off_error_msg_in_init_flow = 1;
	sys_rsp17_addr = S_RSP17_ADDR;   //set rsp buffer addr
	sys_ext_csd_addr = (unsigned char *)S_EXT_CSD_ADDR;
	ptr_ext_csd = (UINT8*)sys_ext_csd_addr;
	emmc_card.rca = 1<<16;
	emmc_card.sector_addressing = 0;

	cr_writel(0x03, SYS_PLL_EMMC1);
	CP15ISB;
	sync();

	rtkemmc_init();

	udelay(10000);

	cr_writeb((cr_readb(EMMC_HOST_CTRL1_R)&0xdd)|EMMC_BUS_WIDTH_1, EMMC_HOST_CTRL1_R);
        CP15ISB;
        sync();

	rtkemmc_set_pad_driving(0x0, 0x0,0x0,0x0);

	phase(0, 0); //VP0, VP1 phase

	cr_writel(cr_readl(EMMC_OTHER1)&0xffffffff7, EMMC_OTHER1);
	cr_writel(cr_readl(EMMC_OTHER1)|0x8, EMMC_OTHER1);

	frequency(0x46, 0x80); //divider = 2 * 128 = 256

	sync();
}

/*******************************************************
 *
 *******************************************************/
 //emmc init flow that for fast init purpose. (for romcode)
int mmc_initial( int reset_only )
{
	if (reset_only)
	{
		rtkemmc_init_setup();
	}
	return 0;
}

void rtkemmc_set_pad_driving(unsigned int clk_drv, unsigned int cmd_drv, unsigned int data_drv, unsigned int ds_drv)
{
	cr_writel((cr_readl(0x9804E000+ EMMC_ISO_pfunc1)&0xff03f03f)|(clk_drv<<6)|(clk_drv<<9)|(cmd_drv<<18)|(cmd_drv<<21),
								0x9804E000 + EMMC_ISO_pfunc1);
	cr_writel((cr_readl(0x9804E000 + EMMC_ISO_pfunc2)&0xff03f03f)|(data_drv<<6)|(data_drv<<9)|(data_drv<<18)|(data_drv<<21),
								0x9804E000 + EMMC_ISO_pfunc2);
	cr_writel((cr_readl(0x9804E000 + EMMC_ISO_pfunc3)&0xff03f03f)|(data_drv<<6)|(data_drv<<9)|(data_drv<<18)|(data_drv<<21),
								0x9804E000 + EMMC_ISO_pfunc3);
	cr_writel((cr_readl(0x9804E000 + EMMC_ISO_pfunc4)&0xff03f03f)|(data_drv<<6)|(data_drv<<9)|(data_drv<<18)|(data_drv<<21),
								0x9804E000 + EMMC_ISO_pfunc4);
	cr_writel((cr_readl(0x9804E000 + EMMC_ISO_pfunc5)&0xff03f03f)|(data_drv<<6)|(data_drv<<9)|(data_drv<<18)|(data_drv<<21),
								0x9804E000 + EMMC_ISO_pfunc5);
	CP15ISB;
	sync();
}

static void
emmc_info_show(void)
{
	MMCPRINTF("%s(%u)\n",__func__,__LINE__);
	MMCPRINTF("EMMC SYS_PLL_EMMC1=0x%08x SYS_PLL_EMMC2=0x%08x \nSYS_PLL_EMMC3=0x%08x SYS_PLL_EMMC4=0x%08x HOST_CONTROL2_REG=0x%08x\n \
		PRESENT_STATE_REG=0x%08x  HOST CONTROL1 REG=0x%08x TRANSFER_MODE_REG=0x%08x \n EMMC_CKGEN_CTL=0x%08x EMMC_DQS_CTRL1=0x%08x \n",
	cr_readl(SYS_PLL_EMMC1), cr_readl(SYS_PLL_EMMC2), cr_readl(SYS_PLL_EMMC3), cr_readl(SYS_PLL_EMMC4), cr_readw(EMMC_HOST_CTRL2_R),
	cr_readl(EMMC_PSTATE_REG), cr_readb(EMMC_HOST_CTRL1_R), cr_readw(EMMC_XFER_MODE_R), cr_readl(EMMC_CKGEN_CTL), cr_readl(EMMC_DQS_CTRL1));
}

void rtkemmc_host_reset(void)
{
	cr_writeb(0x07, EMMC_SW_RST_R);      //9801202f, Software Reset Register
	sync();

	cr_writeb(0x0e, EMMC_TOUT_CTRL_R);      //9801202e, Timeout Control register
	sync();

	cr_writew(0x200, EMMC_BLOCKSIZE_R);      //98012004, block size = 512Byte
	sync();

	cr_writew(0xfeff, EMMC_NORMAL_INT_STAT_EN_R);    //98012034, enable all Normal Interrupt Status register
	sync();

	cr_writew(EMMC_ALL_ERR_STAT_EN, EMMC_ERROR_INT_STAT_EN_R); //98012036, enable all error Interrupt Status register
	sync();

	cr_writew(0xfeff, EMMC_NORMAL_INT_SIGNAL_EN_R); //98012038, enable all Normal SIGNAL Interrupt  register
	sync();

	cr_writew(EMMC_ALL_ERR_SIGNAL_EN, EMMC_ERROR_INT_SIGNAL_EN_R);      //9801203a, enable all error SIGNAL Interrupt register
	sync();
}

void phase(u32 VP0, u32 VP1){

	//phase selection
	if( (VP0==0xff) & (VP1==0xff)){
		MMCPRINTF("phase VP0 and VP1 no change \n");
	}
	else if( (VP0!=0xff) & (VP1==0xff)){
		MMCPRINTF("phase VP0=%x, VP1 no change \n", VP0);
		//change clock to 4MHz
		cr_writel(cr_readl(EMMC_CKGEN_CTL)|0x70000, EMMC_CKGEN_CTL);
		sync();
		//reset pll
		cr_writel(cr_readl(SYS_PLL_EMMC1)&0xfffffffd, SYS_PLL_EMMC1);
		sync();
		//vp0 phase:0x0~0x1f
		cr_writel( (cr_readl(SYS_PLL_EMMC1)&0xffffff07) | ((VP0<<3)), SYS_PLL_EMMC1);
		sync();
		//release reset pll
		cr_writel(cr_readl(SYS_PLL_EMMC1)|0x2, SYS_PLL_EMMC1);
		sync();
		//change clock to PLL
		cr_writel(cr_readl(EMMC_CKGEN_CTL)&0xfff8ffff, EMMC_CKGEN_CTL);
		sync();
	}
	else if((VP0==0xff) & (VP1!=0xff)){
		MMCPRINTF("phase VP0 no change, VP1=0x%x \n", VP1);
		cr_writel(cr_readl(EMMC_CKGEN_CTL)|0x70000, EMMC_CKGEN_CTL);
		sync();
		//reset pll
		cr_writel(cr_readl(SYS_PLL_EMMC1)&0xfffffffd, SYS_PLL_EMMC1);
		sync();
		//vp1 phase:0x0~0x1f
		cr_writel( (cr_readl(SYS_PLL_EMMC1)&0xffffe0ff) | ((VP1<<8)), SYS_PLL_EMMC1);
		sync();
		//release reset pll
		cr_writel(cr_readl(SYS_PLL_EMMC1)|0x2, SYS_PLL_EMMC1);
		sync();
		//change clock to PLL
		cr_writel(cr_readl(EMMC_CKGEN_CTL)&0xfff8ffff, EMMC_CKGEN_CTL);
		sync();
	}
	else{
		MMCPRINTF("phase VP0=0x%x, VP1=0x%x \n", VP0, VP1);
		cr_writel(cr_readl(EMMC_CKGEN_CTL)|0x70000, EMMC_CKGEN_CTL);
		sync();
		//reset pll
		cr_writel(cr_readl(SYS_PLL_EMMC1)&0xfffffffd, SYS_PLL_EMMC1);
		sync();
		//vp0 phase:0x0~0x1f
		cr_writel( (cr_readl(SYS_PLL_EMMC1)&0xffffff07) | ((VP0<<3)), SYS_PLL_EMMC1);
		sync();
		//vp1 phase:0x0~0x1f
		cr_writel( (cr_readl(SYS_PLL_EMMC1)&0xffffe0ff) | ((VP1<<8)), SYS_PLL_EMMC1);
		sync();
		//release reset pll
		cr_writel(cr_readl(SYS_PLL_EMMC1)|0x2, SYS_PLL_EMMC1);
		sync();
		//change clock to PLL
		cr_writel(cr_readl(EMMC_CKGEN_CTL)&0xfff8ffff, EMMC_CKGEN_CTL);
		sync();
	}
	udelay(100);
	sync();
}

int search_best(u32 window, u32 range){
	u32 i, j, k, max;
	u32 window_temp[32];
	u32 window_start[32];
	u32 window_end[32];
	u32 window_max=0;
	u32 window_best=0;
	u32 parse_end=1;
	for( i=0; i<0x20; i++ ){
		window_temp[i]=0;
		window_start[i]=0;
		window_end[i]=-1;
	}
	j=1;    i=0;    k=0;    max=0;
	while((i<0x1f) && (k<0x1f)){
		parse_end=0;
		for( i=window_end[j-1]+1; i<0x20; i++ ){
			if (((window>>i)&1)==1 ){
				window_start[j]=i;
				break;
			}
		}
		if( i==0x20){
			break;
		}
		for( k=window_start[j]+1; k<0x20; k++ ){
			if(((window>>k)&1)==0){
				window_end[j]=k-1;
				parse_end=1;
				break;
			}
		}
		if(parse_end==0){
			window_end[j]=0x1f;
		}
		j++;
	}
	for(i=1; i<j; i++){
		window_temp[i]= window_end[i]-window_start[i]+1;
	}
	if((((window>>(0x20-range))&1)==1)&&(((window>>0x1f)&1)==1))
	{
		window_temp[1]=window_temp[1]+window_temp[j-1];
		window_start[1]=window_start[j-1];
	}
	for(i=1; i<j; i++){
		if(window_temp[i]>window_max){
			window_max=window_temp[i];
			max=i;
		}
	}
	if(window==0xffffffff){
		window_best=0x10;
	}
	else if((window==0xffff0000)&&(range==0x10)){
		window_best=0x18;
	}
	else if(((((window>>(0x20-range))&1)==1)&&(((window>>0x1f)&1)==1))&&(max==1)){
		window_best=(((window_start[max]+window_end[max]+range)/2)&0x1f)|(0x20-range);
	}
	else {
		window_best=((window_start[max]+window_end[max])/2)&0x1f;
	}

	MMCPRINTF("window start=0x%x \n", window_start[max]);
	MMCPRINTF("window end=0x%x \n", window_end[max]);
	MMCPRINTF("window best=0x%x \n", window_best);
	return window_best;
}

int mmc_Tuning_HS200(void){
	volatile UINT32 TX_window=0xffffffff;
	volatile int TX_best=0xff;
	volatile UINT32 RX_window=0xffffffff;
	volatile int RX_best=0xff;
	volatile int i=0;

	gCurrentBootMode= MODE_SD30;
	#ifdef MMC_DEBUG
	printf("[LY]hs200 gCurrentBootMode =%d\n",gCurrentBootMode);
	#endif

	frequency(0xa6, 0x0); //200Mhz
	rtkemmc_set_pad_driving(0x4, 0x4, 0x4, 0x4);
	phase(0, 0);	//VP0, VP1 phas

	cr_writel(cr_readl(EMMC_OTHER1)&0xffffffff7, EMMC_OTHER1);
	udelay(1);
	cr_writel(cr_readl(EMMC_OTHER1)|0x8, EMMC_OTHER1);

	mdelay(5);
	emmc_info_show();
	sync();

	#ifdef MMC_DEBUG
	MMCPRINTF("==============Start HS200 TX Tuning round==================\n");
	#endif
	for(i=0x0; i<0x20; i++){
		phase(i, 0xff);
		cr_writel(cr_readl(EMMC_OTHER1)&0xffffffff7, EMMC_OTHER1);
		udelay(1);
		cr_writel(cr_readl(EMMC_OTHER1)|0x8, EMMC_OTHER1);

		#ifdef MMC_DEBUG
		printf("phase =0x%x \n", i);
		#endif
		if((rtkemmc_send_cmd35()&0x1) != 0)
		{
			TX_window= TX_window&(~(1<<i));
		}
	}
	TX_best = search_best(TX_window,0x20);
	printf("TX_WINDOW=0x%x, TX_best=0x%x \n",TX_window, TX_best);
	if (TX_window == 0x0)
	{
		printf("[LY] HS200 TX tuning fail \n");
		return -1;
	}
	phase(TX_best, 0xff);
	cr_writel(cr_readl(EMMC_OTHER1)&0xffffffff7, EMMC_OTHER1);
	udelay(1);
	cr_writel(cr_readl(EMMC_OTHER1)|0x8, EMMC_OTHER1);

	#ifdef MMC_DEBUG
	MMCPRINTF("++++++++++++++++++ Start HS200 RX Tuning ++++++++++++++++++\n");
	#endif

	for(i=0x0; i<0x20; i++){
		phase(0xff, i);
		cr_writel(cr_readl(EMMC_OTHER1)&0xffffffff7, EMMC_OTHER1);
		udelay(1);
		cr_writel(cr_readl(EMMC_OTHER1)|0x8, EMMC_OTHER1);
		#ifdef MMC_DEBUG
		printf("phase =0x%x \n", i);
		#endif
		if(rtkemmc_send_cmd21()!=0)
		{
			RX_window= RX_window&(~(1<<i));
		}
	}
	RX_best = search_best(RX_window,0x20);
	printf("RX_WINDOW=0x%x, RX_best=0x%x \n",RX_window, RX_best);
	if (RX_window == 0x0)
	{
		printf("[LY] HS200 RX tuning fail \n");
		return -2;
	}
	phase( 0xff, RX_best);
	cr_writel(cr_readl(EMMC_OTHER1)&0xffffffff7, EMMC_OTHER1);
	udelay(1);
	cr_writel(cr_readl(EMMC_OTHER1)|0x8, EMMC_OTHER1);

	cr_writel(cr_readl(EMMC_OTHER1)&0xfffffffe, EMMC_OTHER1);        //enable L4 gated after HS200 finished

	return 0;
}

int mmc_Select_SDR50_Push_Sample(void){
	gCurrentBootMode = MODE_SD20;
	#ifdef MMC_DEBUG
	printf("[LY]sdr gCurrentBootMode =%d\n",gCurrentBootMode);
	#endif

	frequency(0x57, 0x1);
	rtkemmc_set_pad_driving(0x0, 0x0, 0x0, 0x0);
	cr_writel(cr_readl(EMMC_OTHER1)&0xfffffffe, EMMC_OTHER1);        //enable L4 gated after SDR50 finished
	mdelay(5);
	sync();

	return 0;
}

int sample_ctl_switch(int cmd_idx)
{
	int err=0;
	//tbd
	return err;
}

#if 0
/*******************************************************
 *
 *******************************************************/
int rtk_micron_eMMC_write( unsigned int blk_addr, unsigned int data_size, unsigned int * buffer )
{
    int ret_err = 0;
    unsigned int total_blk_cont;
    unsigned int curr_xfer_blk_cont;
    unsigned int address;
    unsigned int curr_blk_addr;
    e_device_type * card = &emmc_card;
    UINT8 ret_state=0;
    UINT32 bRetry=0, resp = 0;
    UINT32 retry_cnt=0, retry_cnt1=0, retry_cnt2=0, retry_cnt3=0;
    int err1=0;

    RDPRINTF("\nemmc:%s blk_addr=0x%08x, data_size=0x%08x, buffer=0x%08x, addressing=%d\n", __FUNCTION__, blk_addr, data_size, buffer, card->sector_addressing);

    total_blk_cont = data_size>>9;
    if(data_size & 0x1ff) {
        total_blk_cont+=1;
    }

	curr_blk_addr = blk_addr;
#ifndef FORCE_SECTOR_MODE
	if( card->sector_addressing ) {
		address = curr_blk_addr;
	}
	else {
		address = curr_blk_addr << 9;
	}
#else
    address = curr_blk_addr;
#endif

#ifdef EMMC_SPLIT_BLK
	while( total_blk_cont ) {
		if( total_blk_cont > EMMC_MAX_XFER_BLKCNT ) {
			curr_xfer_blk_cont = EMMC_MAX_XFER_BLKCNT;
		}
		else {
			curr_xfer_blk_cont = total_blk_cont;
		}

		flush_cache((unsigned long)buffer, curr_xfer_blk_cont << 9);
		ret_err = rtkemmc_Stream( address, curr_xfer_blk_cont, MMC_CMD_MICRON_60, (uintptr_t)buffer, 0 );

		if( ret_err ) {
			return 0;
		}
		total_blk_cont -= curr_xfer_blk_cont;
		buffer += (curr_xfer_blk_cont<<(9-2));
		address += curr_xfer_blk_cont;
	}

	total_blk_cont = data_size>>9;
    if( data_size & 0x1ff ) {
        total_blk_cont+=1;
    }

    return total_blk_cont;
#else
RETRY_RD_CMD:
    flush_cache((unsigned long)buffer, data_size);

    ret_err = rtkemmc_Stream( address, total_blk_cont, MMC_CMD_MICRON_60, (uintptr_t)buffer,0 );  //tbd : not verify

	if (ret_err)
	{
		if (retry_cnt2++ < MAX_CMD_RETRY_COUNT)
		{
			sample_ctl_switch(-1);
			goto RETRY_RD_CMD;
		}
		return ret_err;
	}

    /* To wait status change complete */
    bRetry=0;
    retry_cnt=0;
    retry_cnt1=0;
    while(1)
    {
        err1 = rtkemmc_send_status(&resp,1,0);
        //1. if cmd sent error, try again
        if (err1)
        {
            #ifdef THIS_IS_FLASH_WRITE_U_ENV
	        prints("retry - case 1\n");
            #else
            MMCPRINTF("retry - case 1\n");
            #endif
            sample_ctl_switch(-1);
            if (retry_cnt++ > MAX_CMD_RETRY_COUNT*2)
                return ERR_EMMC_SEND_STATUS_RETRY_FAIL;
            mdelay(1);
            continue;
        }
	ret_state = R1_CURRENT_STATE(resp);
        //2. get state
        if (ret_state != STATE_TRAN)
        {
            #ifdef THIS_IS_FLASH_WRITE_U_ENV
	        prints("retry - case 2\n");
            #else
            MMCPRINTF("retry - case 2\n");
            #endif
            bRetry = 1;
            if (retry_cnt1++ > MAX_CMD_RETRY_COUNT*2)
                return ERR_EMMC_SEND_RW_CMD_RETRY_FAIL;
            rtkemmc_stop_transmission(&emmc_card,0);
            mdelay(1000);
        }
        else
        {
            //out peaceful
            if (bRetry == 0)
            {
                #ifdef THIS_IS_FLASH_WRITE_U_ENV
	            prints("retry - case 3\n");
                #else
                MMCPRINTF("retry - case 3\n");
                #endif
                return !ret_err ?  total_blk_cont : 0;
            }
            else
            {
                #ifdef THIS_IS_FLASH_WRITE_U_ENV
	            prints("retry - case 4\n");
                #else
                MMCPRINTF("retry - case 4\n");
                #endif
                retry_cnt2 = 0;
                if (retry_cnt3++ > MAX_CMD_RETRY_COUNT*2)
                    return ERR_EMMC_SEND_RW_CMD_RETRY_FAIL;
                goto RETRY_RD_CMD;
            }
        }
    }

#endif
	printf("The err1 is %d. The retry_cnt is %d. The retry_cnt1 is %d. The retry_cnt2 is %d. The retry_cnt3 is %d.\n",err1,retry_cnt,retry_cnt1,retry_cnt2,retry_cnt3);
	printf("The ret_state, bRetry, resp are %c, %d, %d\n.",ret_state,bRetry,resp);
	printf("The card addressing=%d\n",card->sector_addressing);

    return !ret_err ?  total_blk_cont : 0;
}

/*******************************************************
 *
 *******************************************************/
int rtk_micron_eMMC_read( unsigned int blk_addr, unsigned int data_size, unsigned int * buffer )
{
    int ret_err = 0;
    unsigned int total_blk_cont;
    unsigned int curr_xfer_blk_cont;
    unsigned int address;
    e_device_type * card = &emmc_card;
    UINT8 ret_state=0;
    UINT32 bRetry=0, resp = 0;
    UINT32 retry_cnt=0, retry_cnt1=0, retry_cnt2=0, retry_cnt3=0;
    int err1=0;

    RDPRINTF("\nemmc:%s blk_addr=0x%08x, data_size=0x%08x, buffer=0x%08x, addressing=%d\n", __FUNCTION__, blk_addr, data_size, buffer, card->sector_addressing);

    total_blk_cont = data_size>>9;
    if( data_size & 0x1ff ) {
        total_blk_cont+=1;
    }

#ifndef FORCE_SECTOR_MODE
	if( card->sector_addressing ) {
		address = blk_addr;
	}
	else {
		address = blk_addr << 9;
	}
#else
    address = blk_addr;
#endif
#ifdef EMMC_SPLIT_BLK
	while( total_blk_cont ) {
		if( total_blk_cont > EMMC_MAX_XFER_BLKCNT ) {
			curr_xfer_blk_cont = EMMC_MAX_XFER_BLKCNT;
		}
		else {
			curr_xfer_blk_cont = total_blk_cont;
		}

		flush_cache((unsigned long)buffer, curr_xfer_blk_cont << 9);
		ret_err = rtkemmc_Stream( address, curr_xfer_blk_cont, MMC_CMD_MICRON_61, (uintptr_t)buffer,0 );

		if( ret_err ) {
			return 0;
		}
		total_blk_cont -= curr_xfer_blk_cont;
		buffer += (curr_xfer_blk_cont<<(9-2));
		address += curr_xfer_blk_cont;

		//EXECUTE_CUSTOMIZE_FUNC(0); // insert execute customer callback at here

	}

	total_blk_cont = data_size>>9;
    if( data_size & 0x1ff ) {
        total_blk_cont+=1;
    }
	return total_blk_cont;
#else
RETRY_RD_CMD:
    flush_cache((unsigned long)buffer, data_size);

    ret_err = rtkemmc_Stream( address, total_blk_cont, MMC_CMD_MICRON_61, buffer,0 );

	if (ret_err)
	{
		if (retry_cnt2++ < MAX_CMD_RETRY_COUNT*2)
		{
			sample_ctl_switch(-1);
			goto RETRY_RD_CMD;
		}
		return ret_err;
	}

    /* To wait status change complete */
    bRetry=0;
    retry_cnt=0;
    retry_cnt1=0;
    while(1)
    {
        err1 = rtkemmc_send_status(&resp,1,0);
        //1. if cmd sent error, try again
        if (err1)
        {
            sample_ctl_switch(-1);
            if (retry_cnt++ > MAX_CMD_RETRY_COUNT*2)
                return ERR_EMMC_SEND_STATUS_RETRY_FAIL;
            mdelay(1);
            continue;
        }
	ret_state = R1_CURRENT_STATE(resp);
        //2. get state
        if (ret_state != STATE_TRAN)
        {
            bRetry = 1;
            if (retry_cnt1++ > MAX_CMD_RETRY_COUNT*2)
                return ERR_EMMC_SEND_RW_CMD_RETRY_FAIL;
            rtkemmc_stop_transmission(&emmc_card,0);
            mdelay(1000);
        }
        else
        {
            //out peaceful
            if (bRetry == 0)
                return !ret_err ?  total_blk_cont : 0;
            else
            {
                retry_cnt2 = 0;
                if (retry_cnt3++ > MAX_CMD_RETRY_COUNT*2)
                    return ERR_EMMC_SEND_RW_CMD_RETRY_FAIL;
                goto RETRY_RD_CMD;
            }
        }
    }
#endif
	printf("The err1 is %d. The retry_cnt is %d. The retry_cnt1 is %d. The retry_cnt2 is %d. The retry_cnt3 is %d.\n",err1,retry_cnt,retry_cnt1,retry_cnt2,retry_cnt3);
	printf("The ret_state, bRetry, resp are %c, %d, %d\n.",ret_state,bRetry,resp);
	printf("The card addressing=%d\n",card->sector_addressing);

    return !ret_err ?  total_blk_cont : 0;
}
#endif

/*******************************************************
 *
 *******************************************************/
int rtk_eMMC_write( unsigned int blk_addr, unsigned int data_size, unsigned int * buffer )
{
	int ret_err = 0;
	unsigned int total_blk_cont;
	unsigned int curr_xfer_blk_cont;
	unsigned int address;
	unsigned int curr_blk_addr;
	e_device_type * card = &emmc_card;
	UINT8 ret_state=0;
	UINT32 bRetry=0, resp = 0;
	UINT32 retry_cnt=0, retry_cnt1=0, retry_cnt2=0, retry_cnt3=0;
	int err1=0;

	RDPRINTF("\nemmc:%s blk_addr=0x%08x, data_size=0x%08x, buffer=0x%08x, addressing=%d\n", __FUNCTION__, blk_addr, data_size, buffer, card->sector_addressing);
	if( !mmc_ready_to_use ) {
		MMCPRINTF("emmc: not ready to use\n");
	}

	total_blk_cont = data_size>>9;
	if(data_size & 0x1ff) {
		total_blk_cont+=1;
	}

	curr_blk_addr = blk_addr;
#ifndef FORCE_SECTOR_MODE
	if( card->sector_addressing ) {
		address = curr_blk_addr;
	}
	else {
		address = curr_blk_addr << 9;
	}
#else
	address = curr_blk_addr;
#endif

#ifdef EMMC_SPLIT_BLK
	while( total_blk_cont ) {
		if( total_blk_cont > EMMC_MAX_XFER_BLKCNT ) {
			curr_xfer_blk_cont = EMMC_MAX_XFER_BLKCNT;
		}
		else {
			curr_xfer_blk_cont = total_blk_cont;
		}

		flush_cache((unsigned long)buffer, curr_xfer_blk_cont << 9);

		if (IS_SRAM_ADDR((uintptr_t)buffer))
			ret_err = -1; //cannot allow r/w to sram in bootcode
		else{
			if (curr_xfer_blk_cont == 1)
				ret_err = rtkemmc_Stream(MMC_WRITE_BLOCK, address, (uintptr_t)buffer, curr_xfer_blk_cont << 9,0);
			else
				ret_err = rtkemmc_Stream(MMC_WRITE_MULTIPLE_BLOCK, address, (uintptr_t)buffer, curr_xfer_blk_cont << 9,0);
		}

		if( ret_err ) {
			return 0;
		}
		total_blk_cont -= curr_xfer_blk_cont;
		buffer += (curr_xfer_blk_cont<<(9-2));
		address += curr_xfer_blk_cont;
	}

	total_blk_cont = data_size>>9;
	if( data_size & 0x1ff ) {
		total_blk_cont+=1;
	}

	return total_blk_cont;
#else
RETRY_RD_CMD:
	flush_cache((unsigned long)buffer, data_size);

	if (IS_SRAM_ADDR((uintptr_t)buffer))
		ret_err = -1; //cannot allow r/w to sram in bootcode
	else{
		if (curr_xfer_blk_cont == 1)
			ret_err = rtkemmc_Stream(MMC_WRITE_BLOCK, address, buffer, total_blk_cont << 9,0);
		else
			ret_err = rtkemmc_Stream(MMC_WRITE_MULTIPLE_BLOCK, address, buffer, total_blk_cont << 9,0);
	}

	if (ret_err)
	{
		if (retry_cnt2++ < MAX_CMD_RETRY_COUNT*2)
		{
			sample_ctl_switch(-1);
			goto RETRY_RD_CMD;
		}
		return ret_err;
	}

	/* To wait status change complete */
	bRetry=0;
	retry_cnt=0;
	retry_cnt1=0;
	while(1)
	{
		err1 = rtkemmc_send_status(&resp,1,0);
		//1. if cmd sent error, try again
		if (err1)
		{
			#ifdef THIS_IS_FLASH_WRITE_U_ENV
			prints("retry - case 1\n");
			#else
			MMCPRINTF("retry - case 1\n");
			#endif
			sample_ctl_switch(-1);
			if (retry_cnt++ > MAX_CMD_RETRY_COUNT*2)
				return ERR_EMMC_SEND_STATUS_RETRY_FAIL;
			mdelay(1);
			continue;
		}
		ret_state = R1_CURRENT_STATE(resp);
		//2. get state
		if (ret_state != STATE_TRAN)
		{
			#ifdef THIS_IS_FLASH_WRITE_U_ENV
			prints("retry - case 2\n");
			#else
			MMCPRINTF("retry - case 2\n");
			#endif
			bRetry = 1;
			if (retry_cnt1++ > MAX_CMD_RETRY_COUNT*2)
				return ERR_EMMC_SEND_RW_CMD_RETRY_FAIL;
			rtkemmc_stop_transmission(&emmc_card,0);
			mdelay(1000);
		}
		else
		{
			//out peaceful
			if (bRetry == 0)
			{
				#ifdef THIS_IS_FLASH_WRITE_U_ENV
				prints("retry - case 3\n");
				#else
				MMCPRINTF("retry - case 3\n");
				#endif
				return !ret_err ?  total_blk_cont : 0;
			}
			else
			{
				#ifdef THIS_IS_FLASH_WRITE_U_ENV
				prints("retry - case 4\n");
				#else
				MMCPRINTF("retry - case 4\n");
				#endif
				retry_cnt2 = 0;
				if (retry_cnt3++ > MAX_CMD_RETRY_COUNT*2)
					return ERR_EMMC_SEND_RW_CMD_RETRY_FAIL;
				goto RETRY_RD_CMD;
			}
		}
	}

#endif
	printf("The err1 is %d. The retry_cnt is %d. The retry_cnt1 is %d. The retry_cnt2 is %d. The retry_cnt3 is %d.\n",err1,retry_cnt,retry_cnt1,retry_cnt2,retry_cnt3);
	printf("The ret_state, bRetry, resp are %c, %d, %d\n.",ret_state,bRetry,resp);
	printf("The card addressing=%d\n",card->sector_addressing);

	return !ret_err ?  total_blk_cont : 0;
}

/*******************************************************
 *
 *******************************************************/
#if 0
int rtkemmc_send_wp_type( unsigned int blk_addr, unsigned int * buffer )
{
    int ret_err = 0;
	int i = 0, j = 0;
	unsigned int tmp = 0;
    unsigned int address;
    e_device_type * card = &emmc_card;
    UINT32 resp = 0;

    RDPRINTF("\nemmc:%s blk_addr=0x%08x, data_size=0x%08x, buffer=0x%08x, addressing=%d\n", __FUNCTION__, blk_addr, data_size, buffer, card->sector_addressing);

    if( !mmc_ready_to_use ) {
         RDPRINTF("emmc: not ready to use\n");
    }

#ifndef FORCE_SECTOR_MODE
	if( card->sector_addressing ) {
		address = blk_addr;
	}
	else {
		address = blk_addr << 9;
	}
#else
    address = blk_addr;
#endif

	flush_cache((unsigned long)buffer, 8);
    if (IS_SRAM_ADDR((uintptr_t)(buffer)))
	ret_err = -1; //cannot allow r/w to sram in bootcode
    else{
	ret_err = rtkemmc_Stream(0xa0000340|MMC_SEND_WRITE_PROT_TYPE, address, (uintptr_t)buffer, 8,0);

    }
	if( ret_err ) {
		return 0;
	}

	CP15ISB;
	sync();
	printf("00:Unprotected 01:Temporary Write Protect\n10:Power-on Write Protect 11:Permanent Write Protect\n");
	//the last 2 bits is the first addressed wp
	for (i = 1; i >= 0; i--){
		printf("0x%x\n",buffer[i]);
		#ifdef CONFIG_UBOOT_DEFAULT
		buffer[i] = swap_endian(buffer[i]);
		#else /* !CONFIG_UBOOT_DEFAULT */
		buffer[i] = (buffer[i] & 0xff000000)>>24|
					(buffer[i] & 0x00ff0000)>>8|
					(buffer[i] & 0x0000ff00)<<8|
					(buffer[i] & 0x000000ff)<<24;
		#endif /* CONFIG_UBOOT_DEFAULT */
		for (j = 0; j < 16; j++){
			tmp =  buffer[i] >> (j * 2);
			printf("%u%u ", (tmp & 2) >> 1, tmp & 1);
		}
		printf("\n");
	}

	rtkemmc_send_status(&resp,1,0);
	//workaround for 1295 a00 non-512B data transfer
	card_stop();
	printf("The card addressing=%d\n",card->sector_addressing);
	return 1;

}
#endif

#if 0
/*******************************************************
 *
 *******************************************************/
int do_rtkemmc (struct cmd_tbl *cmdtp, int flag, int argc, char * const argv[])
{
	int ret = CMD_RET_USAGE;
	unsigned int blk_addr, byte_size;
	void * mem_addr;

	if( argc == 5 ) {
		mem_addr   = (void *)simple_strtoul(argv[2], NULL, 16);
		blk_addr   = simple_strtoul(argv[3], NULL, 16);
		byte_size  = simple_strtoul(argv[4], NULL, 16);
		if( strncmp( argv[1], "read", 4 ) == 0 ) {
			ret = rtk_eMMC_read( blk_addr, byte_size, mem_addr);
			return ((ret==0) ? 1 : 0);
		}
		else if( strncmp( argv[1], "write", 5 ) == 0 ) {
			ret = rtk_eMMC_write( blk_addr, byte_size, mem_addr);
			return ((ret==0) ? 1 : 0);
		}
	}

	if( argc == 4 ) {
		mem_addr   = (void *)simple_strtoul(argv[2], NULL, 16);
		blk_addr   = simple_strtoul(argv[3], NULL, 16);
		if( strncmp( argv[1], "send_wp_type", 12 ) == 0 ) {
			ret = rtkemmc_send_wp_type( blk_addr, mem_addr);
			return ret;
		}
	}
	if( argc == 3 ) {

		blk_addr   = simple_strtoul(argv[2], NULL, 16);

		if( strncmp( argv[1], "set_wp", 6 ) == 0 ) {
			ret = rtkemmc_set_wp( blk_addr, 1, 0);
			return ret;
		}
		else if( strncmp( argv[1], "clr_wp", 6 ) == 0 ) {
			ret = rtkemmc_clr_wp( blk_addr, 1, 0);
			return ret;
		}

	}
	return  ret;
}

/*******************************************************
 *
 *******************************************************/
U_BOOT_CMD(
	rtkemmc, 5, 0, do_rtkemmc,
	"RTK EMMC functions",
	" - rtkemmc read/write/\n"
	"rtkemmc read dma_addr blk_addr byte_size\n"
	"rtkemmc write dma_addr blk_addr byte_size\n"
	"rtkemmc set_wp blk_addr\n"
	"rtkemmc clr_wp blk_addr\n"
	"rtkemmc send_wp_type dma_addr blk_addr\n"
	""
);
#endif
