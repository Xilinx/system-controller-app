/*
 * Copyright (c) 2020 - 2021 Xilinx, Inc.  All rights reserved.
 *
 * SPDX-License-Identifier: MIT
 */

#include <stdio.h>
#include <stdlib.h>
#include <fcntl.h>
#include <unistd.h>
#include <time.h>
#include <string.h>
#include <sys/ioctl.h>
#include <linux/i2c-dev.h>
#include "sc_app.h"

#define BOOTMODE_TCL	"boot_mode/alt_boot_mode.tcl"
#define QSFP_MODSEL_TCL	"qsfp_set_modsel/qsfp_download.tcl"

extern Plat_Devs_t *Plat_Devs;
extern Plat_Ops_t *Plat_Ops;

int VCK190_JTAG_Op(int);
int VCK190_XSDB_Op(const char *, char *, int);
int VCK190_IDT_8A34001_Reset(void);
int Workaround_Vccaux(void *Arg);
extern int Access_Regulator(Voltage_t *, float *, int);
extern int Access_IO_Exp(IO_Exp_t *, int, int, unsigned int *);
extern int FMC_Vadj_Range(FMC_t *, float *, float *);
extern int GPIO_Get(char *, int *);
extern int GPIO_Set(char *, int);
extern int Clocks_Check(void *);
extern int XSDB_BIT(void *);
extern int EBM_EEPROM_Check(void *);
extern int DIMM_EEPROM_Check(void *);
extern int Voltages_Check(void *);

/*
 * Feature List
 */
FeatureList_t VCK190_FeatureList = {
	.Numbers = 15,
	.Feature = { \
		"eeprom", "temp", "bootmode", "clock", "voltage", "power", \
		"powerdomain", "BIT", "ddr", "gpio", "ioexp", "SFP", "QSFP", \
		"EBM", "FMC", \
	 },
};

/*
 * Boot Modes
 */
typedef enum {
	JTAG,
	QSPI24,
	QSPI32,
	SD0_LS,
	SD1,
	EMMC,
	USB,
	OSPI,
	SMAP,
	DFT,
	SD1_LS,
	BOOTMODE_MAX,
} BootMode_Index;

BootModes_t VCK190_BootModes = {
	.Numbers = BOOTMODE_MAX,
	.Mode_Lines = {
		 "SYSCTLR_VERSAL_MODE0", "SYSCTLR_VERSAL_MODE1",
		 "SYSCTLR_VERSAL_MODE2", "SYSCTLR_VERSAL_MODE3"
	},
	.BootMode[JTAG] = {
		.Name = "jtag",
		.Value = 0x0,
	},
	.BootMode[QSPI24] = {
		.Name = "qspi24",
		.Value = 0x1,
	},
	.BootMode[QSPI32] = {
		.Name = "qspi32",
		.Value = 0x2,
	},
	.BootMode[SD0_LS] = {
		.Name = "sd0_ls",
		.Value = 0x3,
	},
	.BootMode[SD1] = {
		.Name = "sd1",
		.Value = 0x5,
	},
	.BootMode[EMMC] = {
		.Name = "emmc",
		.Value = 0x6,
	},
	.BootMode[USB] = {
		.Name = "usb",
		.Value = 0x7,
	},
	.BootMode[OSPI] = {
		.Name = "ospi",
		.Value = 0x8,
	},
	.BootMode[SMAP] = {
		.Name = "smap",
		.Value = 0xa,
	},
	.BootMode[DFT] = {
		.Name = "dft",
		.Value = 0xd,
	},
	.BootMode[SD1_LS] = {
		.Name = "sd1_ls",
		.Value = 0xe,
	},
};

/*
 * Clocks
 */
IDT_8A34001_Data_t VCK190_IDT_8A34001_Data = {
	.Number_Label = 14,
	.Display_Label = { "CIN 1 From Bank 200 GTY_REF", \
			   "CIN 2 From Bank 706", \
			   "Q0 to CIN 0", \
			   "Q1 to Bank 200 GTY_REF", \
			   "Q2 to Bank 201, 204-206 GTY_REF", \
			   "Q3 to FMC2 REFCLK", \
			   "Q4 to FMC2 SYNC", \
			   "Q5 to J328", \
			   "Q6 to NC", \
			   "Q7 to NC", \
			   "Q8 to NC", \
			   "Q9 to NC", \
			   "Q10 to NC", \
			   "Q11 to NC", \
	},
	.Internal_Label = { "IN1_Frequency", \
			    "IN2_Frequency", \
			    "OUT0DesiredFrequency", \
			    "OUT1DesiredFrequency", \
			    "OUT2DesiredFrequency", \
			    "OUT3DesiredFrequency", \
			    "OUT4DesiredFrequency", \
			    "OUT5DesiredFrequency", \
			    "OUT6DesiredFrequency", \
			    "OUT7DesiredFrequency", \
			    "OUT8DesiredFrequency", \
			    "OUT9DesiredFrequency", \
			    "OUT10DesiredFrequency", \
			    "OUT11DesiredFrequency", \
	},
	.Chip_Reset = VCK190_IDT_8A34001_Reset,
};

typedef enum {
	SI570_ZSFP,
	SI570_USER1_FMC,
	IDT_8A34001_FMC2,
	SI570_REF,
	SI570_DIMM1,
	SI570_LPDDR4_CLK1,
	SI570_LPDDR4_CLK2,
	SI570_HSPD,
	CLOCK_MAX,
} Clock_Index;

