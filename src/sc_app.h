/*
 * Copyright (c) 2020 - 2021 Xilinx, Inc.  All rights reserved.
 *
 * SPDX-License-Identifier: MIT
 */

#ifndef SC_APP_H_
#define SC_APP_H_

#include <syslog.h>
#include <sys/ioctl.h>
#include <linux/i2c.h>
#include <linux/i2c-dev.h>

#define LEVELS_MAX	10
#define ITEMS_MAX	24
#define LITEMS_MAX	128
#define STRLEN_MAX	64
#define SYSCMD_MAX	1024

#define APPDIR		"/home/root/.sc_app"
#define LOCKFILE	APPDIR"/lock"
#define CONFIGFILE	APPDIR"/config"
#define BOOTMODEFILE	APPDIR"/boot_mode"
#define SILICONFILE	APPDIR"/silicon"
#define CLOCKFILE	APPDIR"/clock"
#define VOLTAGEFILE	APPDIR"/voltage"
#define IDT8A34001FILE	APPDIR"/8A34001"
#define BOARDFILE	APPDIR"/board"

/*
 * Use busybox-syslog for logging and pick LOG_LOCAL3 facility code
 * to be able to split us from other syslog messages. For example,
 * to save the sc_app messages to /var/log/local3.log file add
 * "local3.*  /var/log/local3.log" to /etc/syslog.conf
 *
 * Assumes system controller has FEATURE_SYSLOG_INFO
 */
