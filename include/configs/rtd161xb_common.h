/*
 * Realtek 1619b common configuration settings
 *
 */

#ifndef __CONFIG_RTK_RTD1619B_COMMON_H
#define __CONFIG_RTK_RTD1619B_COMMON_H

/* Macros to convert string from number */
#ifndef __STR
#define __STR(x) #x
#endif
#ifndef STR
#define STR(x) __STR(x)
#endif

/*
 * Size of malloc() pool
 * Total Size Environment - 128k
 * Malloc - add 256k
 */
/* #define CONFIG_ENV_SIZE			(128 << 10) */
/* #define CONFIG_ENV_IS_IN_REMOTE */
/* #define CONFIG_ENV_ADDR			0x10000000 */
/* #define CONFIG_SYS_MALLOC_LEN		0xE00000 */

/*
 * Hardware drivers
 */

/*
 * serial port - NS16550 compatible
 */
#define V_NS16550_CLK				432000000

/* #define CONFIG_SYS_NS16550 */
/* #define CONFIG_SYS_NS16550_SERIAL */
/* #define CONFIG_SYS_NS16550_REG_SIZE	(-4) */
#define CFG_SYS_NS16550_CLK		V_NS16550_CLK
/* #define CONFIG_SYS_NS16550_UART1_CLK    432000000 */

#define UART1_BASE					0x9801B200
#define UART0_BASE					0x98007800
/* #define CONFIG_CONS_INDEX			1 */
#define CFG_SYS_NS16550_COM1     UART0_BASE

/* #define CONFIG_BAUDRATE				460800 */
#define CFG_SYS_BAUDRATE_TABLE	{4800, 9600, 19200, 38400, 57600, 115200, 460800}

/* #define CONFIG_SYS_CONSOLE_IS_IN_ENV	1 */

#define UART0   0
#define UART1   1
#define UART2   2
#define UART3   3

/*
 * Environment setup
 */

#define CONFIG_EXTRA_ENV_SETTINGS	\
   "fdt_high=0xffffffffffffffff\0"	\
   "initrd_high=0xffffffffffffffff\0"	\

#ifdef CONFIG_CMD_BDI
#define CONFIG_CLOCKS
#endif

/* 1:cache disable   0:enable */
#if 0
	#define CONFIG_SYS_ICACHE_OFF
	#define CONFIG_SYS_DCACHE_OFF
#else
	/* #define CONFIG_NONCACHE_ALLOCATION */
	/* #define CONFIG_SYS_NONCACHED_MEMORY */
	/* #define CONFIG_SYS_NONCACHED_START	0x20000000 */
	/* #define CONFIG_SYS_NONCACHED_SIZE	0x20000000 */
	/* #define CONFIG_CMD_CACHE */
#endif

/*
 * Miscellaneous configurable options
 */

/* #define CONFIG_SYS_LONGHELP		* undef to save memory */

#define CONFIG_SYS_MAXARGS		16

/* Boot Argument Buffer Size */
#define CONFIG_SYS_BARGSIZE		(CONFIG_SYS_CBSIZE)

/* GIC-600 setting */
/* #define CONFIG_GICV3_REALTEK */
#define GICD_BASE			0xFF100000      // FIXME, all these register should be reviewed
#define GICR_RD_BASE 		0xFF140000
#define GICR_SGI_BASE 		0xFF150000
#define GICR_RD_BASE_1 		0xFF160000
#define GICR_SGI_BASE_1	 	0xFF170000
#define GICR_RD_BASE_2 		0xFF180000
#define GICR_SGI_BASE_2 	0xFF190000


#ifdef CONFIG_CMD_NET
	/* Network setting */
	#define CONFIG_ETHADDR				00:10:20:30:40:50
	#define CONFIG_IPADDR				192.168.100.1
	#define CONFIG_GATEWAYIP			192.168.100.254
	#define CONFIG_SERVERIP				192.168.100.2
	#define CONFIG_NETMASK				255.255.255.0
#endif

/* USB Setting */
/*Total USB quantity*/
#define CONFIG_USB_MAX_CONTROLLER_COUNT 4

/* I2C */
#define CONFIG_I2C_MULTI_BUS
#define CONFIG_SYS_I2C_SPEED		100

#endif /* __CONFIG_RTK_RTD1619B_COMMON_H */