Clocks_t VCK190_Clocks = {
	.Numbers = CLOCK_MAX,
	.Clock[SI570_ZSFP] = {
		.Name = "zSFP Si570",		// Name of the device referenced by application
		.Type = Si570,			// Clock generators accessible through sysfs
		.Sysfs_Path = "/sys/devices/platform/si570_zsfp_clk/set_rate",
		.Default_Freq = 156.25,		// Factory default frequency in MHz
		.Upper_Freq = 810.0,		// Upper-bound frequency in MHz
		.Lower_Freq = 10.0,		// Lower-bound frequency in MHz
		.I2C_Bus = "/dev/i2c-8",	// I2C bus address behind the mux
		.I2C_Address = 0x5d,		// I2C device address
	},
	.Clock[SI570_USER1_FMC] = {
		.Name = "User1 FMC2 Si570",
		.Type = Si570,
		.Sysfs_Path = "/sys/devices/platform/si570_user1_clk/set_rate",
		.Default_Freq = 100.0,
		.Upper_Freq = 1250.0,
		.Lower_Freq = 10.0,
		.I2C_Bus = "/dev/i2c-9",
		.I2C_Address = 0x5f,
	},
	.Clock[IDT_8A34001_FMC2] = {
		.Name = "8A34001 FMC2",
		.Type = IDT_8A34001,
		.Type_Data = &VCK190_IDT_8A34001_Data,
		.I2C_Bus = "/dev/i2c-18",
		.I2C_Address = 0x5b,
	},
	.Clock[SI570_REF] = {
		.Name = "Versal Sys Clk Si570",
		.Type = Si570,
		.Sysfs_Path = "/sys/devices/platform/ref_clk/set_rate",
		.Default_Freq = 33.333,
		.Upper_Freq = 160.0,
		.Lower_Freq = 10.0,
		.I2C_Bus = "/dev/i2c-11",
		.I2C_Address = 0x5d,
	},
	.Clock[SI570_DIMM1] = {
		.Name = "Dimm1 Si570",
		.Type = Si570,
		.Sysfs_Path = "/sys/devices/platform/si570_ddrdimm1_clk/set_rate",
		.Default_Freq = 200.0,
		.Upper_Freq = 810.0,
		.Lower_Freq = 10.0,
		.I2C_Bus = "/dev/i2c-14",
		.I2C_Address = 0x60,
	},
	.Clock[SI570_LPDDR4_CLK1] = {
		.Name = "LPDDR4 CLK1 Si570",
		.Type = Si570,
		.Sysfs_Path = "/sys/devices/platform/si570_lpddr4_clk1/set_rate",
		.Default_Freq = 200.0,
		.Upper_Freq = 810.0,
		.Lower_Freq = 10.0,
		.I2C_Bus = "/dev/i2c-16",
		.I2C_Address = 0x60,
	},
	.Clock[SI570_LPDDR4_CLK2] = {
		.Name = "LPDDR4 CLK2 Si570",
		.Type = Si570,
		.Sysfs_Path = "/sys/devices/platform/si570_lpddr4_clk2/set_rate",
		.Default_Freq = 200.0,
		.Upper_Freq = 810.0,
		.Lower_Freq = 10.0,
		.I2C_Bus = "/dev/i2c-15",
		.I2C_Address = 0x60,
	},
	.Clock[SI570_HSPD] = {
		.Name = "HSPD Si570",
		.Type = Si570,
		.Sysfs_Path = "/sys/devices/platform/si570_hsdp_clk/set_rate",
		.Default_Freq = 156.25,
		.Upper_Freq = 810.0,
		.Lower_Freq = 10.0,
		.I2C_Bus = "/dev/i2c-17",
		.I2C_Address = 0x5d,
	},
};

/*
 * INA226
 */
typedef enum {
	INA226_VCCINT,
	INA226_VCC_SOC,
	INA226_VCC_PMC,
	INA226_VCCINT_RAM,
	INA226_VCCINT_PSLP,
	INA226_VCCINT_PSFP,
	INA226_VCCAUX,
	INA226_VCCAUX_PMC,
	INA226_VCC_MIO,
	INA226_VCC1V8,
	INA226_VCC3V3,
	INA226_VCC1V2_DDR4,
	INA226_VCC1V1_LP4,
	INA226_VADJ_FMC,
	INA226_MGTYAVCC,
	INA226_MGTYAVTT,
	INA226_MGTYVCCAUX,
	INA226_MAX,
} Ina226_Index;

Ina226s_t VCK190_Ina226s = {
	.Numbers = INA226_MAX,
	.Ina226[INA226_VCCINT] = {
		.Name = "VCCINT",		// Name of the device referenced by application
		.I2C_Bus = "/dev/i2c-4",	// I2C bus of the device
		.I2C_Address = 0x40,		// I2C address of the device
		.Shunt_Resistor = 500,		// Value of shunt resistor in Micro-Ohm
		.Phase_Multiplier = 6,		// Sensor for # of phases
	},
	.Ina226[INA226_VCC_SOC] = {
		.Name = "VCC_SOC",
		.I2C_Bus = "/dev/i2c-4",
		.I2C_Address = 0x41,
		.Shunt_Resistor = 500,
		.Phase_Multiplier = 1,
	},
	.Ina226[INA226_VCC_PMC] = {
		.Name = "VCC_PMC",
		.I2C_Bus = "/dev/i2c-4",
		.I2C_Address = 0x42,
		.Shunt_Resistor = 5000,
		.Phase_Multiplier = 1,
	},
	.Ina226[INA226_VCCINT_RAM] = {
		.Name = "VCCINT_RAM",
		.I2C_Bus = "/dev/i2c-4",
		.I2C_Address = 0x43,
		.Shunt_Resistor = 5000,
		.Phase_Multiplier = 1,
	},
	.Ina226[INA226_VCCINT_PSLP] = {
		.Name = "VCCINT_PSLP",
		.I2C_Bus = "/dev/i2c-4",
		.I2C_Address = 0x44,
		.Shunt_Resistor = 5000,
		.Phase_Multiplier = 1,
	},
	.Ina226[INA226_VCCINT_PSFP] = {
		.Name = "VCCINT_PSFP",
		.I2C_Bus = "/dev/i2c-4",
		.I2C_Address = 0x45,
		.Shunt_Resistor = 5000,
		.Phase_Multiplier = 1,
	},
	.Ina226[INA226_VCCAUX] = {
		.Name = "VCCAUX",
		.I2C_Bus = "/dev/i2c-6",
		.I2C_Address = 0x40,
		.Shunt_Resistor = 5000,
		.Phase_Multiplier = 1,
	},
	.Ina226[INA226_VCCAUX_PMC] = {
		.Name = "VCCAUX_PMC",
		.I2C_Bus = "/dev/i2c-6",
		.I2C_Address = 0x41,
		.Shunt_Resistor = 5000,
		.Phase_Multiplier = 1,
	},
	.Ina226[INA226_VCC_MIO] = {
		.Name = "VCC_MIO",
		.I2C_Bus = "/dev/i2c-6",
		.I2C_Address = 0x45,
		.Shunt_Resistor = 5000,
		.Phase_Multiplier = 1,
	},
	.Ina226[INA226_VCC1V8] = {
		.Name = "VCC1V8",
		.I2C_Bus = "/dev/i2c-6",
		.I2C_Address = 0x46,
		.Shunt_Resistor = 5000,
		.Phase_Multiplier = 1,
	},
	.Ina226[INA226_VCC3V3] = {
		.Name = "VCC3V3",
		.I2C_Bus = "/dev/i2c-6",
		.I2C_Address = 0x47,
		.Shunt_Resistor = 5000,
		.Phase_Multiplier = 1,
	},
	.Ina226[INA226_VCC1V2_DDR4] = {
		.Name = "VCC1V2_DDR4",
		.I2C_Bus = "/dev/i2c-6",
		.I2C_Address = 0x48,
		.Shunt_Resistor = 5000,
		.Phase_Multiplier = 1,
	},
	.Ina226[INA226_VCC1V1_LP4] = {
		.Name = "VCC1V1_LP4",
		.I2C_Bus = "/dev/i2c-6",
		.I2C_Address = 0x49,
		.Shunt_Resistor = 5000,
		.Phase_Multiplier = 1,
	},
	.Ina226[INA226_VADJ_FMC] = {
		.Name = "VADJ_FMC",
		.I2C_Bus = "/dev/i2c-6",
		.I2C_Address = 0x4A,
		.Shunt_Resistor = 2000,
		.Phase_Multiplier = 1,
	},
	.Ina226[INA226_MGTYAVCC] = {
		.Name = "MGTYAVCC",
		.I2C_Bus = "/dev/i2c-6",
		.I2C_Address = 0x4B,
		.Shunt_Resistor = 2000,
		.Phase_Multiplier = 1,
	},
	.Ina226[INA226_MGTYAVTT] = {
		.Name = "MGTYAVTT",
		.I2C_Bus = "/dev/i2c-6",
		.I2C_Address = 0x4C,
		.Shunt_Resistor = 2000,
		.Phase_Multiplier = 1,
	},
	.Ina226[INA226_MGTYVCCAUX] = {
		.Name = "MGTYVCCAUX",
		.I2C_Bus = "/dev/i2c-6",
		.I2C_Address = 0x4D,
		.Shunt_Resistor = 5000,
		.Phase_Multiplier = 1,
	},
};

