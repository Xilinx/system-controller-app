/*
 * Copyright (c) 2020 - 2022 Xilinx, Inc.  All rights reserved.
 * Copyright (c) 2022 - 2023 Advanced Micro Devices, Inc.  All rights reserved.
 *
 * SPDX-License-Identifier: MIT
 */

#ifndef SC_APP_H_
#define SC_APP_H_

#include <syslog.h>
#include <sys/ioctl.h>
#include <sys/socket.h>
#include <sys/un.h>
#include <linux/i2c.h>
#include <linux/i2c-dev.h>

#define LEVELS_MAX	10
#define ITEMS_MAX	24
#define LITEMS_MAX	128
#define STRLEN_MAX	64
#define LSTRLEN_MAX	128
#define SYSCMD_MAX	1024
#define SOCKBUF_MAX	(4 * SYSCMD_MAX)

#define INSTALLDIR	"/usr/share/system-controller-app"
#define SOCKFILE	Appfile("socket")
#define CONFIGFILE	Appfile("config")
#define BOOTMODEFILE	Appfile("boot_mode")
#define SILICONFILE	Appfile("silicon")
#define CLOCKFILE	Appfile("clock")
#define VOLTAGEFILE	Appfile("voltage")
#define IDT8A34001FILE	Appfile("8A34001")
#define PDIFILE		Appfile("PDI")

#define BIT_PATH	INSTALLDIR"/BIT/"
#define BOARD_PATH	INSTALLDIR"/board/"
#define CFS_PATH	INSTALLDIR"/BIT/clock_files/"
#define CUSTOM_CFS_PATH	INSTALLDIR"/.sc_app/clock_files/"
#define SCRIPT_PATH	INSTALLDIR"/script/"
#define CUSTOM_PDIS_PATH	INSTALLDIR"/.sc_app/PDIs/"

#define PROGRAM_8A34001 "8A34001_eeprom.py"

