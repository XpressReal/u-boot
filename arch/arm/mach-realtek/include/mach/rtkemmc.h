/*
 *  Copyright (C) 2013 Realtek Semiconductors, All Rights Reserved.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */
#ifndef __RTKEMMC_H__
#define __RTKEMMC_H__

#include <asm/cache.h>
#include <asm/arch/rbus/sb2_reg.h>

#define UINT8   unsigned char
#define UINT16  unsigned short
#define UINT32  unsigned int
#define UINT64  unsigned long
#define INT8    char
#define INT16   short
#define INT32   int
#define INT64   long

#define REG8( addr )              (*(volatile UINT8 *) (addr))
#define REG16( addr )             (*(volatile UINT16 *)(addr))
#define REG32( addr )             (*(volatile UINT32 *)(addr))
#define REG64( addr )             (*(volatile UINT64 *)(addr))

#define __align16__ __attribute__((aligned (16)))

#define S_EXT_CSD_ADDR        0xa00000
#define EXT_CSD_BUFFER_SIZE   0x200
#define S_RSP17_ADDR          (S_EXT_CSD_ADDR+EXT_CSD_BUFFER_SIZE)
#define EMMC_BLK_SIZE         0x200
#define MAX_CMD_RETRY_COUNT    4

#define EMMC_DRVNAME "emmc:"
#ifdef THIS_IS_FLASH_WRITE_U_ENV
        #include "sysdefs.h"
        #include "dvrboot_inc/util.h"
        #define UPRINTF(fmt, args...)                   \
                                if( rtprintf ) {                        \
                                        rtprintf(fmt,## args);  \
                                }
        #define EXT_CSD_CMD_SET_NORMAL          (1 << 0)
        #define EXT_CSD_CMD_SET_SECURE          (1 << 1)
        #define EXT_CSD_CMD_SET_CPSECURE        (1 << 2)
        #define EXT_CSD_CARD_TYPE_26            (1 << 0)
        #define EXT_CSD_CARD_TYPE_52            (1 << 1)
        #define EXT_CSD_BUS_WIDTH_1                     0
        #define EXT_CSD_BUS_WIDTH_4                     1
        #define EXT_CSD_BUS_WIDTH_8                     2
#else
        #include <common.h>
        #include <command.h>
        #include <mmc.h>
        #include <part.h>
        #include <malloc.h>
        #include <linux/errno.h>
        #include <asm/io.h>
        #define UPRINTF(fmt, args...)                   \
                                printf(fmt,## args)
#endif

//hank emmc ip register

#define EMMC_SDMASA_R			     (0x98012000)
#define EMMC_BLOCKSIZE_R		     (0x98012004)
#define EMMC_BLOCKCOUNT_R    		     (0x98012006)
#define EMMC_ARGUMENT_R      		     (0x98012008)
#define EMMC_XFER_MODE_R     		     (0x9801200c)
#define EMMC_CMD_R           		     (0x9801200e)
#define EMMC_RESP01_R        		     (0x98012010)
#define EMMC_RESP23_R        		     (0x98012014)
#define EMMC_RESP45_R        		     (0x98012018)
#define EMMC_RESP67_R        		     (0x9801201c)
#define EMMC_BUF_DATA_R			     (0x98012020)
#define EMMC_PSTATE_REG      		     (0x98012024)
#define EMMC_HOST_CTRL1_R		     (0x98012028)
#define EMMC_PWR_CTRL_R    		     (0x98012029)
#define EMMC_BGAP_CTRL_R                     (0x9801202a)
#define EMMC_CLK_CTRL_R                      (0x9801202c)
#define EMMC_TOUT_CTRL_R    		     (0x9801202e)
#define EMMC_SW_RST_R     		     (0x9801202f)
#define EMMC_NORMAL_INT_STAT_R		     (0x98012030)
#define EMMC_ERROR_INT_STAT_R		     (0x98012032)
#define EMMC_NORMAL_INT_STAT_EN_R    	     (0x98012034)
#define EMMC_ERROR_INT_STAT_EN_R      	     (0x98012036)
#define EMMC_NORMAL_INT_SIGNAL_EN_R  	     (0x98012038)
#define EMMC_ERROR_INT_SIGNAL_EN_R           (0x9801203a)
#define EMMC_AUTO_CMD_STAT_R           	     (0x9801203c)
#define EMMC_HOST_CTRL2_R                    (0x9801203e)
#define EMMC_ADMA_ERR_STAT_R		     (0x98012054)
#define EMMC_ADMA_SA_LOW_R                   (0x98012058)
#define EMMC_AT_CTRL_R                       (0x98012240)

#define EMMC_MSHC_CTRL_R		     (0x98012208)
#define EMMC_CMD_CONFLICT_CHECK		     (1<<0)
#define EMMC_CTRL_R                          (0x9801222c)

//hank emmc wrapper register
#define EMMC_CP                       	(0x9801241c)
#define EMMC_OTHER1                     (0x98012420)
#define EMMC_ISR                        (0x98012424)  //unused in hank
#define EMMC_ISREN                      (0x98012428)	//unused in hank
#define EMMC_DUMMY_SYS                  (0x9801242c)
#define EMMC_AHB                  	(0x98012430)
#define EMMC_DBG                        (0x98012444)
#define EMMC_PP_BIST_CTL                (0x98012460)
#define EMMC_IP_BIST_CTL                (0x98012464)
#define EMMC_PP_BIST_STS                (0x98012468)
#define EMMC_IP_BIST_STS                (0x9801246c)
#define EMMC_CKGEN_CTL                	(0x98012478)
#define EMMC_CARD_SIG                   (0x98012484)
#define EMMC_DQS_CTRL1                  (0x98012498)
#define EMMC_DQS_CTRL2                  (0x9801249c)
#define EMMC_IP_DESC0                   (0x980124a0)
#define EMMC_IP_DESC1                   (0x980124a4)
#define EMMC_IP_DESC2                   (0x980124a8)
#define EMMC_IP_DESC3                   (0x980124ac)
#define EMMC_PROTECT0                   (0x980124c0)
#define EMMC_PROTECT1                   (0x980124c4)
#define EMMC_PROTECT2                   (0x980124c8)
#define EMMC_PROTECT3                   (0x980124cc)
#define EMMC_SWC_SEL_CHK                (0x980124e4)
#define EMMC_DUMMY_SYS1                 (0x98012500)
#define EMMC_CLK_DET_PLLEMMC            (0x98012504)
#define EMMC_DQ_CTRL_SET                (0x9801250c)
#define EMMC_WDQ_CTRL0                  (0x98012510)
#define EMMC_WDQ_CTRL1                  (0x98012514)
#define EMMC_WDQ_CTRL2                  (0x98012518)
#define EMMC_WDQ_CTRL3                  (0x9801251c)
#define EMMC_WDQ_CTRL4                  (0x98012520)
#define EMMC_WDQ_CTRL5                  (0x98012524)
#define EMMC_WDQ_CTRL6                  (0x98012528)
#define EMMC_WDQ_CTRL7                  (0x9801252c)
#define EMMC_RDQ_CTRL0                  (0x98012530)
#define EMMC_RDQ_CTRL1                  (0x98012534)
#define EMMC_RDQ_CTRL2                  (0x98012538)
#define EMMC_RDQ_CTRL3                  (0x9801253c)
#define EMMC_RDQ_CTRL4                  (0x98012540)
#define EMMC_RDQ_CTRL5                  (0x98012544)
#define EMMC_RDQ_CTRL6                  (0x98012548)
#define EMMC_RDQ_CTRL7                  (0x9801254c)
#define EMMC_CMD_CTRL_SET		(0x98012550)
#define EMMC_WCMD_CTRL                  (0x98012554)
#define EMMC_RCMD_CTRL                  (0x98012558)
#define EMMC_PLL_STATUS                 (0x9801255c)

#define EMMC_PON_DES0                   (0x98012800)
#define EMMC_PON_DES1                   (0x98012804)
#define EMMC_PON_DES2                   (0x98012808)
#define EMMC_PON_CTRL                   (0x9801280c)
#define EMMC_PON_ID                     (0x98012810)
#define EMMC_PON_ADDR                   (0x98012814)
#define EMMC_PON_ST                     (0x98012818)
#define EMMC_PON_SAVE                   (0x9801281c)
#define EMMC_PON_DBUS_SLV               (0x98012820)
#define EMMC_PON_DBG_CTRL               (0x98012824)
#define EMMC_PON_DBUS_SLV_DBG           (0x98012828)
#define EMMC_PON_MEM                    (0x9801282c)
#define EMMC_PON_DBG0                   (0x98012830)
#define EMMC_PON_DBG1                   (0x98012834)
#define EMMC_PON_DBG2                   (0x98012838)
#define EMMC_PON_DBG3                   (0x9801283c)
#define EMMC_PON_DBG_CTRL1              (0x98012840)

#define EMMC_HD_SEM                     (0x98012900)

//0x98012030 status bitmap
#define EMMC_ERR_INTERRUPT		1<<15
#define EMMC_CQE_EVENT			1<<14
#define EMMC_FX_EVENT			1<<13
#define EMMC_RE_TUNE_EVENT		1<<12
#define EMMC_INT_C			1<<11
#define EMMC_INT_B                      1<<10
#define EMMC_INT_A                      1<<9
#define EMMC_CARD_INTERRUPT		1<<8
#define EMMC_CARD_REMOVAL		1<<7
#define EMMC_CARD_INSERTION             1<<6
#define EMMC_BUF_RD_READY		1<<5
#define EMMC_BUF_WR_READY               1<<4
#define EMMC_DMA_INTERRPT		1<<3
#define EMMC_BGAP_EVENT			1<<2
#define EMMC_XFER_COMPLETE		1<<1
#define EMMC_CMD_COMPLETE		1

//0x98012032 error bitmap
#define EMMC_VENDOR_ERR3		1<<15
#define EMMC_VENDOR_ERR2                1<<14
#define EMMC_VENDOR_ERR1                1<<13
#define EMMC_BOOT_ACK_ERR               1<<12
#define EMMC_RESP_ERR			1<<11
#define EMMC_TUNING_ERR			1<<10
#define EMMC_ADMA_ERR			1<<9
#define EMMC_AUTO_CMD_ERR		1<<8
#define EMMC_CUR_LMT_ERR		1<<7
#define EMMC_DATA_END_BIT_ERR		1<<6
#define EMMC_DATA_CRC_ERR		1<<5
#define EMMC_DATA_TOUT_ERR		1<<4
#define EMMC_CMD_IDX_ERR		1<<3
#define EMMC_CMD_END_BIT_ERR		1<<2
#define EMMC_CMD_CRC_ERR		1<<1
#define EMMC_CMD_TOUT_ERR		1

//0x98012034 status enable bitmap
#define EMMC_CQE_EVENT_STAT_EN                  1<<14
#define EMMC_FX_EVENT_STAT_EN                   1<<13
#define EMMC_RE_TUNE_EVENT_STAT_EN              1<<12
#define EMMC_INT_C_STAT_EN                      1<<11
#define EMMC_INT_B_STAT_EN                      1<<10
#define EMMC_INT_A_STAT_EN                      1<<9
#define EMMC_CARD_INTERRUPT_STAT_EN             1<<8
#define EMMC_CARD_REMOVAL_STAT_EN               1<<7
#define EMMC_CARD_INSERTION_STAT_EN             1<<6
#define EMMC_BUF_RD_READY_STAT_EN               1<<5
#define EMMC_BUF_WR_READY_STAT_EN               1<<4
#define EMMC_DMA_INTERRPT_STAT_EN               1<<3
#define EMMC_BGAP_EVENT_STAT_EN                 1<<2
#define EMMC_XFER_COMPLETE_STAT_EN              1<<1
#define EMMC_CMD_COMPLETE_STAT_EN               1

//0x98012036 error status enable bitmap
#define EMMC_VENDOR_ERR_STAT_EN3                1<<15
#define EMMC_VENDOR_ERR_STAT_EN2                1<<14
#define EMMC_VENDOR_ERR_STAT_EN1                1<<13
#define EMMC_BOOT_ACK_ERR_STAT_EN               1<<12
#define EMMC_RESP_ERR_STAT_EN                   1<<11
#define EMMC_TUNING_ERR_STAT_EN                 1<<10
#define EMMC_ADMA_ERR_STAT_EN                   1<<9
#define EMMC_AUTO_CMD_ERR_STAT_EN               1<<8
#define EMMC_CUR_LMT_ERR_STAT_EN                1<<7
#define EMMC_DATA_END_BIT_ERR_STAT_EN           1<<6
#define EMMC_DATA_CRC_ERR_STAT_EN               1<<5
#define EMMC_DATA_TOUT_ERR_STAT_EN              1<<4
#define EMMC_CMD_IDX_ERR_STAT_EN                1<<3
#define EMMC_CMD_END_BIT_ERR_STAT_EN            1<<2
#define EMMC_CMD_CRC_ERR_STAT_EN                1<<1
#define EMMC_CMD_TOUT_ERR_STAT_EN               1

//0x98012038 signal interrupt enable
#define EMMC_CQE_EVENT_SIGNAL_EN                  1<<14
#define EMMC_FX_EVENT_SIGNAL_EN                   1<<13
#define EMMC_RE_TUNE_EVENT_SIGNAL_EN              1<<12
#define EMMC_INT_C_SIGNAL_EN                      1<<11
#define EMMC_INT_B_SIGNAL_EN                      1<<10
#define EMMC_INT_A_SIGNAL_EN                      1<<9
#define EMMC_CARD_INTERRUPT_SIGNAL_EN             1<<8
#define EMMC_CARD_REMOVAL_SIGNAL_EN               1<<7
#define EMMC_CARD_INSERTION_SIGNAL_EN             1<<6
#define EMMC_BUF_RD_READY_SIGNAL_EN               1<<5
#define EMMC_BUF_WR_READY_SIGNAL_EN               1<<4
#define EMMC_DMA_INTERRPT_SIGNAL_EN               1<<3
#define EMMC_BGAP_EVENT_SIGNAL_EN                 1<<2
#define EMMC_XFER_COMPLETE_SIGNAL_EN              1<<1
#define EMMC_CMD_COMPLETE_SIGNAL_EN               1

//0x9801203a error ssignal enable bitmap
#define EMMC_VENDOR_ERR_SIGNAL_EN3                1<<15
#define EMMC_VENDOR_ERR_SIGNAL_EN2                1<<14
#define EMMC_VENDOR_ERR_SIGNAL_EN1                1<<13
#define EMMC_BOOT_ACK_ERR_SIGNAL_EN               1<<12
#define EMMC_RESP_ERR_SIGNAL_EN                   1<<11
#define EMMC_TUNING_ERR_SIGNAL_EN                 1<<10
#define EMMC_ADMA_ERR_SIGNAL_EN                   1<<9
#define EMMC_AUTO_CMD_ERR_SIGNAL_EN               1<<8
#define EMMC_CUR_LMT_ERR_SIGNAL_EN                1<<7
#define EMMC_DATA_END_BIT_ERR_SIGNAL_EN           1<<6
#define EMMC_DATA_CRC_ERR_SIGNAL_EN               1<<5
#define EMMC_DATA_TOUT_ERR_SIGNAL_EN              1<<4
#define EMMC_CMD_IDX_ERR_SIGNAL_EN                1<<3
#define EMMC_CMD_END_BIT_ERR_SIGNAL_EN            1<<2
#define EMMC_CMD_CRC_ERR_SIGNAL_EN                1<<1
#define EMMC_CMD_TOUT_ERR_STAT_EN                 1

#define EMMC_ALL_NORMAL_STAT_EN			  (0xfeff)
#define EMMC_ALL_ERR_STAT_EN			  (0xffff)	//enablle all error interrupt in 0x98012036
#define EMMC_ALL_SIGNAL_STAT_EN                   (0xfeff)
#define EMMC_ALL_ERR_SIGNAL_EN			  (0xffff)	//enable all singal error interrupt in 0x9801203a


#define CMD_IDX_MASK(x)         ((x >> 8)&0x3f)

//0x9801200e
#define EMMC_RESP_TYPE_SELECT	0
#define EMMC_CMD_TYPE		6
#define EMMC_NO_RESP		0x0
#define EMMC_RESP_LEN_136	0x1
#define EMMC_RESP_LEN_48	0x2
#define EMMC_RESP_LEN_48B	0x3
#define EMMC_CMD_CHK_RESP_CRC        	(1<<3)
#define EMMC_CMD_IDX_CHK_ENABLE		(1<<4)
#define EMMC_DATA			(1<<5)
#define EMMC_ABORT_CMD		0x3
#define EMMC_RESUME_CMD		0x2
#define EMMC_SUSPEND_CMD	0x1
#define EMMC_NORMAL_CMD		0x0

//0x98012028
#define EMMC_DMA_SEL		3
#define EMMC_SDMA		0x0
#define EMMC_ADMA2_32		0x2
#define EMMC_ADMA2_64		0x3
#define EMMC_EXT_DAT_XFER		(1<<5)
#define EMMC_HIGH_SPEED_EN		(1<<2)
#define EMMC_DAT_XFER_WIDTH		(1<<1)
#define EMMC_BUS_WIDTH_8		EMMC_EXT_DAT_XFER
#define EMMC_BUS_WIDTH_4		EMMC_DAT_XFER_WIDTH
#define EMMC_BUS_WIDTH_1		0<<1

//0x9801200c
#define EMMC_MULTI_BLK_SEL		5
#define EMMC_DATA_XFER_DIR		4
#define EMMC_BLOCK_COUNT_ENABLE		(1<<1)
#define EMMC_DMA_ENABLE			(1<<0)
#define EMMC_AUTO_CMD_ENABLE		2
#define EMMC_AUTO_CMD_DISABLED		0x0
#define EMMC_AUTO_CMD12_ENABLED		0x1
#define EMMC_AUTO_CMD23_ENABLED		0x2
#define EMMC_AUTO_CMD_SEL		0x3

//0x9801203e
#define MODE_LEGACY	0x0
#define MODE_SDR	0x1
#define MODE_HS200	0x3
#define MODE_DDR50	0x4
#define MODE_HS400	0x7

#define EMMC_MASK_CMD_CODE        (0xFF)
#define EMMC_NORMALWRITE          (0x00)
#define EMMC_AUTOWRITE3           (0x01)
#define EMMC_AUTOWRITE4           (0x02)
#define EMMC_AUTOREAD3            (0x05)
#define EMMC_AUTOREAD4            (0x06)
#define EMMC_SENDCMDGETRSP        (0x08)
#define EMMC_AUTOWRITE1           (0x09)
#define EMMC_AUTOWRITE2           (0x0A)
#define EMMC_NORMALREAD           (0x0C)
#define EMMC_AUTOREAD1            (0x0D)
#define EMMC_AUTOREAD2            (0x0E)
#define EMMC_TUNING               (0x0F)
#define EMMC_CMD_UNKNOW           (0x10)

#define EMMC_SWC_SEL                    (0x98007e00)
#define EMMC_SWC_SEL1                   (0x98007e04)
#define EMMC_SWC_SEL2                   (0x98007e08)
#define EMMC_SWC_SEL3                   (0x98007e0c)

#define EMMC_NAND_DMA_SEL		(0x54)

#define EMMC_CLK_O_ICG_EN		(1<<3)
#define EMMC_CARD_STOP_ENABLE           (1<<23)

#define EMMC_SW_RST_DAT			(1<<2)

#if defined(CONFIG_TARGET_RTD1319)
#define EMMC_ISO_pfunc1         (0x20)
#define EMMC_ISO_pfunc2         (0x24)
#define EMMC_ISO_pfunc3         (0x28)
#define EMMC_ISO_pfunc4         (0x2c)
#define EMMC_ISO_pfunc5         (0x30)
#elif defined(CONFIG_TARGET_RTD1619B)
#define EMMC_ISO_pfunc4         (0x9804e030)
#define EMMC_ISO_pfunc5         (0x9804e034)
#define EMMC_ISO_pfunc6         (0x9804e038)
#define EMMC_ISO_pfunc7         (0x9804e03c)
#define EMMC_ISO_pfunc8         (0x9804e040)
#define EMMC_ISO_pfunc9         (0x9804e044)
#define EMMC_ISO_pfunc10        (0x9804e048)
#endif
/* Standard MMC commands (4.1)              type    argument            response */
/* class 1 */
#define MMC_GO_IDLE_STATE           0       /* bc                           */
#define MMC_SEND_OP_COND            1       /* bcr  [31:0] OCR          R3  */
#define MMC_ALL_SEND_CID            2       /* bcr                      R2  */
#define MMC_SET_RELATIVE_ADDR       3       /* ac   [31:16] RCA         R1  */
#define MMC_SET_DSR                 4       /* bc   [31:16] RCA             */
#define MMC_SLEEP_AWAKE             5       /* ac   [31:16] RCA 15:flg  R1b */
#define MMC_SWITCH                  6       /* ac   [31:0] See below    R1b */
#define MMC_SELECT_CARD             7       /* ac   [31:16] RCA         R1  */
#define MMC_SEND_EXT_CSD            8       /* adtc                     R1  */
#define MMC_SEND_CSD                9       /* ac   [31:16] RCA         R2  */
#define MMC_SEND_CID                10      /* ac   [31:16] RCA         R2  */
#define MMC_READ_DAT_UNTIL_STOP     11      /* adtc [31:0] dadr         R1  */
#define MMC_STOP_TRANSMISSION       12      /* ac                       R1b */
#define MMC_SEND_STATUS             13      /* ac   [31:16] RCA         R1  */
#define MMC_GO_INACTIVE_STATE       15      /* ac   [31:16] RCA             */
#define MMC_SPI_READ_OCR            58      /* spi                      spi_R3 */
#define MMC_SPI_CRC_ON_OFF          59      /* spi  [0:0] flag          spi_R1 */

  /* class 2 */
#define MMC_SET_BLOCKLEN            16      /* ac   [31:0] block len    R1  */
#define MMC_READ_SINGLE_BLOCK       17      /* adtc [31:0] data addr    R1  */
#define MMC_READ_MULTIPLE_BLOCK     18      /* adtc [31:0] data addr    R1  */

  /* class 3 */
#define MMC_WRITE_DAT_UNTIL_STOP    20      /* adtc [31:0] data addr    R1  */
#define MMC_SEND_TUNING_BLOCK_HS200     21      /* adtc R1  */
  /* class 4 */
#define MMC_SET_BLOCK_COUNT         23      /* adtc [31:0] data addr    R1  */
#define MMC_WRITE_BLOCK             24      /* adtc [31:0] data addr    R1  */
#define MMC_WRITE_MULTIPLE_BLOCK    25      /* adtc                     R1  */
#define MMC_PROGRAM_CID             26      /* adtc                     R1  */
#define MMC_PROGRAM_CSD             27      /* adtc                     R1  */

  /* class 6 */
#define MMC_SET_WRITE_PROT          28      /* ac   [31:0] data addr    R1b */
#define MMC_CLR_WRITE_PROT          29      /* ac   [31:0] data addr    R1b */
#define MMC_SEND_WRITE_PROT         30      /* adtc [31:0] wpdata addr  R1  */
#define MMC_SEND_WRITE_PROT_TYPE    31      /* adtc [31:0] wpdata addr  R1  */


  /* class 5 */
#define MMC_ERASE_GROUP_START       35      /* ac   [31:0] data addr    R1  */
#define MMC_ERASE_GROUP_END         36      /* ac   [31:0] data addr    R1  */
#define MMC_ERASE                   38      /* ac                       R1b */

  /* class 9 */
#define MMC_FAST_IO                 39      /* ac   <Complex>           R4  */
#define MMC_GO_IRQ_STATE            40      /* bcr                      R5  */

  /* class 7 */
#define MMC_LOCK_UNLOCK             42      /* adtc                     R1b */
/* class 8 */
#define MMC_APP_CMD                 55      /* ac   [31:16] RCA         R1  */
#define MMC_GEN_CMD                 56      /* adtc [0] RD/WR           R1  */

#define R1_OUT_OF_RANGE                 (1 << 31)   /* er, c */
#define R1_ADDRESS_ERROR                (1 << 30)   /* erx, c */
#define R1_BLOCK_LEN_ERROR              (1 << 29)   /* er, c */
#define R1_ERASE_SEQ_ERROR              (1 << 28)   /* er, c */
#define R1_ERASE_PARAM                  (1 << 27)   /* ex, c */
#define R1_WP_VIOLATION                 (1 << 26)   /* erx, c */
#define R1_CARD_IS_LOCKED               (1 << 25)   /* sx, a */
#define R1_LOCK_UNLOCK_FAILED           (1 << 24)   /* erx, c */
#define R1_COM_CRC_ERROR                (1 << 23)   /* er, b */
#define R1_ILLEGAL_COMMAND              (1 << 22)   /* er, b */
#define R1_CARD_ECC_FAILED              (1 << 21)   /* ex, c */
#define R1_CC_ERROR                     (1 << 20)   /* erx, c */
#define R1_ERROR                        (1 << 19)   /* erx, c */
#define R1_UNDERRUN                     (1 << 18)   /* ex, c */
#define R1_OVERRUN                      (1 << 17)   /* ex, c */
#define R1_CID_CSD_OVERWRITE            (1 << 16)   /* erx, c, CID/CSD overwrite */
#define R1_WP_ERASE_SKIP                (1 << 15)   /* sx, c */
#define R1_CARD_ECC_DISABLED            (1 << 14)   /* sx, a */
#define R1_ERASE_RESET                  (1 << 13)   /* sr, c */
#define R1_STATUS(x)                    (x & 0xFFFFE000)
#define R1_CURRENT_STATE(x)             ((x & 0x00001E00) >> 9) /* sx, b (4 bits) */
#define R1_READY_FOR_DATA               (1 << 0)    /* sx, a */
#define R1_SWITCH_ERROR                 (1 << 7)    /* sx, c */
#define R1_APP_CMD                      (1 << 5)    /* sr, c */

#define SYS_PLL_EMMC1                                                                0x980001F0
#define SYS_PLL_EMMC2                                                                0x980001F4
#define SYS_PLL_EMMC3                                                                0x980001F8
#define SYS_PLL_EMMC4                                                                0x980001FC

/*register operation*/
#define PTR_U32(_p)   ((unsigned int)(unsigned long)(_p))
#define PTR_U64(_p)   ((unsigned long)(_p))
#define U64_PTR(_v)   ((void *)(unsigned long)(_v))
#define U32_PTR(_v)   U64_PTR(_v)

/************************************************************************
 * MMC BIT DATA READING
*************************************************************************/
#define UNSTUFF_BITS(resp,start,size)                                                                   \
    ({                                                                                                                  \
        const int __size = size;                                                                                \
        const unsigned short __mask = (unsigned short)(__size < 32 ? 1 << __size : 0) - 1;              \
        const int __off = 3 - ((start) / 32);                                                           \
        const int __shft = (start) & 31;                                                                \
        unsigned short __res;                                                                                   \
                                                                                                                        \
        __res = resp[__off] >> __shft;                                                                  \
        if (__size + __shft > 32)                                                                               \
            __res |= resp[__off-1] << ((32 - __shft) % 32);                                     \
        __res & __mask;                                                                                         \
    })

#ifdef MMC_DEBUG
#define MMCCMDPRINTF(fmt, args...)   printf(fmt,## args)
#define MMCPRINTF(fmt, args...)   printf(fmt,## args)
#define RDPRINTF(fmt, args...)   printf(fmt,## args)  //r/w
#define MPRINTF(fmt, args...)   printf(fmt,## args)   //micron disable standy procedure
#else
#define RDPRINTF(fmt, args...)
#define MMCPRINTF(fmt, args...)
#define MPRINTF(fmt, args...)
#endif


#define EXT_CSD_ERASE_GROUP_DEF                175     /* R/W */
#define EXT_CSD_HC_ERASE_GRP_SIZE      224     /* RO */

#ifdef __RTKEMMC_C__
        #define EXTERN_CALL
#else
        #define EXTERN_CALL extern
#endif

#define MAX_DESCRIPTOR_NUM    0x1ff
#define EMMC_MAX_SCRIPT_BLK   128

#define EMMC_MAX_XFER_BLKCNT MAX_DESCRIPTOR_NUM * EMMC_MAX_SCRIPT_BLK

#define MODE_SD20                       (0x01) //sdr20/50
#define MODE_DDR                        (0x02) //ddr50
#define MODE_SD30                       (0x03) //hs-200

#define CHANGE_FREQ_CARD 0x1
#define CHANGE_FREQ_HOST 0x2
#define CHANGE_FREQ_TYPE 0x3

#define BUS_WIDTH_1             (0x00000000)
#define BUS_WIDTH_4             (0x00000001)
#define BUS_WIDTH_8             (0x00000002)

/*
 * EXT_CSD fields
 */
#define EXT_CSD_BUS_WIDTH               183     /* R/W */
#define EXT_CSD_HS_TIMING               185     /* R/W */
#define EXT_CSD_CARD_TYPE               196     /* RO */
#define EXT_CONT_CSD_VER                194     /* RO */
#define EXT_CSD_REV                     192     /* RO */
#define EXT_CSD_SEC_CNT                 212     /* RO, 4 bytes */
#define EXT_CSD_S_A_TIMEOUT             217

/* cmd1 sector mode */
#define MMC_SECTOR_ADDR         0x40000000

#define FORCE_SECTOR_MODE
//MMC ios caps
#define MMC_IOS_LDO             0x1
#define MMC_IOS_CLK             0x2
#define MMC_IOS_BUSWIDTH        0x4
#define MMC_IOS_INIT_DIV        0x10
#define MMC_IOS_NONE_DIV        0x20
#define MMC_IOS_GET_PAD_DRV     0x40
#define MMC_IOS_SET_PAD_DRV     0x80
#define MMC_IOS_RESTORE_PAD_DRV 0x100

#define CSD_ARRAY_SIZE 512

#define EMMC_SPLIT_BLK

#define R1_CURRENT_STATE(x)             ((x & 0x00001E00) >> 9) /* sx, b (4 bits) */
#define R1_READY_FOR_DATA               (1 << 0)    /* sx, a */

#define SRAM_BASE_ADDR              0x80000000
#define SRAM_SIZE                   (32*1024)
#define IS_SRAM_ADDR(addr)          ((addr>=SRAM_BASE_ADDR)&&(addr<SRAM_BASE_ADDR+SRAM_SIZE))

#define RCA_SHIFTER             16

#define SWAP_32(x) \
    (((uint32_t)(x) << 24) | (((uint32_t)(x) & 0xff00) << 8) |(((uint32_t)(x) & 0x00ff0000) >> 8) | ((uint32_t)(x) >> 24))

#define __ARM_ARCH_8A__ 1
//1295 + armv7 implies armv8 aarch32 mode
#ifdef __ARM_ARCH_8A__
#define CP15ISB asm volatile ("ISB SY" : : : "memory")
#define CP15DSB asm volatile ("DSB SY" : : : "memory")
#define CP15DMB asm volatile ("DMB SY" : : : "memory")
#else
#define CP15ISB asm volatile ("mcr     p15, 0, %0, c7, c5, 4" : : "r" (0))
#define CP15DSB asm volatile ("mcr     p15, 0, %0, c7, c10, 4" : : "r" (0))
#define CP15DMB asm volatile ("mcr     p15, 0, %0, c7, c10, 5" : : "r" (0))
#endif

struct rtk_cmd_info {
	struct mmc_cmd  *                   cmd;
	struct mmc_data *                       data;
        unsigned char *                 dma_buffer;
	unsigned int                        byte_count;
	unsigned int                        block_count;
	unsigned int                        xfer_flag;
#define RTK_MMC_DATA_DIR_MASK   (3 << 6)    /* bit 6~7 */
#define RTK_MMC_DATA_SRAM       (1 << 7)
#define RTK_MMC_DATA_WRITE      (1 << 6)
#define RTK_MMC_TUNING         (1 << 5)
#define RTK_MMC_DATA_READ       (0 << 6)    /* can't use this flag to check status, because its value is zero */
#define RTK_MMC_SRAM_WRITE      RTK_MMC_DATA_SRAM | RTK_MMC_DATA_WRITE
#define RTK_MMC_SRAM_READ       RTK_MMC_DATA_SRAM | RTK_MMC_DATA_READ
#define RTK_MMC_DATA_DIR(pkt)  (pkt->xfer_flag & RTK_MMC_DATA_DIR_MASK)
	unsigned int                        cmd_para;
	unsigned int                rsp_len;
};


/*
 * emmc profile
 */

struct rtk_mmc_ocr_reg {
	unsigned char lowV:1;   //1.7v to 1.95v
	unsigned char reserved1:7;
	unsigned char val1;
	unsigned char val2;
	unsigned char reserved2:5;
	unsigned char access_mode:2;
	unsigned char power_up:1;
};

struct rtk_mmc_ocr {
	unsigned char check1:6;
	unsigned char start:1;
	unsigned char transmission:1;
	unsigned char reg[4];
	unsigned char end:1;
	unsigned char check2:7;
};

struct rtk_mmc_cid {
        unsigned int                                    manfid;
        char                                            prod_name[8];
        unsigned int                                    serial;
        unsigned int                                    oemid;
        unsigned int                                    year;
        unsigned char                                   hwrev;
        unsigned char                                   fwrev;
        unsigned char                                   month;
};

struct rtk_mmc_csd {
        unsigned char                                   csd_ver;
        unsigned char                                   csd_ver2;// from EXT_CSD
        unsigned char                                   mmca_vsn;
        unsigned short                                  cmdclass;
        unsigned short                                  tacc_clks;
        unsigned int                                    tacc_ns;
        unsigned int                                    r2w_factor;
        unsigned int                                    max_dtr;
        unsigned int                                    read_blkbits;
        unsigned int                                    write_blkbits;
        unsigned int                                    capacity;
        unsigned int                                    read_partial:1,
                                                        read_misalign:1,
                                                        write_partial:1,
                                                        write_misalign:1;
};

struct rtk_mmc_ext_csd {
        unsigned int                                    rev;
        unsigned char                                   part_cfg;
        unsigned char                                   boot_cfg;
        unsigned char                                   boot_wp_sts;
        unsigned char                                   boot_wp;
        unsigned char                                   user_wp;
        unsigned char                                   rpmb_size_mult;
        unsigned char                                   partitioning_support;
        unsigned int                                    boot_blk_size;
        unsigned int                                    sa_timeout;
        unsigned int                                    hs_max_dtr;
        unsigned int                                    sectors;
};

typedef struct {
        unsigned int                                    sector_addressing;
        unsigned int                                    curr_part_indx;
        unsigned int                                    rca;                            /* relative card address of device */
        unsigned int                                    raw_cid[4];                 /* raw card CID */
        unsigned int                                    raw_csd[4];                 /* raw card CSD */
        struct rtk_mmc_ocr                      ocr;                                    /* card identification */
        struct rtk_mmc_cid                      cid;                                    /* card identification */
        struct rtk_mmc_csd                      csd;                                    /* card specific */
        struct rtk_mmc_ext_csd                  ext_csd;                                /* mmc v4 extended card specific */
} e_device_type;


//*********************************************************
static unsigned int alloc_ptr = 0x18000000;

static inline uchar* alloc_outside_buffer(unsigned int length){
	unsigned int ptr = alloc_ptr;
	alloc_ptr += length;
	return (uchar*)(uintptr_t)ptr;
}

#define MY_ALLOC_CACHE_ALIGN_BUFFER(type, name, size)	type* name = (type*)alloc_outside_buffer(ROUND(size * sizeof(type), ARCH_DMA_MINALIGN));

static inline void MY_CLR_ALIGN_BUFFER(void){
	alloc_ptr = 0x18000000;
}
//*********************************************************



EXTERN_CALL void rtkemmc_set_bits( unsigned int set_bit );
EXTERN_CALL int rtk_eMMC_init( void );
EXTERN_CALL int rtk_eMMC_write( unsigned int blk_addr, unsigned int data_size, unsigned int * buffer );
EXTERN_CALL int rtk_eMMC_read( unsigned int blk_addr, unsigned int data_size, unsigned int * buffer );
EXTERN_CALL int rtkemmc_Stream_Cmd(struct rtk_cmd_info * cmd_info, unsigned int bIgnore);
void error_handling(unsigned int cmd_idx, unsigned int bIgnore);
void frequency(unsigned int  fre, unsigned int  div);
int emmc_send_cmd_get_rsp(unsigned int cmd_idx, unsigned int sd_arg, unsigned int rsp_con, unsigned int crc);
void switch_speed(UINT8 speed);
UINT32 check_error(int bIgnore);
void mmc_show_ext_csd( unsigned char * pext_csd );
void mmc_show_csd( struct mmc* card );
int mmc_decode_cid( struct mmc * rcard );
extern  unsigned int  emmc_cid[4];
#endif