/*
 * Power Domains
 */
typedef enum {
	PD_FPD,
	PD_LPD,
	PD_PLD,
	PD_PMC,
	PD_GTY,
	PD_FMC,
	PD_SYSTEM,
	PD_CHIP,
	PD_MAX,
} Power_Domain_Index;

Power_Domains_t VCK190_Power_Domains = {
	.Numbers = PD_MAX,
	.Power_Domain[PD_FPD] = {
		.Name = "FPD",
		.Rails = { INA226_VCCINT_PSFP },
		.Numbers = 1,
	},
	.Power_Domain[PD_LPD] = {
		.Name = "LPD",
		.Rails = { INA226_VCCINT_PSLP },
		.Numbers = 1,
	},
	.Power_Domain[PD_PLD] = {
		.Name = "PLD",
		.Rails = { INA226_VCCINT,
			   INA226_VCC1V8,
			   INA226_VCC3V3,
			   INA226_VCC1V2_DDR4,
			   INA226_VCC1V1_LP4,
			   INA226_VCCINT_RAM,
			   INA226_VCCAUX },
		.Numbers = 7,
	},
	.Power_Domain[PD_PMC] = {
		.Name = "PMC",
		.Rails = { INA226_VCC_PMC,
			   INA226_VCC_MIO,
			   INA226_VCCAUX_PMC },
		.Numbers = 3,
	},
	.Power_Domain[PD_GTY] = {
		.Name = "GTY",
		.Rails = { INA226_MGTYAVCC,
			   INA226_MGTYAVTT,
			   INA226_MGTYVCCAUX },
		.Numbers = 3,
	},
	.Power_Domain[PD_FMC] = {
		.Name = "FMC",
		.Rails = { INA226_VADJ_FMC },
		.Numbers = 1,
	},
	.Power_Domain[PD_SYSTEM] = {
		.Name = "system",
		.Rails = { INA226_VCC_SOC },
		.Numbers = 1,
	},
	.Power_Domain[PD_CHIP] = {
		.Name = "chip",
		.Rails = { INA226_VCCINT,
			   INA226_VCC_SOC,
			   INA226_VCC_PMC,
			   INA226_VCCINT_RAM,
			   INA226_VCCINT_PSLP,
			   INA226_VCCINT_PSFP,
			   INA226_VCCAUX,
			   INA226_VCCAUX_PMC,
			   INA226_VCC_MIO,
			   INA226_VCC1V8,
			   INA226_VCC3V3,
			   INA226_VCC1V2_DDR4,
			   INA226_VCC1V1_LP4,
			   INA226_VADJ_FMC,
			   INA226_MGTYAVCC,
			   INA226_MGTYAVTT,
			   INA226_MGTYVCCAUX },
		.Numbers = INA226_MAX,
	},
};

/*
 * Voltages - power rails
 */
typedef enum {
	VOLTAGE_VCCINT,
	VOLTAGE_VCCINT_SOC,
	VOLTAGE_VCCINT_PSLP,
	VOLTAGE_VCCINT_PSFP,
	VOLTAGE_VCCAUX,
	VOLTAGE_VCC_RAM,
	VOLTAGE_VCC_PMC,
	VOLTAGE_VCCO_MIO,
	VOLTAGE_VCC3V3,
	VOLTAGE_VCC1V8,
	VOLTAGE_VCCAUX_PMC,
	VOLTAGE_MGTYAVTT,
	VOLTAGE_VADJ_FMC,
	VOLTAGE_MGTYAVCC,
	VOLTAGE_UTIL_1V13,
	VOLTAGE_UTIL_2V5,
	VOLTAGE_VCC1V2_DDR4,
	VOLTAGE_VCC1V1_LP4,
	VOLTAGE_MAX,
} Voltage_Index;

