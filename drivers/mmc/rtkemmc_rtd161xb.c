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

#include <mmc.h>
#include <memalign.h>
#include <dm.h>
#include <linux/delay.h>
#include <asm/arch/rtkemmc.h>
#include <asm/arch/platform_lib/board/gpio.h>
#include <asm/arch/rbus/crt_reg.h>
#include <asm/arch/cpu.h>
#include <linux/io.h>
#include "rtkemmc_generic.h"
#define __RTKEMMC_C__

unsigned char *dummy_512B;
unsigned char *ext_csd = NULL;
unsigned int emmc_cid[4]={0};
char *mmc_name = "RTD161xb eMMC";
unsigned int *rw_descriptor;
unsigned int wait_for_complete(unsigned int cmd, unsigned int bIgnore);
void phase(u32 VP0, u32 VP1);

struct rtk_mmc_plat {
	struct mmc_config cfg;
	struct mmc mmc;
};

void wait_done(volatile UINT32 *addr, UINT32 mask, UINT32 value, unsigned int cmd){
	int n = 0;
	while (1)
	{
		if(((*addr) &mask) == value)
			break;

		if((readw(EMMC_NORMAL_INT_STAT_R) & 0x8000)!=0) {
                        break;
                }

		if(n++ > 3000)
		{
			printf("Time out \n");
			printf("%s: cmd_idx=%d, addr=0x%08x, mask=0x%08x, value=0x%08x, pad_mux=0x%x, 0x98012030=0x%x, 0x98012032=0x%x\n",
                                __func__, cmd, PTR_U32(addr), mask, value, readl(0x9804e000), readw(EMMC_NORMAL_INT_STAT_R), readw(EMMC_ERROR_INT_STAT_R));
			return;
		}
		mdelay(1);
	}
}

static void rtkemmc_invalidate_cache(unsigned long start, unsigned long size)
{
        unsigned long from = ALIGN_DOWN(start, CONFIG_SYS_CACHELINE_SIZE);
        unsigned long to = ALIGN(start + size, CONFIG_SYS_CACHELINE_SIZE);
        invalidate_dcache_range(from, to);
}