#define SC_INFO(msg, ...) fprintf(stdout, msg "\n", ##__VA_ARGS__);
#define SC_ERR(msg, ...) do { \
		extern int Client_FD; \
		extern char Sock_OutBuffer[]; \
		fprintf(stderr, "ERROR: " msg "\n", ##__VA_ARGS__); \
		if (Client_FD) { \
			sprintf(Sock_OutBuffer, "ERROR: " msg "\n", ##__VA_ARGS__); \
			send(Client_FD, Sock_OutBuffer, strlen(Sock_OutBuffer), MSG_NOSIGNAL); \
		} \
	} while (0)
#define SC_PRINT(msg, ...) do { \
		extern int Client_FD; \
		extern char Sock_OutBuffer[]; \
		fprintf(stdout, msg "\n", ##__VA_ARGS__); \
		if (Client_FD) { \
			sprintf(Sock_OutBuffer, msg "\n", ##__VA_ARGS__); \
			send(Client_FD, Sock_OutBuffer, strlen(Sock_OutBuffer), MSG_NOSIGNAL); \
		} \
	} while (0)
#define SC_PRINT_N(msg, ...) do { \
		extern int Client_FD; \
		extern char Sock_OutBuffer[]; \
		fprintf(stdout, msg, ##__VA_ARGS__); \
		if (Client_FD) { \
			sprintf(Sock_OutBuffer, msg, ##__VA_ARGS__); \
			send(Client_FD, Sock_OutBuffer, strlen(Sock_OutBuffer), MSG_NOSIGNAL); \
		} \
	} while (0)

/*
 * Feature List
 */
typedef struct {
	int	Numbers;
	char	**Feature;
} FeatureList_t;

/*
 * Boot Modes
 */
typedef struct {
	char	*Name;
	int	Value;
} BootMode_t;

typedef struct BootModes {
	int	Numbers;
	char	**Mode_Lines;
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
	char	*Name;
	double	Default_Freq;
	double	Upper_Freq;
	double	Lower_Freq;
	Clock_Type	Type;
	void	*Type_Data;
	char	*Sysfs_Path;
	char	*I2C_Bus;
	int	I2C_Address;
} Clock_t;

typedef struct Clocks {
	int	Numbers;
	Clock_t	Clock[ITEMS_MAX];
} Clocks_t;

typedef struct {
	int	Number_Label;
	char	**Display_Label;
	char	**Internal_Label;
	int	(*Chip_Reset)(void);
} IDT_8A34001_Data_t;

/*
 * INA226
 */
typedef struct {
	char	*Name;
	char	*I2C_Bus;
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
	char	*Name;
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
	char	*Name;
	char	*Part_Name;
	float	Maximum_Volt;
	float	Typical_Volt;
	float	Minimum_Volt;
	int	PMBus_VOUT_MODE;
	int	Page_Select;
	int	Voltage_Multiplier;
	char	*I2C_Bus;
	int	I2C_Address;
} Voltage_t;

typedef struct Voltages {
	int	Numbers;
	Voltage_t	Voltage[ITEMS_MAX];
} Voltages_t;

/*
 * Temperature
 */
typedef struct {
	char	*Name;
	char	*Sensor;
} Temperature_t;

/*
 * IO Expander
 */
typedef struct {
	char	*Name;
	int	Numbers;
	char	**Labels;
	unsigned int	Directions[ITEMS_MAX];
	char	*I2C_Bus;
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
	char	*Name;
	char	*I2C_Bus;
	int	I2C_Address;
} OnBoard_EEPROM_t;

/*
 * Daughter Card
 */
typedef struct {
	char	*Name;
	char	*I2C_Bus;
	int	I2C_Address;
} Daughter_Card_t;

/*
 * SFP Transceivers
 */
typedef enum {
	sfp,
	sfpdd,
	qsfp,
	qsfpdd,
	osfp
} SFP_Type;

typedef struct {
	char	*Name;
	SFP_Type	Type;
	char	*I2C_Bus;
	int	I2C_Address;
} SFP_t;

typedef struct SFPs {
	int	Numbers;
	SFP_t	SFP[ITEMS_MAX];
} SFPs_t;

/*
 * FMC Cards
 */
typedef struct {
	char	*Name;
	char	*I2C_Bus;
	int	I2C_Address;
	int	Label_Numbers;
	char	**Presence_Labels;
	int	Volt_Numbers;
	float	*Supported_Volts;
	char	*Voltage_Regulator;
	float	Default_Volt;
} FMC_t;

typedef struct FMCs {
	int	Numbers;
	FMC_t	FMC[ITEMS_MAX];
} FMCs_t;

/*
 * Workarounds
 */
typedef struct {
	char	*Name;
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
	char	*Instruction;
	int	(*Plat_BIT_Op)(void *, void *);
	char	*TCL_File;
} BIT_Level_t;

typedef struct {
	char	*Name;
	int	Manual;
	int	Levels;
	BIT_Level_t	Level[LEVELS_MAX];
} BIT_t;

typedef struct BITs {
	int	Numbers;
	BIT_t	BIT[ITEMS_MAX];
} BITs_t;

/* DDR DIMMs */
typedef struct {
	char	*Name;
	char	*I2C_Bus;
	int	I2C_Address_SPD;
	int	I2C_Address_Thermal;
} DIMM_t;

typedef struct DIMMs {
	int	Numbers;
	DIMM_t	DIMM[ITEMS_MAX];
} DIMMs_t;

/*
 * GPIO lines
 */

#define IO_TYPES ENC(RW)ENC(RO)ENC(OD)
#define ENC(x) x,
enum io_type_e { IO_TYPES };
#undef ENC
#define ENC(x) #x,

typedef struct {
	const char	*Display_Name;
	const char	*Internal_Name;
	enum io_type_e	Type;
} GPIO_t;

typedef struct GPIOs {
	int		Numbers;
	GPIO_t		GPIO[LITEMS_MAX];
} GPIOs_t;

/*
 * GPIO group of lines
 */
typedef struct {
	char		*Name;
	int		Numbers;
	char		**GPIO_Lines;
	enum io_type_e	Type;
} GPIO_Group_t;

typedef struct {
	int		Numbers;
	GPIO_Group_t	GPIO_Group[ITEMS_MAX];
} GPIO_Groups_t;

/*
 * Command Constraints
 */
typedef struct {
	const char	*Type;
	const char	*Command;
	const char	*Args;
} Constraint_Phase_t;

typedef struct {
	int		Numbers;
	Constraint_Phase_t Phase[ITEMS_MAX];
} Constraint_Phases_t;

typedef struct {
	const char	*Type;
	const char	*Command;
	const char	*Target;
	const char	*Value;
	Constraint_Phases_t *Pre_Phases;
	Constraint_Phases_t *Post_Phases;
} Constraint_t;

typedef struct Constraints {
	int		Numbers;
	Constraint_t	Constraint[LITEMS_MAX];
} Constraints_t;

/*
 * Board-specific Devices
 */
typedef struct {
	FeatureList_t	*FeatureList;
	BootModes_t	*BootModes;
	Clocks_t	*Clocks;
	INA226s_t	*INA226s;
	Power_Domains_t	*Power_Domains;
	Voltages_t	*Voltages;
	Temperature_t	*Temperature;
	DIMMs_t		*DIMMs;
	GPIOs_t		*GPIOs;
	GPIO_Groups_t	*GPIO_Groups;
	IO_Exp_t	*IO_Exp;
	OnBoard_EEPROM_t *OnBoard_EEPROM;
	Daughter_Card_t	*Daughter_Card;
	SFPs_t		*SFPs;
	FMCs_t		*FMCs;
	Workarounds_t	*Workarounds;
	BITs_t		*BITs;
	Constraints_t	*Constraints;
} Plat_Devs_t;

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

#define I2C_TRY_READ(FD, Address, Len, Out, In, Return) \
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
		SC_INFO("unable to read from I2C device %#x: %m", (Address)); \
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

#define IDCODE_TCL	"idcode_verify.tcl"
#define BOOTMODE_TCL	"alt_boot_mode.tcl"
#define PDI_LOAD_TCL	"versal_pdi_download.tcl"
#define DEFAULT_PDI	"system_wrapper.pdi"

#define MAX(x, y)	(((x) > (y)) ? (x) : (y))
#define MIN(x, y)	(((x) < (y)) ? (x) : (y))

#endif	/* SC_APP_H_ */