Voltages_t VCK190_Voltages = {
	.Numbers = VOLTAGE_MAX,
	.Voltage[VOLTAGE_VCCINT] = {
		.Name = "VCCINT",		// Name of the device referenced by application
		.Part_Name = "IR35215",		// Name of the part that is used
		.Supported_Volt = { 0.8, 0.88, -1 }, // List of supported voltages (volt)
		.Maximum_Volt = VOLT_MAX(0.8),	// Maximum voltage (volt)
		.Typical_Volt = 0.8,		// Typical voltage (volt)
		.Minimum_Volt = VOLT_MIN(0.8),	// Minimum voltage (volt)
		.I2C_Bus = "/dev/i2c-3",	// I2C bus of the device
		.I2C_Address = 0x46,		// I2C address of the device
		.Page_Select = 0,		// Device page selection
	},
	.Voltage[VOLTAGE_VCCINT_SOC] = {
		.Name = "VCCINT_SOC",
		.Part_Name = "IR35215",
		.Supported_Volt = { 0.8, -1 },
		.Maximum_Volt = VOLT_MAX(0.8),
		.Typical_Volt = 0.8,
		.Minimum_Volt = VOLT_MIN(0.8),
		.I2C_Bus = "/dev/i2c-3",
		.I2C_Address = 0x46,
		.Page_Select = 1,
	},
	.Voltage[VOLTAGE_VCCINT_PSLP] = {
		.Name = "VCCINT_PSLP",
		.Part_Name = "IRPS5401",
		.Supported_Volt = { 0.8, -1 },
		.Maximum_Volt = VOLT_MAX(0.8),
		.Typical_Volt = 0.8,
		.Minimum_Volt = VOLT_MIN(0.8),
		.I2C_Bus = "/dev/i2c-3",
		.I2C_Address = 0x47,
		.Page_Select = 0,
	},
	.Voltage[VOLTAGE_VCCINT_PSFP] = {
		.Name = "VCCINT_PSFP",
		.Part_Name = "IRPS5401",
		.Supported_Volt = { 0.8, -1 },
		.Maximum_Volt = VOLT_MAX(0.8),
		.Typical_Volt = 0.8,
		.Minimum_Volt = VOLT_MIN(0.8),
		.I2C_Bus = "/dev/i2c-3",
		.I2C_Address = 0x47,
		.Page_Select = 1,
	},
	.Voltage[VOLTAGE_VCCAUX] = {
		.Name = "VCCAUX",
		.Part_Name = "IRPS5401",
		.Supported_Volt = { 1.5, -1 },
		.Maximum_Volt = VOLT_MAX(1.5),
		.Typical_Volt = 1.5,
		.Minimum_Volt = VOLT_MIN(1.5),
		.I2C_Bus = "/dev/i2c-3",
		.I2C_Address = 0x47,
		.Page_Select = 2,
	},
	.Voltage[VOLTAGE_VCC_RAM] = {
		.Name = "VCC_RAM",
		.Part_Name = "IRPS5401",
		.Supported_Volt = { 0.0, 0.8, -1 },
		.Maximum_Volt = VOLT_MAX(0.8),
		.Typical_Volt = 0.8,
		.Minimum_Volt = 0.0,		// Due to vccaux workaround
		.I2C_Bus = "/dev/i2c-3",
		.I2C_Address = 0x47,
		.Page_Select = 3,
	},
	.Voltage[VOLTAGE_VCC_PMC] = {
		.Name = "VCC_PMC",
		.Part_Name = "IRPS5401",
		.Supported_Volt = { 0.8, -1 },
		.Maximum_Volt = VOLT_MAX(0.8),
		.Typical_Volt = 0.8,
		.Minimum_Volt = VOLT_MIN(0.8),
		.I2C_Bus = "/dev/i2c-3",
		.I2C_Address = 0x47,
		.Page_Select = 4,
	},
	.Voltage[VOLTAGE_VCCO_MIO] = {
		.Name = "VCCO_MIO",
		.Part_Name = "IRPS5401",
		.Supported_Volt = { 1.8, -1 },
		.Maximum_Volt = VOLT_MAX(1.8),
		.Typical_Volt = 1.8,
		.Minimum_Volt = VOLT_MIN(1.8),
		.I2C_Bus = "/dev/i2c-3",
		.I2C_Address = 0x4C,
		.Page_Select = 0,
	},
	.Voltage[VOLTAGE_VCC3V3] = {
		.Name = "VCC3V3",
		.Part_Name = "IRPS5401",
		.Supported_Volt = { 3.3, -1 },
		.Maximum_Volt = VOLT_MAX(3.3),
		.Typical_Volt = 3.3,
		.Minimum_Volt = VOLT_MIN(3.3),
		.I2C_Bus = "/dev/i2c-3",
		.I2C_Address = 0x4C,
		.Page_Select = 1,
	},
	.Voltage[VOLTAGE_VCC1V8] = {
		.Name = "VCC1V8",
		.Part_Name = "IRPS5401",
		.Supported_Volt = { 1.8, -1 },
		.Maximum_Volt = VOLT_MAX(1.8),
		.Typical_Volt = 1.8,
		.Minimum_Volt = VOLT_MIN(1.8),
		.I2C_Bus = "/dev/i2c-3",
		.I2C_Address = 0x4C,
		.Page_Select = 2,
	},
	.Voltage[VOLTAGE_VCCAUX_PMC] = {
		.Name = "VCCAUX_PMC",
		.Part_Name = "IRPS5401",
		.Supported_Volt = { 1.5, -1 },
		.Maximum_Volt = VOLT_MAX(1.5),
		.Typical_Volt = 1.5,
		.Minimum_Volt = VOLT_MIN(1.5),
		.I2C_Bus = "/dev/i2c-3",
		.I2C_Address = 0x4C,
		.Page_Select = 4,
	},
	.Voltage[VOLTAGE_MGTYAVTT] = {
		.Name = "MGTYAVTT",
		.Part_Name = "IR38164",
		.Supported_Volt = { 1.2, -1 },
		.Maximum_Volt = VOLT_MAX(1.2),
		.Typical_Volt = 1.2,
		.Minimum_Volt = VOLT_MIN(1.2),
		.I2C_Bus = "/dev/i2c-3",
		.I2C_Address = 0x50,
		.Page_Select = -1,
	},
	.Voltage[VOLTAGE_VADJ_FMC] = {
		.Name = "VADJ_FMC",
		.Part_Name = "IR38164",
		.Supported_Volt = { 0.0, 1.2, 1.5, -1 },
		.Maximum_Volt = VOLT_MAX(1.5),
		.Typical_Volt = 1.5,
		.Minimum_Volt = 0.0,
		.I2C_Bus = "/dev/i2c-3",
		.I2C_Address = 0x4E,
		.Page_Select = -1,
	},
	.Voltage[VOLTAGE_MGTYAVCC] = {
		.Name = "MGTYAVCC",
		.Part_Name = "IR38164",
		.Supported_Volt = { 0.88, -1 },
		.Maximum_Volt = VOLT_MAX(0.88),
		.Typical_Volt = 0.88,
		.Minimum_Volt = VOLT_MIN(0.88),
		.I2C_Bus = "/dev/i2c-3",
		.I2C_Address = 0x4F,
		.Page_Select = -1,
	},
	.Voltage[VOLTAGE_UTIL_1V13] = {
		.Name = "UTIL_1V13",
		.Part_Name = "IRPS5401",
		.Supported_Volt = { 1.13, -1 },
		.Maximum_Volt = VOLT_MAX(1.13),
		.Typical_Volt = 1.13,
		.Minimum_Volt = VOLT_MIN(1.13),
		.I2C_Bus = "/dev/i2c-3",
		.I2C_Address = 0x4D,
		.Page_Select = 0,
	},
	.Voltage[VOLTAGE_UTIL_2V5] = {
		.Name = "UTIL_2V5",
		.Part_Name = "IRPS5401",
		.Supported_Volt = { 2.5, -1 },
		.Maximum_Volt = VOLT_MAX(2.5),
		.Typical_Volt = 2.5,
		.Minimum_Volt = VOLT_MIN(2.5),
		.I2C_Bus = "/dev/i2c-3",
		.I2C_Address = 0x4D,
		.Page_Select = 1,
	},
	.Voltage[VOLTAGE_VCC1V2_DDR4] = {
		.Name = "VCC1V2_DDR4",
		.Part_Name = "IRPS5401",
		.Supported_Volt = { 1.2, -1 },
		.Maximum_Volt = VOLT_MAX(1.2),
		.Typical_Volt = 1.2,
		.Minimum_Volt = VOLT_MIN(1.2),
		.I2C_Bus = "/dev/i2c-3",
		.I2C_Address = 0x4D,
		.Page_Select = 2,
	},
	.Voltage[VOLTAGE_VCC1V1_LP4] = {
		.Name = "VCC1V1_LP4",
		.Part_Name = "IRPS5401",
		.Supported_Volt = { 1.1, -1 },
		.Maximum_Volt = VOLT_MAX(1.1),
		.Typical_Volt = 1.1,
		.Minimum_Volt = VOLT_MIN(1.1),
		.I2C_Bus = "/dev/i2c-3",
		.I2C_Address = 0x4D,
		.Page_Select = 3,
	},
};

/*
 * According to UG1366 "VCK190 Evaluation Board User Guide"
 * DDR4_DIMM1 is connected to Channel 3 of the i2c mux at address 0x74
 * on the I2C1 bus. The system controller running 5.4.0-xilinx-v2020.1
 * linux maps it to i2c-14 (i2cdetect -l):
 *      i2c-14  i2c     i2c-2-mux (chan_id 3)
 *
 *  addr=0x50, reg_addr=0 len=16:   DIMM's SPD EEPROM
 *  addr=0x18, reg_addr=5 len=2:    DIMM's temp sensor
 */