static void rtkemmc_flush_cache(unsigned long start, unsigned long size)
{
	unsigned long from = ALIGN_DOWN(start, CONFIG_SYS_CACHELINE_SIZE);
	unsigned long to = ALIGN(start + size, CONFIG_SYS_CACHELINE_SIZE);
	flush_dcache_range(from, to);
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
static e_device_type emmc_card;
static unsigned int sys_rsp17_addr;
static unsigned char* ptr_ext_csd;
int swap_endian(UINT32 input);
int gCurrentBootDeviceType=0;

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

struct emmc_error_entry emmc_error_table[] = {
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

UINT32 check_error(int bIgnore)
{
	unsigned int error;
	struct emmc_error_entry *p;

	error = readw(EMMC_ERROR_INT_STAT_R);
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

int rtk_eMMC_read(unsigned int blk_addr, unsigned int data_size, unsigned int * buffer)
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
	}

	total_blk_cont = data_size>>9;
    if( data_size & 0x1ff ) {
        total_blk_cont+=1;
    }
	return total_blk_cont;
#else
RETRY_RD_CMD:
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

void pll_setup(unsigned int freq)
{
	u32 sscpll_icp = 1;
	unsigned int tmp_val=0;

	writel(readl(SYS_PLL_EMMC1)&~(1<<1), SYS_PLL_EMMC1);      //reset pll, phrt0=0
	udelay(10);      //

	tmp_val = readl(SYS_PLL_EMMC4) & 0x06;
	writel(tmp_val, SYS_PLL_EMMC4);

	tmp_val = (readl(SYS_PLL_EMMC3) & 0xffff)|(freq<<16);
	writel(tmp_val, SYS_PLL_EMMC3);
	udelay(3);

	if(freq<98)
		sscpll_icp = 0;

	writel((readl(SYS_PLL_EMMC2)&0xfffffc1f)|(sscpll_icp<<5), SYS_PLL_EMMC2); //f=0, rs=5

	tmp_val = readl(SYS_PLL_EMMC4) | 0x01;
	writel(tmp_val, SYS_PLL_EMMC4);
	udelay(60);

	writel(readl(SYS_PLL_EMMC1)|(1<<1), SYS_PLL_EMMC1);       //reset pll, phrt0=1
	udelay(10);

	wait_done((UINT32 *)0x9801255c, 1, 1, 1000); //wait for emmc pll usable
	wait_done((UINT32 *)0x9801202c, 3, 3, 1000);            //wait for clock is active

	writeb(0x6, EMMC_SW_RST_R); //Perform a software reset
	wait_done((UINT32 *)0x9801202c, (0x6<<24), 0x0, 1000); //wait for clear 0x2f bit 1 & 2
}

void frequency(unsigned int  freq, unsigned int  div_ip)
{
	pll_setup(freq);

	writel(readl(SYS_PLL_EMMC1)&0xfffffffd, SYS_PLL_EMMC1);
        udelay(6);

	switch(div_ip) {
		case 1:
			writel(readl(EMMC_CKGEN_CTL)&~(1<<8), EMMC_CKGEN_CTL);
			break;
		case 4:
			writel(readl(EMMC_CKGEN_CTL)|(1<<9), EMMC_CKGEN_CTL);
			writel(readl(EMMC_CKGEN_CTL)|(1<<8), EMMC_CKGEN_CTL);
			break;
		case 512:
			writel(readl(EMMC_CKGEN_CTL)& ~(1<<9), EMMC_CKGEN_CTL);
			writel(readl(EMMC_CKGEN_CTL)|(1<<8), EMMC_CKGEN_CTL);
			break;
		default:
			writel(readl(EMMC_CKGEN_CTL)&~(1<<8), EMMC_CKGEN_CTL);
			break;
	}

	writel(readl(EMMC_CKGEN_CTL)&0xffefffff, EMMC_CKGEN_CTL);
	udelay(6);
	//release reset pll
	writel(readl(SYS_PLL_EMMC1)|0x2, SYS_PLL_EMMC1);
	udelay(6);

	wait_done((UINT32 *)0x9801255c, 0x1, 0x1, 1000); //wait for clear 0x255c bit 0

	wait_done((UINT32 *)0x9801202c, 3, 3, 1000);
	writeb(0x6, EMMC_SW_RST_R); //Perform a software reset
	wait_done((UINT32 *)0x9801202c, (0x6<<24), 0x0, 1000); //wait for clear 0x2f bit 1 & 2

	writel(readl(EMMC_OTHER1)&~(1<<3), EMMC_OTHER1);
	udelay(10); //10us, wait more than 2T emmc interface clock
	writel(readl(EMMC_OTHER1)|(1<<3), EMMC_OTHER1);
	udelay(10); //10us, wait more than 2T emmc interface clock

	printf("switch frequency to 0x%x, divder to 0x%x\n", freq, div_ip);
}

void print_ip_desc(void)
{
	printf("------------------------------>\n");
	printf("EMMC IP_DESC0 = 0x%08x\n", readl(EMMC_IP_DESC0));
	printf("EMMC IP_DESC1 = 0x%08x\n", readl(EMMC_IP_DESC1));
	printf("EMMC IP_DESC2 = 0x%08x\n", readl(EMMC_IP_DESC2));
	printf("EMMC IP_DESC3 = 0x%08x\n", readl(EMMC_IP_DESC3));

	printf("0x98012058 EMMC EMMC_ADMA_SA_LOW_R = 0x%08x\n------------------------------>\n", readl(EMMC_ADMA_SA_LOW_R));
}

void make_ip_des(UINT32 dma_addr, UINT32 dma_length)
{
	UINT32* des_base = rw_descriptor;
	UINT32  blk_cnt;
	UINT32  blk_cnt2;
	UINT32  remain_blk_cnt;
	UINT32  tmp_val;
	unsigned int b1, b2;

	//CMD31 only transfer 8B
	if (dma_length < 512) {
		writel(dma_length, EMMC_BLOCKSIZE_R);
		blk_cnt = 1;

	} else {
		writel(0x200, EMMC_BLOCKSIZE_R);
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
		printf("%s : cmd5 arg=0x%08x\n",__func__,cmd_info->cmd->cmdarg);
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
	case MMC_SET_WRITE_PROT:
	case MMC_CLR_WRITE_PROT:
		cmd_info->cmd_para = (EMMC_RESP_LEN_48B|EMMC_CMD_CHK_RESP_CRC|EMMC_CMD_IDX_CHK_ENABLE);
		cmd_info->rsp_len = 6;
		break;
	case MMC_SEND_WRITE_PROT:
	case MMC_SEND_WRITE_PROT_TYPE:
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

	writew(0, EMMC_ERROR_INT_SIGNAL_EN_R);      //9801203a, enable all error SIGNAL Interrupt register
	CP15ISB;
	sync();

	if((readw(EMMC_ERROR_INT_STAT_R)&
		(EMMC_AUTO_CMD_ERR|EMMC_CMD_IDX_ERR|EMMC_CMD_END_BIT_ERR|EMMC_CMD_CRC_ERR|EMMC_CMD_TOUT_ERR)) !=0){ //check cmd line
#ifdef MMC_DEBUG
		printf("CMD Line error occurs \n");
#endif
		writeb(0x2, EMMC_SW_RST_R); //Perform a software reset
		wait_done((UINT32 *)0x9801202c, (0x2<<24), 0, cmd_idx);
	}
	if((readw(EMMC_ERROR_INT_STAT_R)&
		(EMMC_ADMA_ERR|EMMC_DATA_END_BIT_ERR|EMMC_DATA_CRC_ERR|EMMC_DATA_TOUT_ERR)) !=0){ //check data line
#ifdef MMC_DEBUG
		printf("DAT Line error occurs \n");
#endif
	writeb(0x4, EMMC_SW_RST_R); //Perform a software reset
			wait_done((UINT32 *)0x9801202c, (0x4<<24), 0x0, cmd_idx); //wait for clear 0x2f bit 2
	}

retry_L1:
	//synchronous abort: stop host dma
	writeb(0x1, EMMC_BGAP_CTRL_R); //stop emmc transactions  <-------????????????????????????????
	wait_done((UINT32 *)EMMC_NORMAL_INT_STAT_R, 0x2, 0x2, cmd_idx); //wait for xfer complete
	writew(0x2, EMMC_NORMAL_INT_STAT_R); //clear transfer complete status

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

	writeb(0x6, EMMC_SW_RST_R); //Perform a software reset
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
		printf("Tuning_cmd13_ERROR: register 0x98012030=0x%x, 0x98012032= 0x%08x\n", readw(EMMC_NORMAL_INT_STAT_R), readw(EMMC_ERROR_INT_STAT_R));
#endif
		if((readw(EMMC_ERROR_INT_STAT_R)&
			(EMMC_AUTO_CMD_ERR|EMMC_CMD_IDX_ERR|EMMC_CMD_END_BIT_ERR|EMMC_CMD_CRC_ERR|EMMC_CMD_TOUT_ERR))!=0){ //check cmd line
#ifdef MMC_DEBUG
			printf("CMD Line error occurs \n");
#endif
			writeb(0x2, EMMC_SW_RST_R); //Perform a software reset
			wait_done((UINT32 *)0x9801202c, (0x2<<24), 0x0, MMC_SEND_STATUS); //wait for clear 0x2f bit 1
		}
		if((readw(EMMC_ERROR_INT_STAT_R)&
			(EMMC_ADMA_ERR|EMMC_DATA_END_BIT_ERR|EMMC_DATA_CRC_ERR|EMMC_DATA_TOUT_ERR)) !=0){ //check data line
#ifdef MMC_DEBUG
			printf("DAT Line error occurs \n");
#endif
			writeb(0x4, EMMC_SW_RST_R); //Perform a software reset
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
		readw(EMMC_NORMAL_INT_STAT_R), readw(EMMC_ERROR_INT_STAT_R));
#endif
		if((readw(EMMC_ERROR_INT_STAT_R)&
			(EMMC_AUTO_CMD_ERR|EMMC_CMD_IDX_ERR|EMMC_CMD_END_BIT_ERR|EMMC_CMD_CRC_ERR|EMMC_CMD_TOUT_ERR))!=0){ //check cmd line
#ifdef MMC_DEBUG
			printf("CMD Line error occurs \n");
#endif
			writeb(0x2, EMMC_SW_RST_R); //Perform a software reset
			wait_done((UINT32 *)0x9801202c, (0x2<<24), 0x0, MMC_ERASE_GROUP_START); //wait for clear 0x2f bit 1
		}
		if((readw(EMMC_ERROR_INT_STAT_R)&
			(EMMC_ADMA_ERR|EMMC_DATA_END_BIT_ERR|EMMC_DATA_CRC_ERR|EMMC_DATA_TOUT_ERR)) !=0){ //check data line
#ifdef MMC_DEBUG
			printf("DAT Line error occurs \n");
#endif
			writeb(0x4, EMMC_SW_RST_R); //Perform a software reset
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

	ALLOC_CACHE_ALIGN_BUFFER(char, crd_tmp_buffer, 0x80);

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
		printf("Tuning_cmd21_ERROR: register 0x98012030=0x%x, 0x98012032= 0x%08x\n", readw(EMMC_NORMAL_INT_STAT_R), readw(EMMC_ERROR_INT_STAT_R));
#endif
		error_handling(cmd_info.cmd->cmdidx, 1);
	}
	return ret_err;
}

int rtkemmc_send_cmd25(void)
{
	int ret_err=0;
	struct rtk_cmd_info cmd_info;
	struct mmc_cmd cmd_val;
	struct mmc_data data;

	ALLOC_CACHE_ALIGN_BUFFER(u8, crd_tmp_buffer, 1024);

	cmd_info.cmd= &cmd_val;
	cmd_info.data= &data;
	cmd_info.cmd->cmdarg = 0xfe;
	cmd_info.cmd->cmdidx = MMC_WRITE_MULTIPLE_BLOCK;
	cmd_info.rsp_len         = 6;
	cmd_info.byte_count  = 0x200;
	cmd_info.block_count = 2;
	cmd_info.dma_buffer = (unsigned char *) crd_tmp_buffer;
	data.blocks = 2;
	data.dest = crd_tmp_buffer;
	cmd_info.xfer_flag       = RTK_MMC_DATA_WRITE; //dma the result to ddr
	MMCPRINTF("*** %s %s %d ***, arg=0x%08x\n", __FILE__, __FUNCTION__, __LINE__,cmd_info.cmd->cmdarg);
	ret_err = rtkemmc_Stream_Cmd(&cmd_info, 1);

	if (ret_err) {
#ifdef MMC_DEBUG
		printf("Tuning_cmd25_ERROR: register 0x98012030=0x%x, 0x98012032= 0x%08x\n", readw(EMMC_NORMAL_INT_STAT_R), readw(EMMC_ERROR_INT_STAT_R));
#endif
		error_handling(cmd_info.cmd->cmdidx, 1);
	}
	return ret_err;
}

unsigned int wait_for_complete(unsigned int cmd, unsigned int bIgnore)
{
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
		case MMC_SET_WRITE_PROT:
		case MMC_CLR_WRITE_PROT:
		case MMC_SEND_WRITE_PROT:
		case MMC_SEND_WRITE_PROT_TYPE:
			XFER_flag=1;
			break;
	}
	if(XFER_flag==1)
		wait_done((UINT32 *)EMMC_NORMAL_INT_STAT_R, 3, 3, cmd); // Check CMD_COMPLETE and XFER_COMPLETE
	else wait_done((UINT32 *)EMMC_NORMAL_INT_STAT_R, 1, 1, cmd); // Check CMD_COMPLETE

	if((readw(EMMC_NORMAL_INT_STAT_R)&0x8000)!=0)
		ret_error = check_error(bIgnore);

	return ret_error;
}

int rtkemmc_SendCMDGetRSP( struct rtk_cmd_info * cmd_info, unsigned int bIgnore)
{
	volatile int ret_error;
	UINT32 cmd_idx = cmd_info->cmd->cmdidx;
	u32 *rsp = (u32 *)&cmd_info->cmd->response;

	rtkemmc_set_rspparam(cmd_info);
	writew(0xffff, EMMC_ERROR_INT_STAT_R);
	writew(0xffff, EMMC_NORMAL_INT_STAT_R);
	writel(cmd_info->cmd->cmdarg, EMMC_ARGUMENT_R);
	CP15ISB;
	sync();

#ifdef MMC_DEBUG
	printf("[%s:%d] cmd_idx = %d, args=0x%x resp_len=%d, 0x98012030=0x%x\n",
		__FILE__, __LINE__, cmd_idx, cmd_info->cmd->cmdarg, cmd_info->rsp_len, readw(EMMC_NORMAL_INT_STAT_R));
#endif
	writew((cmd_idx<<0x8)| cmd_info->cmd_para, EMMC_CMD_R);   // Command register
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
			if((readw(EMMC_ERROR_INT_STAT_R)&
				(EMMC_AUTO_CMD_ERR|EMMC_CMD_IDX_ERR|EMMC_CMD_END_BIT_ERR|EMMC_CMD_CRC_ERR|EMMC_CMD_TOUT_ERR))!=0){ //check cmd line
#ifdef MMC_DEBUG
				printf("CMD Line error occurs \n");
#endif
				writeb(0x2, EMMC_SW_RST_R); //Perform a software reset
				wait_done((UINT32 *)0x9801202c, (0x2<<24), 0x0, cmd_idx); //wait for clear 0x2f bit 1
			}
			if((readw(EMMC_ERROR_INT_STAT_R)&
				(EMMC_ADMA_ERR|EMMC_DATA_END_BIT_ERR|EMMC_DATA_CRC_ERR|EMMC_DATA_TOUT_ERR)) !=0){ //check data line
#ifdef MMC_DEBUG
				printf("DAT Line error occurs \n");
#endif
				writeb(0x4, EMMC_SW_RST_R); //Perform a software reset
				wait_done((UINT32 *)0x9801202c, (0x4<<24), 0x0, cmd_idx); //wait for clear 0x2f bit 2
			}
		}
	}
	return ret_error;
}

