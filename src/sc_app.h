/*
 * Copyright (c) 2020 - 2022 Xilinx, Inc.  All rights reserved.
 * Copyright (c) 2022 - 2025 Advanced Micro Devices, Inc.  All rights reserved.
 *
 * SPDX-License-Identifier: MIT
 */

#ifndef SC_APP_H_
#define SC_APP_H_

#include <stdbool.h>
#include <syslog.h>
#include <sys/ioctl.h>
#include <sys/socket.h>
#include <sys/un.h>
#include <linux/i2c.h>
#include <linux/i2c-dev.h>

#define LEVELS_MAX	12
#define ITEMS_MAX	24
#define LITEMS_MAX	128
#define STRLEN_MAX	64
#define LSTRLEN_MAX	128
#define XLSTRLEN_MAX	256
#define XXLSTRLEN_MAX	512
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
#define BITLOGFILE	Appfile("BIT.log")
#define VENDORCLOCKDIR	Appfile("vendor_clock")

#define BIT_PATH	INSTALLDIR"/BIT/"
#define BOARD_PATH	INSTALLDIR"/board/"
#define IDT8A34001_CFS_PATH	INSTALLDIR"/BIT/clock_files/8A34001/"
#define SCRIPT_PATH	INSTALLDIR"/script/"
#define DATADIR		"/data"
#define CUSTOM_CFS_PATH		DATADIR"/clock_files/"
#define CUSTOM_PDIS_PATH	DATADIR"/PDIs/"
#define ONBOARD_EEPROM_PATH	"/sys/bus/i2c/devices/*/eeprom_cc*"
#define RAFT_CLI	"/usr/share/raft/examples/python/pmtool/pm-cmd.py"
#define GPIO_DONE	"VERSAL_DONE"

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
typedef struct {
	char	*Name;
	char	*Part_Name;
	bool	Vendor_Managed;
	int	Outputs;
	double	Default_Freq;
	double	Upper_Freq;
	double	Lower_Freq;
	void	*Type_Data;
	char	*Sysfs_Path;
	char	*Default_Design;
	char	*I2C_Bus;
	int	I2C_Address;
	char	FPGA_Counter_Reg[LEVELS_MAX][LEVELS_MAX];
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
	INA226_t	INA226[LITEMS_MAX];
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
	Voltage_t	Voltage[LITEMS_MAX];
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
	char	*Path;
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
	int	Presence_Boundary_Scan;
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
	char	*Access_Label;
	int	Access_Level;
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
 * Default PDI
 */
typedef struct {
	char	*PDI;
	char	*ImageID;
	char	*UniqueID_Rev0;
	char	*UniqueID_Rev1;
} Default_PDI_t;

/*
 * Board Interface Tests (BITs)
 */
typedef struct {
	char	*Instruction;
	int	(*Plat_BIT_Op)(void *, void *);
	char	Arg[LEVELS_MAX];
	char	*TCL_File;
} BIT_Level_t;

typedef struct {
	char	*Name;
	char	*Description;
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
	Default_PDI_t	*Default_PDI;
} Plat_Devs_t;

#define I2C_READ_BYTES(FD, Address, OutLen, InLen, Out, In, Return) \
{ \
	struct i2c_msg Msgs[2]; \
	struct i2c_rdwr_ioctl_data Msgset[1]; \
	Msgs[0].addr = (Address); \
	Msgs[0].flags = 0; \
	Msgs[0].len = OutLen; \
	Msgs[0].buf = (__u8 *)(Out); \
	Msgs[1].addr = (Address); \
	Msgs[1].flags = (I2C_M_RD | I2C_M_NOSTART); \
	Msgs[1].len = (InLen); \
	Msgs[1].buf = (__u8 *)(In); \
	Msgset[0].msgs = Msgs; \
	Msgset[0].nmsgs = 2; \
	if (ioctl((FD), I2C_RDWR, &Msgset) < 0) { \
		SC_ERR("unable to read from I2C device %#x: %m", (Address)); \
		(Return) = -1; \
	} \
}

#define I2C_READ(FD, Address, Len, Out, In, Return) \
{ \
	I2C_READ_BYTES(FD, Address, 1, Len, Out, In, Return) \
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
#define XSDB_ENV	"source /etc/profile.d/xsdb-variables.sh"
#define XSDB_CMD	"xsdb"

#define IDCODE_TCL	"idcode_verify.tcl"
#define BOOTMODE_TCL	"alt_boot_mode.tcl"
#define BIT_LOAD_TCL	"versal_bit_download.tcl"
#define PDI_LOAD_TCL	"versal_pdi_download.tcl"
#define TCL_CMD_TCL	"versal_tcl_cmd.tcl"
#define SFP_PRES_TCL	"sfp_presence.tcl"
#define DEFAULT_PDI	"system_wrapper.pdi"
#define PROGRAM_8A34001	"8A34001_eeprom.py"
#define READ_CLOCK_CMD	"read_clock"
#define LOAD_DEFAULT_PDI_CMD	"load_default_pdi"

#define MAX(x, y)	(((x) > (y)) ? (x) : (y))
#define MIN(x, y)	(((x) < (y)) ? (x) : (y))

/*
 * Function Declarations
 */
char *Appfile(char *);
int Access_IO_Exp(IO_Exp_t *, int, int, unsigned int *);
int Access_Regulator(Voltage_t *, float *, int);
int Assert_Reset(void *, void *);
int Board_Identification(char *);
int Check_Config_File(char *, char *, int *);
int Clocks_Check(void *, void *);
int DDRMC_Test(void *, void *);
int DIMM_EEPROM_Check(void *, void *);
int Display_Instruction(void *, void *);
int EBM_EEPROM_Check(void *, void *);
int EEPROM_Common(char *);
int EEPROM_Board(char *, int);
int EEPROM_MultiRecord(char *, int);
void FMC_Access(FMC_t *, bool);
int FMCAutoVadj_Op(void);
int Get_BootMode(int);
int Get_GPIO(char *, int *);
int Get_IDCODE(char *, int);
int Get_IDT_8A34001(Clock_t *);
int Get_Measured_Clock(char *, char *);
int Get_Measured_Clock_Vendor(Clock_t *);
int Get_Silicon_Revision(char *);
int Get_Temperature(Temperature_t *);
int JTAG_Op(int);
int Parse_JSON(const char *, Plat_Devs_t *);
int QSFP_ModuleSelect(SFP_t *, int);
int Reset_IDT_8A34001(void);
int Reset_Op(void);
int Restore_IDT_8A34001(Clock_t *);
int Set_AltBootMode(int);
int Set_BootMode(BootMode_t *, int);
int Set_GPIO(char *, int);
int Set_IDT_8A34001(Clock_t *, char *, int);
int Shell_Execute(char *);
int Silicon_Identification(char *, int);
int VCK190_ES1_Vccaux_Workaround(void *);
int VCK190_QSFP_ModuleSelect(SFP_t *, int);
int Vendor_Utility_Clock(Clock_t *, char *, char *, char *);
int Voltages_Check(void *, void *);
int XSDB_BIT(void *, void *);
int XSDB_Op(const char *, const char *, char *, int);

#endif	/* SC_APP_H_ */