DIMM_t VCK190_DIMM = {
	.Name = "DIMM1",
	.I2C_Bus = "/dev/i2c-14",
	.Spd   = { .Bus_addr = 0x50, .Reg_addr = 0, .Read_len = 16 },
	.Therm = { .Bus_addr = 0x18, .Reg_addr = 5, .Read_len = 2 }
};

GPIOs_t VCK190_GPIOs = {
	.Numbers = 37,
	.GPIO[0] = { 7, "500 - DC_SYS_CTRL0", "DC_SYS_CTRL0" },
	.GPIO[1] = { 8, "500 - DC_SYS_CTRL1", "DC_SYS_CTRL1" },
	.GPIO[2] = { 9, "500 - DC_SYS_CTRL2", "DC_SYS_CTRL2" },
	.GPIO[3] = { 10, "500 - DC_SYS_CTRL3", "DC_SYS_CTRL3" },
	.GPIO[4] = { 12, "500 - SYSCTLR_PB", "SYSCTLR_PB" },
	.GPIO[5] = { 34, "501 - LP_I2C0_PMC_SCL", "LP_I2C0_PMC_SCL" },
	.GPIO[6] = { 35, "501 - LP_I2C0_PMC_SDA", "LP_I2C0_PMC_SDA" },
	.GPIO[7] = { 36, "501 - LP_I2C1_SCL", "LP_I2C1_SCL" },
	.GPIO[8] = { 37, "501 - LP_I2C1_SDA", "LP_I2C1_SDA" },
	.GPIO[9] = { 38, "501 - SYSCTLR_UART0_RXD_IN", "UART0_RXD_IN" },
	.GPIO[10] = { 39, "501 - SYSCTLR_UART0_TXD_OUT", "UART0_TXD_OUT" },
	.GPIO[11] = { 42, "501 - SYSCTLR_ETH_RESET_B_R", "ETH_RESET_B" },
	.GPIO[12] = { 45, "501 - SYSCTLR_SD1_CD_B", "SD1_CD_B" },
	.GPIO[13] = { 46, "501 - SYSCTLR_SD1_DATA0", "SD1_DATA0" },
	.GPIO[14] = { 47, "501 - SYSCTLR_SD1_DATA1", "SD1_DATA1" },
	.GPIO[15] = { 48, "501 - SYSCTLR_SD1_DATA2", "SD1_DATA2" },
	.GPIO[16] = { 49, "501 - SYSCTLR_SD1_DATA3", "SD1_DATA3" },
	.GPIO[17] = { 50, "501 - SYSCTLR_SD1_CMD", "SD1_CMD" },
	.GPIO[18] = { 51, "501 - SYSCTLR_SD1_CLK", "SD1_CLK" },
	.GPIO[19] = { 76, "502 - SYSCTLR_ETH_MDC", "ETH_MDC" },
	.GPIO[20] = { 77, "502 - SYSCTLR_ETH_MDIO", "ETH_MDIO" },
	.GPIO[21] = { 90, "43 - SYSCTLR_GPIO0", "SYSCTLR_GPIO0" },
	.GPIO[22] = { 91, "43 - SYSCTLR_GPIO1", "SYSCTLR_GPIO1" },
	.GPIO[23] = { 92, "43 - SYSCTLR_GPIO2", "SYSCTLR_GPIO2" },
	.GPIO[24] = { 93, "43 - SYSCTLR_GPIO3", "SYSCTLR_GPIO3" },
	.GPIO[25] = { 94, "43 - SYSCTLR_GPIO4", "SYSCTLR_GPIO4" },
	.GPIO[26] = { 95, "43 - SYSCTLR_GPIO5", "SYSCTLR_GPIO5" },
	.GPIO[27] = { 78, "44 - SYSCTLR_VERSAL_MODE0", "SYSCTLR_VERSAL_MODE0" },
	.GPIO[28] = { 79, "44 - SYSCTLR_VERSAL_MODE1", "SYSCTLR_VERSAL_MODE1" },
	.GPIO[29] = { 80, "44 - SYSCTLR_VERSAL_MODE2", "SYSCTLR_VERSAL_MODE2" },
	.GPIO[30] = { 81, "44 - SYSCTLR_VERSAL_MODE3", "SYSCTLR_VERSAL_MODE3" },
	.GPIO[31] = { 82, "44 - SYSCTLR_POR_B_LS", "SYSCTLR_POR_B_LS" },
	.GPIO[32] = { 83, "44 - DC_PRSNT", "DC_PRSNT" },
	.GPIO[33] = { 85, "44 - SYSCTLR_JTAG_S0", "SYSCTLR_JTAG_S0" },
	.GPIO[34] = { 86, "44 - SYSCTLR_JTAG_S1", "SYSCTLR_JTAG_S1" },
	.GPIO[35] = { 87, "44 - SYSCTLR_IIC_MUX0_RESET_B", "SYSCTLR_IIC_MUX0_RESET_B" },
	.GPIO[36] = { 88, "44 - SYSCTLR_IIC_MUX1_RESET_B", "SYSCTLR_IIC_MUX1_RESET_B" },
};

/*
 * IO Expander
 */
IO_Exp_t VCK190_IO_Exp = {
	.Name = "TCA6416A",
	.Numbers = 16,
	.Labels = { "Port 0(7) - MAX6643 FULLSPD",
		    "Port 0(6) - N.C.",
		    "Port 0(5) - N.C.",
		    "Port 0(4) - PMBUS2 INA226 ALERT",
		    "Port 0(3) - N.C.",
		    "Port 0(2) - N.C.",
		    "Port 0(1) - MAX6643 Fan Fail (AL)",
		    "Port 0(0) - MAX6643 Fan Over Temp (AL)",
		    "Port 1(7) - PMBUS1 INA226 Alert (AL)",
		    "Port 1(6) - PMBUS Alert (AL)",
		    "Port 1(5) - 8A34001_EXP_RST_B (AL)",
		    "Port 1(4) - VCCINT VRHOT B (AL)",
		    "Port 1(3) - FMCP2 FMCP PRSNT M2C B (AL)",
		    "Port 1(2) - FMCP1 FMCP PRSNT M2C B (AL)",
		    "Port 1(1) - FMCP2 FMC PRSNT M2C B (AL)",
		    "Port 1(0) - FMCP1 FMC PRSNT M2C B (AL)" },
	.Directions = { 1, -1, -1, 1, -1, -1, 1, 1,
			1, 1, 0, 1, 1, 1, 1, 1 },
	.I2C_Bus = "/dev/i2c-0",
	.I2C_Address = 0x20,
};

/*
 * On-board EEPROM
 */
OnBoard_EEPROM_t VCK190_OnBoard_EEPROM = {
	.Name = "onboard",
	.I2C_Bus = "/dev/i2c-11",
	.I2C_Address = 0x54,
};

/*
 * Daughter Card
 */