#define SC_OPENLOG(ident) openlog(ident, LOG_PID, LOG_LOCAL3)
#define SC_INFO(msg, ...) syslog(LOG_INFO, msg, ##__VA_ARGS__)
#define SC_ERR(msg, ...) do { \
		syslog(LOG_ERR, msg, ##__VA_ARGS__); \
		fprintf(stderr, "ERROR: " msg "\n", ##__VA_ARGS__); \
	} while (0)
#define SC_PRINT(msg, ...) do { \
		syslog(LOG_INFO, msg, ##__VA_ARGS__); \
		fprintf(stdout, msg "\n", ##__VA_ARGS__); \
	} while (0)

/*
 * Feature List
 */
typedef struct {
	int	Numbers;
	char	*Feature[STRLEN_MAX];
} FeatureList_t;

/*
 * Boot Modes
 */
typedef struct {
	char	Name[STRLEN_MAX];
	int	Value;
} BootMode_t;

typedef struct BootModes {
	int	Numbers;
	char	Mode_Lines[4][SYSCMD_MAX];
	BootMode_t	BootMode[ITEMS_MAX];
} BootModes_t;

/*
 * Clocks
 */
typedef enum {
	Si570,
	IDT_8A34001,
} Clock_Type;

typedef struct {
	char	Name[STRLEN_MAX];
	double	Default_Freq;
	double	Upper_Freq;
	double	Lower_Freq;
	Clock_Type	Type;
	void	*Type_Data;
	char	Sysfs_Path[SYSCMD_MAX];
	char	I2C_Bus[STRLEN_MAX];
	int	I2C_Address;
} Clock_t;

typedef struct Clocks {
	int	Numbers;
	Clock_t	Clock[ITEMS_MAX];
} Clocks_t;

typedef struct {
	int	Number_Label;
	char	*Display_Label[SYSCMD_MAX];
	char	*Internal_Label[SYSCMD_MAX];
	int	(*Chip_Reset)(void);
} IDT_8A34001_Data_t;

/*
 * INA226
 */
typedef struct {
	char	Name[STRLEN_MAX];
	char	I2C_Bus[STRLEN_MAX];
	int	I2C_Address;
	int	Shunt_Resistor;
	int	Maximum_Current;
	int	Phase_Multiplier;
} INA226_t;

typedef struct INA226s {
	int	Numbers;
	INA226_t	INA226[ITEMS_MAX];
} INA226s_t;

/*
 * Power Domains
 */
typedef struct {
	char	Name[STRLEN_MAX];
	int	Rails[ITEMS_MAX];
	int	Numbers;
} Power_Domain_t;

typedef struct Power_Domains {
	int	Numbers;
	Power_Domain_t	Power_Domain[ITEMS_MAX];
} Power_Domains_t;

/*
 * Voltages
 */
typedef struct {
	char	Name[STRLEN_MAX];
	char	Part_Name[STRLEN_MAX];
	float	Maximum_Volt;
	float	Typical_Volt;
	float	Minimum_Volt;
	int	Page_Select;
	char	I2C_Bus[STRLEN_MAX];
	int	I2C_Address;
} Voltage_t;

typedef struct Voltages {
	int	Numbers;
	Voltage_t	Voltage[ITEMS_MAX];
} Voltages_t;

/*
 * IO Expander
 */
typedef struct {
	char	Name[STRLEN_MAX];
	int	Numbers;
	char	Labels[ITEMS_MAX][STRLEN_MAX];
	unsigned int	Directions[ITEMS_MAX];
	char	I2C_Bus[STRLEN_MAX];
	int	I2C_Address;
} IO_Exp_t;

typedef enum {
	EEPROM_SUMMARY,
	EEPROM_ALL,
	EEPROM_COMMON,
	EEPROM_BOARD,
	EEPROM_MULTIRECORD,
} EEPROM_Targets;

/*
 * On-Board EEPROM
 */
typedef struct {
	char	Name[STRLEN_MAX];
	char	I2C_Bus[STRLEN_MAX];
	int	I2C_Address;
} OnBoard_EEPROM_t;

/*
 * Daughter Card
 */
typedef struct {
	char	Name[STRLEN_MAX];
	char	I2C_Bus[STRLEN_MAX];
	int	I2C_Address;
} Daughter_Card_t;

/*
 * SFP Transceivers
 */
typedef struct {
	char	Name[STRLEN_MAX];
	char	I2C_Bus[STRLEN_MAX];
	int	I2C_Address;
} SFP_t;

typedef struct SFPs {
	int	Numbers;
	SFP_t	SFP[ITEMS_MAX];
} SFPs_t;

/*
 * QSFP Transceivers
 */
typedef enum {
	qsfp,
	qsfpdd,
} QSFP_Type;

typedef struct {
	char	Name[STRLEN_MAX];
	QSFP_Type	Type;
	char	I2C_Bus[STRLEN_MAX];
	int	I2C_Address;
} QSFP_t;

typedef struct QSFPs {
	int	Numbers;
	QSFP_t	QSFP[ITEMS_MAX];
} QSFPs_t;

/*
 * FMC Cards
 */
typedef struct {
	char	Name[STRLEN_MAX];
	char	I2C_Bus[STRLEN_MAX];
	int	I2C_Address;
} FMC_t;

typedef struct FMCs {
	int	Numbers;
	FMC_t	FMC[ITEMS_MAX];
} FMCs_t;

/*
 * Workarounds
 */
typedef struct {
	char	Name[STRLEN_MAX];
	int	Arg_Needed;
	int	(*Plat_Workaround_Op)(void *);
} Workaround_t;

typedef struct Workarounds {
	int	Numbers;
	Workaround_t	Workaround[ITEMS_MAX];
} Workarounds_t;

/*
 * Board Interface Tests (BITs)
 */
typedef struct {
	char	Instruction[SYSCMD_MAX];
	int	(*Plat_BIT_Op)(void *, void *);
	char	TCL_File[SYSCMD_MAX];
} BIT_Level_t;

typedef struct {
	char	Name[STRLEN_MAX];
	int	Manual;
	int	Levels;
	BIT_Level_t	Level[LEVELS_MAX];
} BIT_t;

typedef struct BITs {
	int	Numbers;
	BIT_t	BIT[ITEMS_MAX];
} BITs_t;

/*
 * DDR serial presence detect (SPD)
 */
struct i2c_info {
	__u16 Bus_addr;
	__u8  Reg_addr;
	__u16 Read_len;
};

/* DDR DIMM */
typedef struct {
	char	Name[STRLEN_MAX];
	char	I2C_Bus[STRLEN_MAX];
	struct i2c_info	Spd;
	struct i2c_info	Therm;
} DIMM_t;

/*
 * GPIO lines
 */
typedef struct {
	int Line;
	const char *Display_Name;
	const char *Internal_Name;
} GPIO_t;

typedef struct GPIOs {
	int Numbers;
	GPIO_t GPIO[LITEMS_MAX];
} GPIOs_t;

/*
 * Board-specific Devices
 */
typedef struct {
	FeatureList_t *FeatureList;
	BootModes_t *BootModes;
	Clocks_t *Clocks;
	INA226s_t *INA226s;
	Power_Domains_t *Power_Domains;
	Voltages_t *Voltages;
	DIMM_t *DIMM;
	GPIOs_t *GPIOs;
	IO_Exp_t *IO_Exp;
	OnBoard_EEPROM_t *OnBoard_EEPROM;
	Daughter_Card_t *Daughter_Card;
	SFPs_t *SFPs;
	QSFPs_t *QSFPs;
	FMCs_t *FMCs;
	Workarounds_t *Workarounds;
	BITs_t *BITs;
} Plat_Devs_t;

/*
 * Board-specific Operations
 */
typedef struct {
	void (*Version_Op)(int *, int *);
	int (*BootMode_Op)(BootMode_t *, int);
	int (*Reset_Op)(void);
	int (*IDCODE_Op)(char *, int);
	int (*XSDB_Op)(const char *, char *, int);
	int (*Temperature_Op)(void);
	int (*QSFP_ModuleSelect_Op)(QSFP_t *, int);
	int (*FMCAutoVadj_Op)(void);
} Plat_Ops_t;

/*
 * Board Info
 */
typedef struct {
	char Name[STRLEN_MAX];
	Plat_Devs_t *Devs;
	Plat_Ops_t *Ops;
} Board_t;

typedef struct {
	int Numbers;
	Board_t Board_Info[ITEMS_MAX];
} Boards_t;

#define I2C_READ(FD, Address, Len, Out, In, Return) \
{ \
	struct i2c_msg Msgs[2]; \
	struct i2c_rdwr_ioctl_data Msgset[1]; \
	Msgs[0].addr = (Address); \
	Msgs[0].flags = 0; \
	Msgs[0].len = 1; \
	Msgs[0].buf = (__u8 *)(Out); \
	Msgs[1].addr = (Address); \
	Msgs[1].flags = (I2C_M_RD | I2C_M_NOSTART); \
	Msgs[1].len = (Len); \
	Msgs[1].buf = (__u8 *)(In); \
	Msgset[0].msgs = Msgs; \
	Msgset[0].nmsgs = 2; \
	if (ioctl((FD), I2C_RDWR, &Msgset) < 0) { \
		SC_ERR("unable to read from I2C device %#x: %m", (Address)); \
		(Return) = -1; \
	} \
}

#define I2C_WRITE(FD, Address, Len, Out, Return) \
{ \
	if (ioctl((FD), I2C_SLAVE_FORCE, (Address)) < 0) { \
		SC_ERR("unable to access I2C device %#x: %m", (Address)); \
		(Return) = -1; \
	} \
	if ((Return) == 0 && write((FD), (Out), (Len)) != (Len)) { \
		SC_ERR("unable to write to I2C device %#x: %m", (Address)); \
		(Return) = -1; \
	} \
}

#define PMBUS_OPERATION			0x1
#define PMBUS_VOUT_MODE			0x20
#define PMBUS_VOUT_COMMAND		0x21
#define PMBUS_VOUT_OV_FAULT_LIMIT	0x40
#define PMBUS_VOUT_OV_WARN_LIMIT	0x42
#define PMBUS_VOUT_UV_WARN_LIMIT	0x43
#define PMBUS_VOUT_UV_FAULT_LIMIT	0x44
#define PMBUS_READ_VOUT			0x8B

/*
 * Definitions for invoking xsdb.
 */
#define XSDB_ENV	"export XILINX_VITIS=/usr/local/xilinx_vitis; \
			 export TCLLIBPATH=/usr/local/xilinx_vitis; \
			 export TCL_LIBRARY=/usr/local/lib/tcl8.5"
#define XSDB_CMD	"/usr/local/xilinx_vitis/xsdb"
#define BIT_PATH	"/usr/share/system-controller-app/BIT/"
#define BOARD_PATH  "/usr/share/system-controller-app/board/"

#define IDCODE_TCL	"idcode/idcode_check.tcl"

#define MAX(x, y)	(((x) > (y)) ? (x) : (y))
#define MIN(x, y)	(((x) < (y)) ? (x) : (y))

#endif	/* SC_APP_H_ */