int rtkemmc_Stream_Cmd( struct rtk_cmd_info * cmd_info, unsigned int bIgnore)
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
		else if(cmd_idx==MMC_SEND_WRITE_PROT) dma_len=0x4;
		else if(cmd_idx==MMC_SEND_WRITE_PROT_TYPE) dma_len=0x8;
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
	//if (cmd_idx == MMC_SEND_EXT_CSD)
		return 0;
	//else
	//	return block_count;
}

int rtkemmc_Stream( unsigned int cmd_idx,UINT32 blk_addr, UINT32 dma_addr, UINT32 dma_length, int bIgnore)
{
	unsigned int read=1;
	unsigned int  mul_blk=0;
	unsigned int ret_error = 0;

	writel(readl(EMMC_SWC_SEL)|0x10, EMMC_SWC_SEL);
	writel(readl(EMMC_SWC_SEL1)&0xffffffef, EMMC_SWC_SEL1);
	writel(readl(EMMC_SWC_SEL2)|0x10, EMMC_SWC_SEL2);
	writel(readl(EMMC_SWC_SEL3)&0xffffffef, EMMC_SWC_SEL3);
	writel(0, EMMC_CP);

	CP15ISB;
	sync();

	make_ip_des(dma_addr, dma_length);
	CP15ISB;
	sync();

	writew(0xffff, EMMC_ERROR_INT_STAT_R);
	writew(0xffff, EMMC_NORMAL_INT_STAT_R);
	if(cmd_idx==MMC_SEND_TUNING_BLOCK_HS200 ||
	   cmd_idx==MMC_SEND_WRITE_PROT ||
	   cmd_idx==MMC_SEND_WRITE_PROT_TYPE)
		writew(0x1, EMMC_BLOCKCOUNT_R);
	else writew((dma_length/0x200), EMMC_BLOCKCOUNT_R);

	rtkemmc_flush_cache((uintptr_t)rw_descriptor, 2 * sizeof(unsigned int) * MAX_DESCRIPTOR_NUM);
	CP15ISB;
	sync();

	writel((uintptr_t)rw_descriptor, EMMC_ADMA_SA_LOW_R);
	writel(blk_addr, EMMC_ARGUMENT_R);
	CP15ISB;
	sync();

#ifdef MMC_DEBUG
	printf("[%s:%d] cmd_idx = %d, blk_addr = 0x%08x, dma_addr = 0x%08x, dma_length = 0x%x, 0x98012030=0x%x\n",
		 __FILE__, __LINE__, cmd_idx, blk_addr, dma_addr, dma_length, readw(EMMC_NORMAL_INT_STAT_R));
#endif
        if(cmd_idx==MMC_WRITE_BLOCK || cmd_idx==MMC_WRITE_MULTIPLE_BLOCK || cmd_idx==MMC_LOCK_UNLOCK ||cmd_idx==47 || cmd_idx==49)
                read=0;

        if(cmd_idx==MMC_WRITE_MULTIPLE_BLOCK || cmd_idx==MMC_READ_MULTIPLE_BLOCK)
                mul_blk=1;

        writew((mul_blk<<EMMC_MULTI_BLK_SEL)|
                       (read<<EMMC_DATA_XFER_DIR)|
                       (mul_blk<<EMMC_AUTO_CMD_ENABLE)|
                        EMMC_BLOCK_COUNT_ENABLE|
                        EMMC_DMA_ENABLE,
                        EMMC_XFER_MODE_R);

	writew((cmd_idx<<8)|0x3a, EMMC_CMD_R);
	CP15ISB;
	sync();

	rtkemmc_flush_cache(dma_addr, dma_length);
	CP15ISB;
	sync();

	ret_error = wait_for_complete(cmd_idx, bIgnore);

	if(read == 1)
		rtkemmc_invalidate_cache(dma_addr, dma_length);
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

void set_emmc_pin_mux(void)
{
	writel((readl(0x9804E000)&0xff000000)|0x00aaaaaa, 0x9804E000); //pad mux
	sync();
}

int rtkemmc_request(struct udevice *dev, struct mmc_cmd *cmd, struct mmc_data *data)
{
	int ret_err=0;
	volatile struct rtk_cmd_info cmd_info;

	MMCPRINTF("\n*** %s %s %d, cmdidx=0x%02x(%d), flags=0x%08x -------\n", __FILE__, __FUNCTION__, __LINE__, cmd->cmdidx, cmd->cmdidx, cmd->flags);

	switch (cmd->cmdidx) {
	case MMC_CMD_SEND_EXT_CSD: /* = SD_SEND_IF_COND (8) */
		if (data)
			/* ext_csd */
			break;
		else
			/* send_if_cond cmd (not support) */
			return -ETIMEDOUT;
	case MMC_CMD_STOP_TRANSMISSION:
	/* In realtek eMMC IP, hardware will send the stop command
	 * SPI multiblock writes terminate using a special
	 * token, not a STOP_TRANSMISSION request.
	 */
	case MMC_CMD_SET_BLOCKLEN:
		return 0;
	case MMC_CMD_APP_CMD:
		return -ETIMEDOUT;
	default:
		break;
	}

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

int rtkemmc_set_ios (struct udevice *dev)
{
	struct rtk_mmc_plat *plat = dev_get_plat(dev);
	struct mmc *mmc = &plat->mmc;
	MMCPRINTF("*** %s %s %d, bw=%d, clk=%d, tran_speed = %d, mode = %d\n", __FILE__, __FUNCTION__, __LINE__, mmc->bus_width, mmc->clock, mmc->tran_speed, mmc->selected_mode);

	// mmc->bus_width
	if( mmc->bus_width == 1 ) {
		writeb((readb(EMMC_HOST_CTRL1_R)&0xdd)|EMMC_BUS_WIDTH_1, EMMC_HOST_CTRL1_R);
		sync();
	}
	else if( mmc->bus_width == 4 ) {
		writeb((readb(EMMC_HOST_CTRL1_R)&0xdd)|EMMC_BUS_WIDTH_4, EMMC_HOST_CTRL1_R);
		sync();
	}
	else if( mmc->bus_width == 8 ) {
		writeb((readb(EMMC_HOST_CTRL1_R)&0xdf)|EMMC_BUS_WIDTH_8, EMMC_HOST_CTRL1_R);
		sync();
	}
	else {
		UPRINTF("*** %s %s %d, bw=%d, clk=%d\n", __FILE__, __FUNCTION__, __LINE__, mmc->bus_width, mmc->clock);
	}


	if (mmc->selected_mode == MMC_LEGACY) {
		frequency(0xa6, 0x200);
		writew((readw(EMMC_HOST_CTRL2_R)&0xfff8)|MODE_LEGACY, EMMC_HOST_CTRL2_R);
		printf("legacy mode\n");
	} else if (mmc->selected_mode == MMC_HS_200) {
		frequency(0xa6, 0x1);  //200MHZ
                writew((readw( EMMC_HOST_CTRL2_R)&0xfff8)|MODE_HS200, EMMC_HOST_CTRL2_R);
                printf("speed to hs200 (200MHZ)\n");
	} else if (mmc->selected_mode & (MMC_HS|MMC_HS_52)){
		frequency(0xa6, 0x4);
		writew((readw(EMMC_HOST_CTRL2_R)&0xfff8)|MODE_SDR, EMMC_HOST_CTRL2_R);
		printf("sdr50 mode\n");
	}
	return 0;
}

int rtkemmc_init(void)
{
	//Intialize
	writel(readl(EMMC_OTHER1)&0xffffffff7, EMMC_OTHER1);
        writel(readl(EMMC_OTHER1)|0x8, EMMC_OTHER1);
	CP15ISB;
	sync();

	writeb(0x07, EMMC_SW_RST_R);      //9801202f, Software Reset Register
	CP15ISB;
	sync();
	printf("EMMC_SW_RST_R=0x%x\n", readb(EMMC_SW_RST_R));

	writeb(0x0e, EMMC_TOUT_CTRL_R);      //9801202e, Timeout Control register
	CP15ISB;
	sync();

	writew(0x200, EMMC_BLOCKSIZE_R);      //98012004, block size = 512Byte
	CP15ISB;
	sync();

	writew(0x1008 , EMMC_HOST_CTRL2_R);
	CP15ISB;
	sync();

	writew(0xfeff, EMMC_NORMAL_INT_STAT_EN_R);      //98012034, enable all Normal Interrupt Status register
	CP15ISB;
	sync();

	writew(EMMC_ALL_ERR_STAT_EN, EMMC_ERROR_INT_STAT_EN_R); //98012036, enable all error Interrupt Status register
	CP15ISB;
	sync();

	writew(0xfeff, EMMC_NORMAL_INT_SIGNAL_EN_R);     //98012038, enable all Normal SIGNAL Interrupt  register
	CP15ISB;
	sync();

	writew(EMMC_ALL_ERR_SIGNAL_EN, EMMC_ERROR_INT_SIGNAL_EN_R);      //9801203a, enable all error SIGNAL Interrupt register
	CP15ISB;
	sync();

	writeb(0x0d, EMMC_CTRL_R);      //9801202f, choose is card emmc bit
	CP15ISB;
	sync();

	writel(readl(EMMC_OTHER1) | 0x1, EMMC_OTHER1);        //disable L4 gated
	CP15ISB;
	sync();

	writel(readl(EMMC_DUMMY_SYS) | (EMMC_CLK_O_ICG_EN | EMMC_CARD_STOP_ENABLE), EMMC_DUMMY_SYS);        //enable eMMC command clock
	CP15ISB;
	sync();

	writel(readl(EMMC_OTHER1) | (1 << 11), EMMC_OTHER1);    //card stop bit in stark

	writeb((readb(EMMC_HOST_CTRL1_R) & 0xe7) | (EMMC_ADMA2_32 << EMMC_DMA_SEL), EMMC_HOST_CTRL1_R);   //ADMA2 32 bit select
	CP15ISB;
	sync();

	writeb((readb(EMMC_MSHC_CTRL_R) & (~EMMC_CMD_CONFLICT_CHECK)), EMMC_MSHC_CTRL_R);      //disable emmc cmd conflict checkout
	CP15ISB;
	sync();

	writew(readw(EMMC_CLK_CTRL_R) | 0x1, EMMC_CLK_CTRL_R);   //enable internal clock
	CP15ISB;
	sync();

	writel(0x1, 0x9804F000+EMMC_NAND_DMA_SEL);        // #sram_ctrl, 0 for nf, 1 for emmc
	CP15ISB;
	sync();

	return 0;
}

/*******************************************************
 *
 *******************************************************/
int rtkemmc_init_setup(struct udevice *dev)
{
	//struct mmc *pmmc = mmc->priv;
	writel(readl(0x98000454) | 0x1, 0x98000454);
	set_emmc_pin_mux();
	//emmc descriptor must be 8 byte aligned
	rw_descriptor = memalign(8, MAX_DESCRIPTOR_NUM * sizeof(unsigned int) * 4);
	sys_rsp17_addr = S_RSP17_ADDR;   //set rsp buffer addr
	sys_ext_csd_addr = (unsigned char *)S_EXT_CSD_ADDR;
	ptr_ext_csd = (UINT8*)sys_ext_csd_addr;
	emmc_card.rca = 1<<16;
	emmc_card.sector_addressing = 0;

	writel(0x03, SYS_PLL_EMMC1);
	CP15ISB;
	sync();

	rtkemmc_init();

	udelay(10000);

	writeb((readb(EMMC_HOST_CTRL1_R) & 0xdd) | EMMC_BUS_WIDTH_1, EMMC_HOST_CTRL1_R);
        CP15ISB;
        sync();

	rtkemmc_set_pad_driving(0x0, 0x0,0x0,0x0);

	phase(0, 0); //VP0, VP1 phase

	writel(readl(EMMC_OTHER1) & 0xffffffff7, EMMC_OTHER1);
	writel(readl(EMMC_OTHER1) | 0x8, EMMC_OTHER1);

	frequency(0xa6, 0x200); //divider = 2 * 128 = 256

	sync();

	return 0;
}

/*******************************************************
 *
 *******************************************************/
void rtkemmc_set_pad_driving(unsigned int clk_drv, unsigned int cmd_drv, unsigned int data_drv, unsigned int ds_drv)
{
	writel((readl(EMMC_ISO_pfunc4)&0xfff81fff)|(clk_drv<<13)|(clk_drv<<16), EMMC_ISO_pfunc4);
	writel((readl(EMMC_ISO_pfunc5)&0xfffff03f)|(cmd_drv<<6)|(cmd_drv<<9), EMMC_ISO_pfunc5);
	writel((readl(EMMC_ISO_pfunc6)&0xfff03fff)|(data_drv<<14)|(data_drv<<17), EMMC_ISO_pfunc6);
	writel((readl(EMMC_ISO_pfunc7)&0xfe07f03f)|(data_drv<<6)|(data_drv<<9)|(data_drv<<19)|(data_drv<<22), EMMC_ISO_pfunc7);
	writel((readl(EMMC_ISO_pfunc8)&0xfff81fc0)|(data_drv<<0)|(data_drv<<3)|(data_drv<<13)|(data_drv<<16), EMMC_ISO_pfunc8);
	writel((readl(EMMC_ISO_pfunc9)&0xfe07f03f)|(data_drv<<6)|(data_drv<<9)|(clk_drv<<19)|(clk_drv<<22), EMMC_ISO_pfunc9);
	writel((readl(EMMC_ISO_pfunc10)&0xffffffc0)|(clk_drv<<0)|(clk_drv<<3), EMMC_ISO_pfunc10);

	CP15ISB;
	sync();
}

static void
emmc_info_show(void)
{
	MMCPRINTF("%s(%u)\n",__func__,__LINE__);
	MMCPRINTF("EMMC SYS_PLL_EMMC1=0x%08x SYS_PLL_EMMC2=0x%08x \nSYS_PLL_EMMC3=0x%08x SYS_PLL_EMMC4=0x%08x HOST_CONTROL2_REG=0x%08x\n \
		PRESENT_STATE_REG=0x%08x  HOST CONTROL1 REG=0x%08x TRANSFER_MODE_REG=0x%08x \n EMMC_CKGEN_CTL=0x%08x EMMC_DQS_CTRL1=0x%08x \n",
	readl(SYS_PLL_EMMC1), readl(SYS_PLL_EMMC2), readl(SYS_PLL_EMMC3), readl(SYS_PLL_EMMC4), readw(EMMC_HOST_CTRL2_R),
	readl(EMMC_PSTATE_REG), readb(EMMC_HOST_CTRL1_R), readw(EMMC_XFER_MODE_R), readl(EMMC_CKGEN_CTL), readl(EMMC_DQS_CTRL1));
}

void rtkemmc_host_reset(void)
{
	writeb(0x07, EMMC_SW_RST_R);      //9801202f, Software Reset Register
	sync();

	writeb(0x0e, EMMC_TOUT_CTRL_R);      //9801202e, Timeout Control register
	sync();

	writew(0x200, EMMC_BLOCKSIZE_R);      //98012004, block size = 512Byte
	sync();

	writew(0xfeff, EMMC_NORMAL_INT_STAT_EN_R);    //98012034, enable all Normal Interrupt Status register
	sync();

	writew(EMMC_ALL_ERR_STAT_EN, EMMC_ERROR_INT_STAT_EN_R); //98012036, enable all error Interrupt Status register
	sync();

	writew(0xfeff, EMMC_NORMAL_INT_SIGNAL_EN_R); //98012038, enable all Normal SIGNAL Interrupt  register
	sync();

	writew(EMMC_ALL_ERR_SIGNAL_EN, EMMC_ERROR_INT_SIGNAL_EN_R);      //9801203a, enable all error SIGNAL Interrupt register
	sync();
}

void phase(u32 VP0, u32 VP1){
	u32 t1=10; //us, after phrt0 = 0
	u32 t2=3; //us, after phse setup

	//phase selection
	if( (VP0!=0xff) || (VP1!=0xff)){
		writel(readl(SYS_PLL_EMMC1)&0xfffffffd, SYS_PLL_EMMC1);
		sync();
		udelay(t1);

		writel(readl(EMMC_OTHER1)|((1<<10)), EMMC_OTHER1);
		if(VP0!=0xff) {
			//vp0 phase:0x0~0x1f
			writel((readl(SYS_PLL_EMMC1)&0xffffff07)|(VP0<<3), SYS_PLL_EMMC1);
		}
		if(VP1!=0xff) {
			writel((readl(SYS_PLL_EMMC1)&0xffffe0ff) | ((VP1<<8)), SYS_PLL_EMMC1);
		}
		sync();
		udelay(t2);

		writel(readl(EMMC_OTHER1)&(~(1<<10)), EMMC_OTHER1);
                //release reset pll
                writel(readl(SYS_PLL_EMMC1)|0x2, SYS_PLL_EMMC1);
                sync();

                wait_done((UINT32 *)0x9801255c, 0x1, 0x1, 1000); //wait for clear 0x255c bit 0
                wait_done((UINT32 *)0x9801202c, 3, 3, 1000);            //wait for clock is active
                writeb(0x06, EMMC_SW_RST_R);      //9801202f, Software Reset Register
                sync();

                wait_done((UINT32 *)0x9801202c, 0x6<<24, 0x0, 1000);
	}
}

#ifdef MMC_SUPPORTS_TUNING
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

int rtkemmc_execute_tuning(struct udevice *dev, u8 opcode){
//int mmc_Tuning_HS200(void) {
	volatile UINT32 TX_window=0xffffffff;
	volatile UINT32 TX1_window=0xffffffff;
	volatile int TX_best=0xff;
	volatile UINT32 RX_window=0xffffffff;
	volatile int RX_best=0xff;
	volatile int i=0;
	int loop_cnt = 0;
	int j;

	rtkemmc_set_pad_driving(0x2, 0x2, 0x2, 0x2);
	phase(0, 0);	//VP0, VP1 phas

	//mdelay(5);
	emmc_info_show();
	sync();

	/*Actually, in Stark, the clock source is from crc, command tx does not need to be tuned, but this action is for safety.
         We ecpected that tx phase is 0xffffffff(hs200) or 0xffff(hs400) if the clock source is from crc
         if user encounter a bad IC, they can adjust the clock source from 0x98012420 [14:15] 2b' 00 to 2b' 10,
         Again, this tx tuning is needed*/
        if((readw(EMMC_OTHER1)&(0x3<<14))!=0) {   //2b' 10 tx case, not crc clk source
		for(i=0x0; i<0x20; i++){
			phase(i, 0xff);

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
	}
	else {
		TX_window = 0xffffffff;
	}

rx_retry:
	for(i=0x0; i<0x20; i++){
		phase(0xff, i);
		if(rtkemmc_send_cmd21()!=0)
		{
			RX_window= RX_window&(~(1<<i));
		} else {
			RX_window= RX_window|((1<<i));
		}

	}
	RX_best = search_best(RX_window,0x20);
	printf("RX_WINDOW=0x%x, RX_best=0x%x \n",RX_window, RX_best);
	if (RX_window == 0x0)
	{
		printf("[LY] HS200 RX tuning fail \n");
		return -2;
	}
	if(RX_window==0xffffffff) {
                loop_cnt++;
                switch(loop_cnt) {
                        case 10:
                                rtkemmc_set_pad_driving(1, 1, 1, 0);
                                printf("try pad driving 1: RX_WINDOW = 0x%08x RX_best=0x%x\n", RX_window,RX_best);
                                break;
                        case 20:
                                rtkemmc_set_pad_driving(0, 0, 0, 0);
                                printf("try pad driving 0: RX_WINDOW = 0x%08x RX_best=0x%x\n", RX_window,RX_best);
                                break;
                        default:
                                if(loop_cnt>20)
                                        printf("loop cnt %d: RX_WINDOW = 0x%08x, cannot find a proper rx phase\n", loop_cnt, RX_window);
                }

                if(loop_cnt<=20)
                        goto rx_retry;
        }
	else {
		rtkemmc_set_pad_driving(2, 2, 2, 0);
	}

	phase( 0xff, RX_best);

	TX1_window= TX_window;
	printf("Start HS200 TX Tuning2:\n");

	loop_cnt=0;
tx1_retry:
	for(i=0x0; i<0x20; i++){
                j=(TX_window>>i)&1;
                if(j == 1){
                        phase(i, 0xff);
                        if(rtkemmc_send_cmd25() != 0)
                        {
                                TX1_window= TX1_window&(~(1<<i));
                        }
                }
        }
        TX_best = search_best(TX1_window,0x20);
	printf("TX1_WINDOW=0x%x, TX_best=0x%x \n",TX1_window, TX_best);
	if(TX1_window==0xffffffff) {
                loop_cnt++;
                switch(loop_cnt) {
                        case 10:
                                rtkemmc_set_pad_driving(1, 1, 1, 0);
                                printf("try pad driving 1: TX1_WINDOW = 0x%08x TX_best=0x%x\n", TX1_window, TX_best);
                                break;
                        case 20:
                                rtkemmc_set_pad_driving(0, 0, 0, 0);
                                printf("try pad driving 0: TX1_WINDOW = 0x%08x TX_best=0x%x\n", TX1_window, TX_best);
                                break;
                        default:
                                if(loop_cnt>20)
                                        printf("loop cnt %d: TX1_WINDOW = 0x%08x, cannot find a proper tx1 phase\n", loop_cnt, TX1_window);
                }

                if(loop_cnt<=20)
                        goto tx1_retry;
        }
        else {
                rtkemmc_set_pad_driving(2, 2, 2, 0);
        }
	phase(TX_best, 0xff);
	writel(readl(EMMC_OTHER1)&0xfffffffe, EMMC_OTHER1);        //enable L4 gated after HS200 finished

	return 0;
}
#endif

int mmc_Select_SDR50_Push_Sample(void){
	frequency(0xa6, 0x4);
	rtkemmc_set_pad_driving(0x0, 0x0, 0x0, 0x0);
	writel(readl(EMMC_OTHER1)&0xfffffffe, EMMC_OTHER1);        //enable L4 gated after SDR50 finished
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

/*******************************************************
 *
 *******************************************************/
int rtk_eMMC_write(unsigned int blk_addr, unsigned int data_size, unsigned int *buffer)
{
	int ret_err = 0;
	unsigned int total_blk_cont;
	unsigned int curr_xfer_blk_cont;
	unsigned int address;
	unsigned int curr_blk_addr;
	e_device_type * card = &emmc_card;
	UINT8 ret_state = 0;
	UINT32 bRetry = 0, resp = 0;
	UINT32 retry_cnt = 0, retry_cnt1 = 0, retry_cnt2 = 0, retry_cnt3 = 0;
	int err1=0;

	printf("\nemmc:%s blk_addr=0x%08x, data_size=0x%08x, buffer=0x%08x, addressing=%d\n",
			__FUNCTION__, blk_addr, data_size, (unsigned int)(uintptr_t)buffer, card->sector_addressing);

	total_blk_cont = data_size >> 9;
	if (data_size & 0x1ff)
		total_blk_cont+=1;

	curr_blk_addr = blk_addr;
#ifndef FORCE_SECTOR_MODE
	if (card->sector_addressing)
		address = curr_blk_addr;
	else
		address = curr_blk_addr << 9;
#else
	address = curr_blk_addr;
#endif

#ifdef EMMC_SPLIT_BLK
	while (total_blk_cont) {
		if (total_blk_cont > EMMC_MAX_XFER_BLKCNT)
			curr_xfer_blk_cont = EMMC_MAX_XFER_BLKCNT;
		else
			curr_xfer_blk_cont = total_blk_cont;

		if (IS_SRAM_ADDR((uintptr_t)buffer))
			ret_err = -1; //cannot allow r/w to sram in bootcode
		else {
			if (curr_xfer_blk_cont == 1)
				ret_err = rtkemmc_Stream(MMC_WRITE_BLOCK, address, (uintptr_t)buffer, curr_xfer_blk_cont << 9,0);
			else
				ret_err = rtkemmc_Stream(MMC_WRITE_MULTIPLE_BLOCK, address, (uintptr_t)buffer, curr_xfer_blk_cont << 9,0);
		}

		if (ret_err)
			return 0;

		total_blk_cont -= curr_xfer_blk_cont;
		buffer += (curr_xfer_blk_cont<<(9-2));
		address += curr_xfer_blk_cont;
	}

	total_blk_cont = data_size >> 9;

	if (data_size & 0x1ff)
		total_blk_cont += 1;

	return total_blk_cont;
#else
RETRY_RD_CMD:
	if (IS_SRAM_ADDR((uintptr_t)buffer))
		ret_err = -1; //cannot allow r/w to sram in bootcode
	else {
		if (curr_xfer_blk_cont == 1)
			ret_err = rtkemmc_Stream(MMC_WRITE_BLOCK, address, buffer, total_blk_cont << 9,0);
		else
			ret_err = rtkemmc_Stream(MMC_WRITE_MULTIPLE_BLOCK, address, buffer, total_blk_cont << 9,0);
	}

	if (ret_err) {
		if (retry_cnt2++ < MAX_CMD_RETRY_COUNT * 2) {
			sample_ctl_switch(-1);
			goto RETRY_RD_CMD;
		}
		return ret_err;
	}

	/* To wait status change complete */
	bRetry=0;
	retry_cnt=0;
	retry_cnt1=0;
	while (1) {
		err1 = rtkemmc_send_status(&resp,1,0);
		//1. if cmd sent error, try again
		if (err1) {
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
		if (ret_state != STATE_TRAN) {
#ifdef THIS_IS_FLASH_WRITE_U_ENV
			prints("retry - case 2\n");
#else
			MMCPRINTF("retry - case 2\n");
#endif
			bRetry = 1;

			if (retry_cnt1++ > MAX_CMD_RETRY_COUNT * 2)
				return ERR_EMMC_SEND_RW_CMD_RETRY_FAIL;

			rtkemmc_stop_transmission(&emmc_card,0);
			mdelay(1000);
		} else {
			//out peaceful
			if (bRetry == 0) {
#ifdef THIS_IS_FLASH_WRITE_U_ENV
				prints("retry - case 3\n");
#else
				MMCPRINTF("retry - case 3\n");
#endif
				return !ret_err ?  total_blk_cont : 0;
			} else {
#ifdef THIS_IS_FLASH_WRITE_U_ENV
				prints("retry - case 4\n");
#else
				MMCPRINTF("retry - case 4\n");
#endif
				retry_cnt2 = 0;

				if (retry_cnt3++ > MAX_CMD_RETRY_COUNT * 2)
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

void rtk_mmc_config_setup(const char *name, struct mmc_config *cfg)
{
	if(!cfg)
		return;

	cfg->name = name;
#ifdef CONFIG_MMC_MODE_4BIT
	cfg->host_caps = (MMC_MODE_4BIT | MMC_MODE_HS_52MHz | MMC_MODE_HS | MMC_MODE_HC);
#else
	cfg->host_caps = (MMC_MODE_4BIT | MMC_MODE_8BIT | MMC_MODE_HS200 | MMC_MODE_HSDDR_52MHz | MMC_MODE_HS_52MHz | MMC_MODE_HS | MMC_MODE_HC);
#endif
	cfg->voltages = MMC_VDD_32_33 | MMC_VDD_33_34 | MMC_VDD_165_195;
	cfg->f_min = 400000;
	cfg->f_max = 50000000;
	cfg->b_max = 256; // max transfer block size
	cfg->part_type = PART_TYPE_UNKNOWN;
}

#if CONFIG_IS_ENABLED(DM_MMC)
static const struct dm_mmc_ops rtk_mmc_ops = {
	.send_cmd       = rtkemmc_request,
	.set_ios        = rtkemmc_set_ios,
#ifdef MMC_SUPPORTS_TUNING
	.execute_tuning = rtkemmc_execute_tuning,
#endif
};

static int rtk_mmc_probe(struct udevice *dev)
{
	struct mmc_uclass_priv *upriv = dev_get_uclass_priv(dev);
	struct rtk_mmc_plat *plat = dev_get_plat(dev);
	struct mmc_config *cfg = &plat->cfg;

	rtk_mmc_config_setup(dev->name, cfg);
	upriv->mmc = &plat->mmc;

	return rtkemmc_init_setup(dev);
}

static int rtk_mmc_bind(struct udevice *dev)
{
        struct rtk_mmc_plat *plat = dev_get_plat(dev);

        return mmc_bind(dev, &plat->mmc, &plat->cfg);
}

static const struct udevice_id rtk_mmc_ids[] = {
        { .compatible = "rtd161xb-dw-cqe-emmc" },
        { }
};

U_BOOT_DRIVER(rtk_mmc_drv) = {
        .name           = "RTD161xb eMMC",
        .id             = UCLASS_MMC,
        .of_match       = rtk_mmc_ids,
	.bind           = rtk_mmc_bind,
        .probe          = rtk_mmc_probe,
        .ops            = &rtk_mmc_ops,
	.plat_auto = sizeof(struct rtk_mmc_plat),
};
#endif