Daughter_Card_t VCK190_Daughter_Card = {
	.Name = "X-EBM",
	.I2C_Bus = "/dev/i2c-11",
	.I2C_Address = 0x52,
};

/*
 * SFP Transceivers
 */
typedef enum {
	SFP_0,
	SFP_1,
	SFP_MAX,
} SFP_Index;

SFPs_t VCK190_SFPs = {
	.Numbers = SFP_MAX,
	.SFP[SFP_0] = {
		.Name = "zSFP-0",
		.I2C_Bus = "/dev/i2c-19",
		.I2C_Address = 0x50,
	},
	.SFP[SFP_1] = {
		.Name = "zSFP-1",
		.I2C_Bus = "/dev/i2c-20",
		.I2C_Address = 0x50,
	},
};

/*
 * QSFP Transceivers
 */
typedef enum {
	QSFP_0,
	QSFP_MAX,
} QSFP_Index;

QSFPs_t VCK190_QSFPs = {
	.Numbers = QSFP_MAX,
	.QSFP[QSFP_0] = {
		.Name = "zQSFP1",
		.Type = qsfp,
		.I2C_Bus = "/dev/i2c-21",
		.I2C_Address = 0x50,
	},
};

/*
 * FMC Cards
 */
typedef enum {
	FMC_1,
	FMC_2,
	FMC_MAX,
} FMC_Index;

FMCs_t VCK190_FMCs = {
	.Numbers = FMC_MAX,
	.FMC[FMC_1] = {
		.Name = "FMC1",
		.I2C_Bus = "/dev/i2c-12",
		.I2C_Address = 0x50,
	},
	.FMC[FMC_2] = {
		.Name = "FMC2",
		.I2C_Bus = "/dev/i2c-13",
		.I2C_Address = 0x50,
	},
};

/*
 * Workarounds
 */
typedef enum {
	WORKAROUND_VCCAUX,
	WORKAROUND_MAX,
} Workaround_Index;

Workarounds_t VCK190_Workarounds = {
	.Numbers = WORKAROUND_MAX,
	.Workaround[WORKAROUND_VCCAUX] = {
		.Name = "vccaux",		// Name of workaround for this platform
		.Arg_Needed = 1,		// Whether following workaround routine needs argument
		.Plat_Workaround_Op = Workaround_Vccaux, // Platform workaround routine
	},
};

/*
 * Board Interface Tests
 */
typedef enum {
	BIT_CLOCKS_CHECK,
	BIT_IDCODE_CHECK,
	BIT_EFUSE_CHECK,
	BIT_EBM_EEPROM_CHECK,
	BIT_DIMM_EEPROM_CHECK,
	BIT_VOLTAGES_CHECK,
	BIT_PL_UART_TEST,
	BIT_MAX,
} BIT_Index;

BITs_t VCK190_BITs = {
	.Numbers = BIT_MAX,
	.BIT[BIT_CLOCKS_CHECK] = {
		.Name = "Check Clocks",		// Name of BIT to run
		.Plat_BIT_Op = Clocks_Check,	// BIT routine to invoke
	},
	.BIT[BIT_IDCODE_CHECK] = {
		.Name = "IDCODE Check",
		.TCL_File = "idcode/idcode_check.tcl",
		.Plat_BIT_Op = XSDB_BIT,
	},
	.BIT[BIT_EFUSE_CHECK] = {
		.Name = "EFUSE Check",
		.TCL_File = "efuse/read_efuse.tcl",
		.Plat_BIT_Op = XSDB_BIT,
	},
	.BIT[BIT_EBM_EEPROM_CHECK] = {
		.Name = "X-EBM EEPROM Check",
		.Plat_BIT_Op = EBM_EEPROM_Check,
	},
	.BIT[BIT_DIMM_EEPROM_CHECK] = {
		.Name = "DIMM EEPROM Check",
		.Plat_BIT_Op = DIMM_EEPROM_Check,
	},
	.BIT[BIT_VOLTAGES_CHECK] = {
		.Name = "Check Voltages",
		.Plat_BIT_Op = Voltages_Check,
	},
	.BIT[BIT_PL_UART_TEST] = {
		.Name = "PL UART Test",
		.Manual = 1,
		.Instruction = "\n" \
			"1- Connect to the 2nd com port (PL console), baud rate 115200\n" \
			"2- The following output should be displayed on the console:\n\n" \
			"	Testing UART\n" \
			"	115200,8,N,1\n" \
			"	Hello world!\n" \
			"	UART 02 Test Passed\n",
		.TCL_File = "pl_uart/pl_uart_download.tcl",
		.Plat_BIT_Op = XSDB_BIT,
	},
};

/*
 * VCK190-sepcific Devices
 */
Plat_Devs_t VCK190_Devs = {
	.FeatureList = &VCK190_FeatureList,
	.BootModes = &VCK190_BootModes,
	.Clocks = &VCK190_Clocks,
	.Ina226s = &VCK190_Ina226s,
	.Power_Domains = &VCK190_Power_Domains,
	.Voltages = &VCK190_Voltages,
	.DIMM = &VCK190_DIMM,
	.GPIOs = &VCK190_GPIOs,
	.IO_Exp = &VCK190_IO_Exp,
	.OnBoard_EEPROM = &VCK190_OnBoard_EEPROM,
	.Daughter_Card = &VCK190_Daughter_Card,
	.SFPs = &VCK190_SFPs,
	.QSFPs = &VCK190_QSFPs,
	.FMCs = &VCK190_FMCs,
	.Workarounds = &VCK190_Workarounds,
	.BITs = &VCK190_BITs,
};

int
Workaround_Vccaux(void *Arg)
{
	int fd;
	int *State;
	char WriteBuffer[10];

	State = (int *)Arg;

	fd = open("/dev/i2c-3", O_RDWR);
	if (fd < 0) {
		SC_ERR("cannot open the I2C device %s: %m", "/dev/i2c-3");
		return -1;
	}

	// Select IRPS5401
	if (ioctl(fd, I2C_SLAVE_FORCE, 0x47) < 0) {
		SC_ERR("unable to access IRPS5401 address");
		(void) close(fd);
		return -1;
	}

	// Set the page for the IRPS5401
	WriteBuffer[0] = 0x00;
	WriteBuffer[1] = 0x03;
	if (write(fd, WriteBuffer, 2) != 2) {
		SC_ERR("unable to select page for IRPS5401");
		(void) close(fd);
		return -1;
	}

	// Change the state of VOUT (ON: State = 1, OFF: State = 0)
	WriteBuffer[0] = 0x01;
	WriteBuffer[1] = (1 == *State) ? 0x80 : 0x00;
	if (write(fd, WriteBuffer, 2) != 2) {
		SC_ERR("unable to change VOUT for IRPS5401");
		(void) close(fd);
		return -1;
	}

	(void) close(fd);
	return 0;
}

