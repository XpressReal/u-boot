/*
 * This file is subject to the terms and conditions of the GNU General Public
 * License.  See the file "COPYING" in the main directory of this archive
 * for more details.
 *
 * Copyright (C) 2018 by YH_HSIEH <yh_hsieh@realtek.com>
 *
 */

#ifndef _ASM_ARCH_FW_INFO_H_
#define _ASM_ARCH_FW_INFO_H_

#include <linux/types.h>

struct rtk_ipc_shm {
/*0C4*/	volatile unsigned int		sys_assign_serial;
/*0C8*/	volatile unsigned int		pov_boot_vd_std_ptr;
/*0CC*/	volatile unsigned int		pov_boot_av_info;
/*0D0*/	volatile unsigned int		audio_rpc_flag;
/*0D4*/	volatile unsigned int		suspend_mask;
/*0D8*/	volatile unsigned int		suspend_flag;
/*0DC*/	volatile unsigned int		vo_vsync_flag;
/*0E0*/	volatile unsigned int		audio_fw_entry_pt;
/*0E4*/	volatile unsigned int		power_saving_ptr;
/*0E8*/	volatile unsigned char		printk_buffer[24];
/*100*/	volatile unsigned int		ir_extended_tbl_pt;
/*104*/	volatile unsigned int		vo_int_sync;
/*108*/	volatile unsigned int		bt_wakeup_flag;			    /* Bit31~24:magic key(0xEA), Bit23:high-active(1) low-active(0), Bit22~0:mask */
/*10C*/	volatile unsigned int		ir_scancode_mask;
/*110*/	volatile unsigned int		ir_wakeup_scancode;
/*114*/	volatile unsigned int		suspend_wakeup_flag;                /* [31-24] magic key(0xEA) [5] cec [4] timer [3] alarm [2] gpio [1] ir [0] lan , (0) disable (1) enable */
/*118*/	volatile unsigned int		acpu_resume_state;                  /* [31-24] magic key(0xEA) [23-16] enum { NONE = 0, UNKNOW, IR, GPIO, LAN, ALARM, TIMER, CEC}  [15-0] ex GPIO Number */
/*11C*/	volatile unsigned int		gpio_wakeup_enable;                 /* [31-24] magic key(0xEA) [23-0] mapping to the number of iso gpio 0~23 */
/*120*/	volatile unsigned int		gpio_wakeup_activity;               /* [31-24] magic key(0xEA) [23-0] mapping to the number of iso gpio 0~23 , (0) low activity (1) high activity */
/*124*/	volatile unsigned int		gpio_output_change_enable;          /* [31-24] magic key(0xEA) [23-0] mapping to the number of iso gpio 0~23 */
/*128*/	volatile unsigned int		gpio_output_change_activity;        /* [31-24] magic key(0xEA) [23-0] mapping to the number of iso gpio 0~23 , (0) low activity (1) high activity AT SUSPEND TIME */
/*12C*/	volatile unsigned int		audio_reciprocal_timer_sec;         /* [31-24] magic key(0xEA) [23-0] audio reciprocal timer (sec) */
/*130*/	volatile unsigned int		u_boot_version_magic;
/*134*/	volatile unsigned int		u_boot_version_info;
/*138*/	volatile unsigned int		suspend_watchdog;                   /* [31-24] magic key(0xEA) [23] state (0) disable (1) enable [22] acpu response state [15-0] watch timeout (sec) */
/*13C*/	volatile unsigned int		xen_domu_boot_st;                   /* [31-24] magic key(0xEA) [23-20] version [19-16] Author (1) ACPU (2) SCPU [15-8] STATE [7-0] EXT */
/*140*/	volatile unsigned int		gpio_wakeup_enable2;		    /* [31-24] magic key(0xEA) [10-0] mapping to the number of iso gpio 24~34 */
/*144*/	volatile unsigned int		gpio_wakeup_activity2;		    /* [31-24] magic key(0xEA) [10-0] mapping to the number of iso gpio 24~34 , (0) low activity (1) high activity */
/*148*/	volatile unsigned int		gpio_output_change_enable2;	    /* [31-24] magic key(0xEA) [10-0] mapping to the number of iso gpio 24~34 */
/*14C*/	volatile unsigned int		gpio_output_change_activity2;	    /* [31-24] magic key(0xEA) [10-0] mapping to the number of iso gpio 24~34 , (0) low activity (1) high activity AT SUSPEND TIME */
/*150*/	volatile unsigned int 		gpio_wakeup_enable3;
/*154*/	volatile unsigned int 		gpio_wakeup_activity3;
/*158*/	volatile unsigned int		video_rpc_flag;
/*15C*/	volatile unsigned int		video_int_sync;
/*160*/	volatile unsigned char		video_printk_buffer[24];
/*178*/	volatile unsigned int 		video_suspend_mask;
/*17C*/	volatile unsigned int		video_suspend_flag;
/*180*/	volatile unsigned int 		ve3_rpc_flag;
/*184*/	volatile unsigned int		ve3_int_sync;
/*188*/	volatile unsigned int		reserved1[30];
/*200*/	volatile unsigned int		hifi_rpc_flag;
/*204*/	volatile unsigned int		reserved2[31];
/*280*/	volatile unsigned int		hifi_acpu_hifiWrite[32];
/*300*/	volatile unsigned int		hifi_acpu_hifiRead[32];
/*380*/ volatile unsigned char		hifi_printk_buffer[128];
};

typedef struct {
	uint	dwMagicNumber;		//identificatin String "$RTK" or "STD3"

	uint	dwVideoStreamAddress;	//The Location of Video ES Stream
	uint	dwVideoStreamLength;	//Video ES Stream Length

	uint	dwAudioStreamAddress;	//The Location of Audio ES Stream
	uint	dwAudioStreamLength;	//Audio ES Stream Length

	uchar	bVideoDone;
	uchar	bAudioDone;

	uchar	bHDMImode;		//monitor device mode (DVI/HDMI)

	char	dwAudioStreamVolume;	//Audio ES Stream Volume -60 ~ 40

	char	dwAudioStreamRepeat;	//0 : no repeat ; 1 :repeat

	uchar	bPowerOnImage;		// Alternative of power on image or video
	uchar	bRotate;				// enable v &h flip
	uint	dwVideoStreamType;	// Video Stream Type

	uint	audio_buffer_addr;	// Audio decode buffer
	uint	audio_buffer_size;
	uint	video_buffer_addr;	// Video decode buffer
	uint	video_buffer_size;
	uint	last_image_addr;	// Buffer used to keep last image
	uint	last_image_size;

	uchar	logo_enable;
	uint	logo_addr;
	u_int   src_width;              //logo_src_width
	u_int   src_height;             //logo_src_height
	u_int   dst_width;              //logo_dst_width
	u_int   dst_height;             //logo_dst_height
	uint    logo_addr_2;

	uint	vo_secure_addr;		//Address for secure vo
	uint	is_OSD_mixer1;
	uint	bGraphicBufferDone;
	uint	is_from_fastboot_resume;
	uint	zoomMagic;              //0x2452544F
	uint	zoomFunction;           //bit0: zoomratio
	uint	zoomXYWHRatio_x;
	uint	zoomXYWHRatio_y;
	uint	zoomXYWHRatio_w;
	uint	zoomXYWHRatio_h;
	uint	zoomXYWHRatio_base;
	uint	video_DLSR_binary_addr;//0x24525451
	uint	video_DLSR_binary_addr_svp;//0x24525451
} boot_av_info_t;

#endif	// _ASM_ARCH_FW_INFO_H_