int
VCK190_BootMode_Op(BootMode_t *BootMode, int Op)
{
	FILE *FP;
	char Buffer[STRLEN_MAX];

	/* Only set boot mode is supported */
	if (Op != 1) {
		SC_ERR("invalid boot mode operation");
		return -1;
	}

	/* Record the boot mode */
	FP = fopen(BOOTMODEFILE, "w");
	if (FP == NULL) {
		SC_ERR("failed to open boot_mode file %s: %m", BOOTMODEFILE);
		return -1;
	}

	(void) sprintf(Buffer, "%s\n", BootMode->Name);
	SC_INFO("Boot Mode: %s", Buffer);
	(void) fputs(Buffer, FP);
	(void) fclose(FP);

	return 0;
}

int
VCK190_SetBootMode(int Value)
{
	FILE *FP;
	char Output[STRLEN_MAX] = { 0 };
	char System_Cmd[SYSCMD_MAX];

	(void) VCK190_JTAG_Op(1);
	sprintf(System_Cmd, "%s; %s %s%s %x 2>&1", XSDB_ENV, XSDB_CMD, BIT_PATH,
	    BOOTMODE_TCL, Value);
	SC_INFO("Command: %s", System_Cmd);
	FP = popen(System_Cmd, "r");
	if (FP == NULL) {
		SC_ERR("failed to invoke xsdb: %m");
		return -1;
	}

	(void) fgets(Output, sizeof(Output), FP);
	(void) pclose(FP);
	SC_INFO("XSDB Output: %s", Output);
	(void) VCK190_JTAG_Op(0);
	if (strstr(Output, "no targets found") != NULL) {
		SC_ERR("could not connect to Versal through jtag");
		return -1;
	}

	return 0;
}

int
VCK190_Reset_Op(void)
{
	FILE *FP;
	int State;
	char Buffer[SYSCMD_MAX] = { 0 };
	BootModes_t *BootModes;
	BootMode_t *BootMode;

	if (access(SILICONFILE, F_OK) == 0) {
		FP = fopen(SILICONFILE, "r");
		if (FP == NULL) {
			SC_ERR("failed to read silicon file %s: %m", SILICONFILE);
			return -1;
		}

		(void) fgets(Buffer, SYSCMD_MAX, FP);
		(void) fclose(FP);
		SC_INFO("%s: %s", SILICONFILE, Buffer);
		if (strcmp(Buffer, "ES1\n") == 0) {
			// Turn VCCINT_RAM off
			State = 0;
			if (Workaround_Vccaux(&State) != 0) {
				SC_ERR("failed to turn VCCINT_RAM off");
				return -1;
			}
		}
	}

	/* Assert POR */
	if (GPIO_Set("SYSCTLR_POR_B_LS", 0) != 0) {
		SC_ERR("failed to assert power-on-reset");
		return -1;
	}

	sleep(1);

	/* De-assert POR */
	if (GPIO_Set("SYSCTLR_POR_B_LS", 1) != 0) {
		SC_ERR("failed to de-assert power-on-reset");
		return -1;
	}

	/* If a boot mode is defined, set it after POR */
	if (access(BOOTMODEFILE, F_OK) == 0) {
		FP = fopen(BOOTMODEFILE, "r");
		if (FP == NULL) {
			SC_ERR("failed to read boot_mode file %s: %m", BOOTMODEFILE);
			return -1;
		}

		(void) fscanf(FP, "%s", Buffer);
		(void) fclose(FP);
		SC_INFO("%s: %s", BOOTMODEFILE, Buffer);
		BootModes = Plat_Devs->BootModes;
		for (int i = 0; i < BootModes->Numbers; i++) {
			BootMode = &BootModes->BootMode[i];
			if (strcmp(Buffer, (char *)BootMode->Name) == 0) {
				if (VCK190_SetBootMode(BootMode->Value) != 0) {
					SC_ERR("failed to set the boot mode");
					return -1;
				}

				break;
			}
		}
	}

	return 0;
}

int
VCK190_JTAG_Op(int Select)
{
	int State;

	switch (Select) {
	case 1:
		/* Overwrite the default JTAG switch */
		if (GPIO_Set("SYSCTLR_JTAG_S0", 0) != 0) {
			SC_ERR("failed to set JTAG 0");
			return -1;
		}

		if (GPIO_Set("SYSCTLR_JTAG_S1", 0) != 0) {
			SC_ERR("failed to set JTAG 1");
			return -1;
		}

		break;
	case 0:
		/*
		 * Reading the gpio lines causes ZU4 to tri-state the select
		 * lines and that allows the switch to set back the default
		 * values.  The value of 'State' is ignored.
		 */
		if (GPIO_Get("SYSCTLR_JTAG_S0", &State) != 0) {
			SC_ERR("failed to release JTAG 0");
			return -1;
		}

		if (GPIO_Get("SYSCTLR_JTAG_S1", &State) != 0) {
			SC_ERR("failed to release JTAG 1");
			return -1;
		}

		break;
	default:
		SC_ERR("invalid JTAG select option");
		return -1;
	}

	return 0;
}

int
VCK190_XSDB_Op(const char *TCL_File, char *Output, int Length)
{
	FILE *FP;
	char System_Cmd[SYSCMD_MAX];
	int Ret = 0;

	(void) sprintf(System_Cmd, "%s%s", BIT_PATH, TCL_File);
	if (access(System_Cmd, F_OK) != 0) {
		SC_ERR("failed to access file %s: %m", System_Cmd);
		return -1;
	}

	if (Output == NULL) {
		SC_ERR("unallocated output buffer");
		return -1;
	}

	(void) VCK190_JTAG_Op(1);
	(void) sprintf(System_Cmd, "%s; %s %s%s 2>&1", XSDB_ENV, XSDB_CMD,
		       BIT_PATH, TCL_File);
	SC_INFO("Command: %s", System_Cmd);
	FP = popen(System_Cmd, "r");
	if (FP == NULL) {
		SC_ERR("failed to invoke xsdb");
		Ret = -1;
		goto Out;
	}

	(void) fgets(Output, Length, FP);
	(void) pclose(FP);
	SC_INFO("XSDB Output: %s", Output);
	if (strstr(Output, "no targets found") != NULL) {
		SC_ERR("could not connect to Versal through jtag");
		Ret = -1;
	}

Out:
	(void) VCK190_JTAG_Op(0);
	return Ret;
}

int
VCK190_IDCODE_Op(char *Output, int Length)
{
	return VCK190_XSDB_Op(IDCODE_TCL, Output, Length);
}

/*
 * Get the board temperature
 */
int
VCK190_Temperature_Op(void)
{
	FILE *FP;
	char Output[STRLEN_MAX];
	char Command[] = "/usr/bin/sensors ff0b0000ethernetffffffff00-mdio-0";
	double Temperature;

	SC_INFO("Command: %s", Command);
	FP = popen(Command, "r");
	if (FP == NULL) {
		SC_ERR("failed to execute sensors command %s: %m", Command);
		return -1;
	}

	/* Temperature is on the 3rd line */
	for (int i = 0; i < 3; i++) {
		(void) fgets(Output, sizeof(Output), FP);
	}

	pclose(FP);
	SC_INFO("Output: %s", Output);
	if (strstr(Output, "temp1:") == NULL) {
		SC_ERR("failed to get board temperature");
		return -1;
	}

	(void) strtok(Output, ":");
	(void) strcpy(Output, strtok(NULL, "C"));
	Temperature = atof(Output);
	SC_PRINT("Temperature(C):\t%.1f", Temperature);

	return 0;
}

/*
 * The QSFP must be selected and not held in Reset (High) in order to be
 * accessed.  These lines are driven by Vesal.  The QSFP1_RESETL_LS high
 * has a pull up, but QSFP1_MODSKLL_LS has no pull down.  If Versal is not
 * programmed to drive QSFP1_MODSKLL_LS low, the QSFP will not respond.
 */
int
VCK190_QSFP_ModuleSelect_Op(QSFP_t *Arg, int State)
{
	FILE *FP;
	char Output[STRLEN_MAX] = { 0 };
	char System_Cmd[SYSCMD_MAX];

	if (State != 0 && State != 1) {
		SC_ERR("invalid QSFP module select state");
		return -1;
	}

	/*
	 * Call with 'State == 1' may change the current boot mode to JTAG
	 * to download a PDI.  Calling VCK190_Reset_Op() restores the current
	 * boot mode.
	 */
	if (State == 0) {
		(void) VCK190_Reset_Op();
		return 0;
	}

	/* State == 1 */
	(void) VCK190_JTAG_Op(1);
	sprintf(System_Cmd, "%s; %s %s%s 2>&1", XSDB_ENV, XSDB_CMD, BIT_PATH,
	    QSFP_MODSEL_TCL);
	SC_INFO("Command: %s", System_Cmd);
	FP = popen(System_Cmd, "r");
	if (FP == NULL) {
		SC_ERR("failed to invoke xsdb: %m");
		return -1;
	}

	(void) fgets(Output, sizeof(Output), FP);
	(void) pclose(FP);
	SC_INFO("XSDB Output: %s", Output);
	(void) VCK190_JTAG_Op(0);
	if (strstr(Output, "no targets found") != NULL) {
		SC_ERR("could not connect to Versal through jtag");
		return -1;
	}

	return 0;
}

int
VCK190_FMCAutoVadj_Op(void)
{
	int Target_Index = -1;
	IO_Exp_t *IO_Exp;
	unsigned int Value;
	Voltages_t *Voltages;
	Voltage_t *Regulator;
	FMCs_t *FMCs;
	FMC_t *FMC;
	int FMC1 = 0;
	int FMC2 = 0;
	float Voltage = 0;
	float Min_Voltage_1 = 0;
	float Max_Voltage_1 = 0;
	float Min_Voltage_2 = 0;
	float Max_Voltage_2 = 0;
	float Min_Combined, Max_Combined;

	IO_Exp = Plat_Devs->IO_Exp;
	if (Access_IO_Exp(IO_Exp, 0, 0x0, &Value) != 0) {
		SC_ERR("failed to read input of IO Expander");
		return -1;
	}

	SC_INFO("IO Expander input: %#x", Value);

	/* If bit[0] is 0, then FMC1 is connected */
	if ((~Value & 0x1) == 0x1) {
		FMC1 = 1;
	}

	/* If bit[1] is 0, then FMC2 is connected */
	if ((~Value & 0x2) == 0x2) {
		FMC2 = 1;
	}

	Voltages = Plat_Devs->Voltages;
	for (int i = 0; i < Voltages->Numbers; i++) {
		if (strcmp("VADJ_FMC", (char *)Voltages->Voltage[i].Name) == 0) {
			Target_Index = i;
			Regulator = &Voltages->Voltage[Target_Index];
			break;
		}
	}

	if (Target_Index == -1) {
		SC_ERR("no regulator exists for VADJ_FMC");
		return -1;
	}

	FMCs = Plat_Devs->FMCs;
	if (FMC1 == 1) {
		FMC = &FMCs->FMC[FMC_1];
		if (FMC_Vadj_Range(FMC, &Min_Voltage_1, &Max_Voltage_1) != 0) {
			SC_ERR("failed to get Voltage Adjust range for FMC1");
			return -1;
		}
	}

	if (FMC2 == 1) {
		FMC = &FMCs->FMC[FMC_2];
		if (FMC_Vadj_Range(FMC, &Min_Voltage_2, &Max_Voltage_2) != 0) {
			SC_ERR("failed to get Voltage Adjust range for FMC2");
			return -1;
		}
	}

	if (FMC1 == 1 && FMC2 == 0) {
		Min_Combined = Min_Voltage_1;
		Max_Combined = Max_Voltage_1;
	} else if (FMC1 == 0 && FMC2 == 1) {
		Min_Combined = Min_Voltage_2;
		Max_Combined = Max_Voltage_2;
	} else if (FMC1 == 1 && FMC2 == 1) {
		Min_Combined = MAX(Min_Voltage_1, Min_Voltage_2);
		Max_Combined = MIN(Max_Voltage_1, Max_Voltage_2);
	} else {
		Min_Combined = 0;
		Max_Combined = 1.5;
	}

	SC_INFO("Combined Min: %.2f, Combined Max: %.2f",
		Min_Combined, Max_Combined);

	/*
	 * Start with satisfying the lower target voltage and then see
	 * if it does also satisfy the higher voltage.
	 */
	if (Min_Combined <= 1.2 && Max_Combined >= 1.2) {
		Voltage = 1.2;
	}

	if (Min_Combined <= 1.5 && Max_Combined >= 1.5) {
		Voltage = 1.5;
	}

	if (Access_Regulator(Regulator, &Voltage, 1) != 0) {
		SC_ERR("failed to set voltage of VADJ_FMC regulator");
		return -1;
	}

	return 0;
}

int
VCK190_IDT_8A34001_Reset(void)
{
	IO_Exp_t *IO_Exp;
	unsigned int Value;

	IO_Exp = Plat_Devs->IO_Exp;

	/*
	 * The '8A34001_EXP_RST_B' line is controlled by bit 5 of register
	 * offset 3.  The output register pair (offsets 2 & 3) are written
	 * at once.
	 */
	Value = 0x0;	// Assert reset - active low
	if (Access_IO_Exp(IO_Exp, 1, 0x2, &Value) != 0) {
		SC_ERR("failed to assert reset of 8A34001 chip");
		return -1;
	}

	sleep (1);

	Value = 0x20;	// De-assert reset
	if (Access_IO_Exp(IO_Exp, 1, 0x2, &Value) != 0) {
		SC_ERR("failed to de-assert reset of 8A34001 chip");
		return -1;
	}

	return 0;
}

/*
 * VCK190-specific Operations
*/
Plat_Ops_t VCK190_Ops = {
	.BootMode_Op = VCK190_BootMode_Op,
	.Reset_Op = VCK190_Reset_Op,
	.IDCODE_Op = VCK190_IDCODE_Op,
	.XSDB_Op = VCK190_XSDB_Op,
	.Temperature_Op = VCK190_Temperature_Op,
	.QSFP_ModuleSelect_Op = VCK190_QSFP_ModuleSelect_Op,
	.FMCAutoVadj_Op = VCK190_FMCAutoVadj_Op,
};
