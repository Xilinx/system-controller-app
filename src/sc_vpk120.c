/*
 * Copyright (c) 2021 Xilinx, Inc.  All rights reserved.
 *
 * SPDX-License-Identifier: MIT
 */

#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>
#include <string.h>
#include "sc_app.h"

extern Plat_Devs_t *Plat_Devs;
extern Plat_Ops_t *Plat_Ops;

int VPK120_IDT_8A34001_Reset(void);
extern int Access_Regulator(Voltage_t *, float *, int);
extern int Access_IO_Exp(IO_Exp_t *, int, int, unsigned int *);
extern int FMC_Vadj_Range(FMC_t *, float *, float *);
extern int GPIO_Get(char *, int *);
extern int GPIO_Set(char *, int);
extern int Clocks_Check(void *);
extern int XSDB_BIT(void *);
extern int Voltages_Check(void *);

/*
 * Feature List
 */
FeatureList_t VPK120_FeatureList = {
	.Numbers = 11,
	.Feature = { \
		"eeprom", "temp", "bootmode", "clock", "voltage", "power", \
		"BIT", "gpio", "ioexp", "QSFP", "FMC", \
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

BootModes_t VPK120_BootModes = {
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
IDT_8A34001_Data_t VPK120_IDT_8A34001_Data = {
	.Number_Label = 12,
	.Display_Label = { "Q0 - From 8A34001 Q0 to 8A34001 CLK0", \
			   "Q1 - From Bank 703 to Bank 206 GTM RX2", \
			   "Q2 - From Bank 206 GTM TX2 to Bank 703", \
			   "Q3 - From 8A34001 Q4 to SMA J339", \
			   "Q4 - From SMA J330-331 to 8A34001 CLK3", \
			   "Q5 - From Bank 202/204 GTM REFCLK1 to J328", \
			   "Q6 - From Bank 206 GTM REFCLK1 to Bank 711", \
			   "Q7 - From FMC REFCLK M2C to Bank 206 GTM REFCLK0", \
			   "Q8 - To Bank 204/205 GTM REFCLKP0", \
			   "Q9 - To Bank 202/203 GTM REFCLKP0", \
			   "Q10 - To SMA J328", \
			   "Q11 - To N.C.", \
	},
	.Internal_Label = { "OUT0DesiredFrequency", \
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
	.Chip_Reset = VPK120_IDT_8A34001_Reset,
};

typedef enum {
	SI570_USER1_FMC,
	IDT_8A34001_FMC,
	SI570_VERSAL_SYS,
	SI570_LPDDR4_CLK1,
	SI570_LPDDR4_CLK2,
	SI570_LPDDR4_CLK3,
	CLOCK_MAX,
} Clock_Index;

Clocks_t VPK120_Clocks = {
	.Numbers = CLOCK_MAX,
	.Clock[SI570_USER1_FMC] = {
		.Name = "User1 FMC Si570",// Name of the device referenced by application
		.Type = Si570,		// Clock generators accessible through sysfs
		.Sysfs_Path = "/sys/devices/platform/si570_user1_fmc_clk/set_rate",
		.Default_Freq = 100.0,	// Factory default frequency in MHz
		.Upper_Freq = 810.0,	// Upper-bound frequency in MHz
		.Lower_Freq = 10.0,	// Lower-bound frequency in MHz
		.I2C_Bus = "/dev/i2c-8",// I2C bus address behind the mux
		.I2C_Address = 0x5f,	// I2C device address
	},
	.Clock[IDT_8A34001_FMC] = {
		.Name = "8A34001 FMC",
		.Type = IDT_8A34001,
		.Type_Data = &VPK120_IDT_8A34001_Data,
		.I2C_Bus = "/dev/i2c-17",
		.I2C_Address = 0x5b,
	},
	.Clock[SI570_VERSAL_SYS] = {
		.Name = "Versal Sys Clk Si570",
		.Type = Si570,
		.Sysfs_Path = "/sys/devices/platform/si570_ref_clk/set_rate",
		.Default_Freq = 33.333,
		.Upper_Freq = 160.0,
		.Lower_Freq = 10.0,
		.I2C_Bus = "/dev/i2c-10",
		.I2C_Address = 0x5d,
	},
	.Clock[SI570_LPDDR4_CLK1] = {
		.Name = "LPDDR4 Clk1 Si570",
		.Type = Si570,
		.Sysfs_Path = "/sys/devices/platform/si570_lpddr4_clk1/set_rate",
		.Default_Freq = 200.0,
		.Upper_Freq = 810.0,
		.Lower_Freq = 10.0,
		.I2C_Bus = "/dev/i2c-15",
		.I2C_Address = 0x60,
	},
	.Clock[SI570_LPDDR4_CLK2] = {
		.Name = "LPDDR4 Clk2 Si570",
		.Type = Si570,
		.Sysfs_Path = "/sys/devices/platform/si570_lpddr4_clk2/set_rate",
		.Default_Freq = 200.0,
		.Upper_Freq = 810.0,
		.Lower_Freq = 10.0,
		.I2C_Bus = "/dev/i2c-14",
		.I2C_Address = 0x60,
	},
	.Clock[SI570_LPDDR4_CLK3] = {
		.Name = "LPDDR4 Clk3 Si570",
		.Type = Si570,
		.Sysfs_Path = "/sys/devices/platform/si570_lpddr4_clk3/set_rate",
		.Default_Freq = 200.0,
		.Upper_Freq = 810.0,
		.Lower_Freq = 10.0,
		.I2C_Bus = "/dev/i2c-13",
		.I2C_Address = 0x60,
	},
};

/*
 * INA226
 */
typedef enum {
	INA226_VCCINT,
	INA226_VCC_SOC,
	INA226_VCC_PSFP,
	INA226_VCC_PMC,
	INA226_VCC_RAM_VCCINT_GT,
	INA226_VCC_PSLP_CPM5,
	INA226_VCCO_MIO,
	INA226_VCCAUX,
	INA226_VCC1V5,
	INA226_LPDMGTYAVCC,
	INA226_MGTVCCAUX,
	INA226_MGTAVCC,
	INA226_VCCAUX_PMC,
	INA226_VCCO_502,
	INA226_VADJ_FMC,
	INA226_VCC1V1_LP4,
	INA226_LPDMGTYVCCAUX,
	INA226_LPDMGTYAVTT,
	INA226_MGTAVTT,
	INA226_MAX,
} Ina226_Index;

Ina226s_t VPK120_Ina226s = {
	.Numbers = INA226_MAX,
	.Ina226[INA226_VCCINT] = {
		.Name = "VCCINT",		// Name of the device referenced by application
		.I2C_Bus = "/dev/i2c-3",	// I2C bus of the device
		.I2C_Address = 0x40,		// I2C address of the device
		.Shunt_Resistor = 500,		// Value of shunt resistor in Micro-Ohm
		.Phase_Multiplier = 4,		// Sensor for # of phases
	},
	.Ina226[INA226_VCC_SOC] = {
		.Name = "VCC_SOC",
		.I2C_Bus = "/dev/i2c-3",
		.I2C_Address = 0x41,
		.Shunt_Resistor = 500,
		.Phase_Multiplier = 1,
	},
	.Ina226[INA226_VCC_PSFP] = {
		.Name = "VCCINT_PSFP",
		.I2C_Bus = "/dev/i2c-3",
		.I2C_Address = 0x45,
		.Shunt_Resistor = 5000,
		.Phase_Multiplier = 1,
	},
	.Ina226[INA226_VCC_PMC] = {
		.Name = "VCC_PMC",
		.I2C_Bus = "/dev/i2c-3",
		.I2C_Address = 0x42,
		.Shunt_Resistor = 5000,
		.Phase_Multiplier = 1,
	},
	.Ina226[INA226_VCC_RAM_VCCINT_GT] = {
		.Name = "VCC_RAM_VCCINT_GT",
		.I2C_Bus = "/dev/i2c-3",
		.I2C_Address = 0x43,
		.Shunt_Resistor = 2000,
		.Phase_Multiplier = 1,
	},
	.Ina226[INA226_VCC_PSLP_CPM5] = {
		.Name = "VCC_PSLP_CPM5",
		.I2C_Bus = "/dev/i2c-3",
		.I2C_Address = 0x44,
		.Shunt_Resistor = 5000,
		.Phase_Multiplier = 1,
	},
	.Ina226[INA226_VCCO_MIO] = {
		.Name = "VCCO_MIO",
		.I2C_Bus = "/dev/i2c-5",
		.I2C_Address = 0x45,
		.Shunt_Resistor = 5000,
		.Phase_Multiplier = 1,
	},
	.Ina226[INA226_VCCAUX] = {
		.Name = "VCCAUX",
		.I2C_Bus = "/dev/i2c-5",
		.I2C_Address = 0x40,
		.Shunt_Resistor = 5000,
		.Phase_Multiplier = 1,
	},
	.Ina226[INA226_VCC1V5] = {
		.Name = "VCC1V5",
		.I2C_Bus = "/dev/i2c-5",
		.I2C_Address = 0x43,
		.Shunt_Resistor = 5000,
		.Phase_Multiplier = 1,
	},
	.Ina226[INA226_LPDMGTYAVCC] = {
		.Name = "LPDMGTYAVCC",
		.I2C_Bus = "/dev/i2c-5",
		.I2C_Address = 0x4B,
		.Shunt_Resistor = 5000,
		.Phase_Multiplier = 1,
	},
	.Ina226[INA226_MGTVCCAUX] = {
		.Name = "MGTVCCAUX",
		.I2C_Bus = "/dev/i2c-5",
		.I2C_Address = 0x48,
		.Shunt_Resistor = 5000,
		.Phase_Multiplier = 1,
	},
	.Ina226[INA226_MGTAVCC] = {
		.Name = "MGTAVCC",
		.I2C_Bus = "/dev/i2c-5",
		.I2C_Address = 0x42,
		.Shunt_Resistor = 5000,
		.Phase_Multiplier = 1,
	},
	.Ina226[INA226_VCCAUX_PMC] = {
		.Name = "VCCAUX_PMC",
		.I2C_Bus = "/dev/i2c-5",
		.I2C_Address = 0x41,
		.Shunt_Resistor = 5000,
		.Phase_Multiplier = 1,
	},
	.Ina226[INA226_VCCO_502] = {
		.Name = "VCCO_502",
		.I2C_Bus = "/dev/i2c-5",
		.I2C_Address = 0x47,
		.Shunt_Resistor = 5000,
		.Phase_Multiplier = 1,
	},
	.Ina226[INA226_VADJ_FMC] = {
		.Name = "VADJ_FMC",
		.I2C_Bus = "/dev/i2c-5",
		.I2C_Address = 0x4A,
		.Shunt_Resistor = 2000,
		.Phase_Multiplier = 1,
	},
	.Ina226[INA226_VCC1V1_LP4] = {
		.Name = "VCC1V1_LP4",
		.I2C_Bus = "/dev/i2c-5",
		.I2C_Address = 0x49,
		.Shunt_Resistor = 2000,
		.Phase_Multiplier = 1,
	},
	.Ina226[INA226_LPDMGTYVCCAUX] = {
		.Name = "LPDMGTYVCCAUX",
		.I2C_Bus = "/dev/i2c-5",
		.I2C_Address = 0x4D,
		.Shunt_Resistor = 5000,
		.Phase_Multiplier = 1,
	},
	.Ina226[INA226_LPDMGTYAVTT] = {
		.Name = "LPDMGTYAVTT",
		.I2C_Bus = "/dev/i2c-5",
		.I2C_Address = 0x4C,
		.Shunt_Resistor = 2000,
		.Phase_Multiplier = 1,
	},
	.Ina226[INA226_MGTAVTT] = {
		.Name = "MGTAVTT",
		.I2C_Bus = "/dev/i2c-5",
		.I2C_Address = 0x46,
		.Shunt_Resistor = 2000,
		.Phase_Multiplier = 1,
	},
};

/*
 * Voltages - power rails
 */
typedef enum {
	VOLTAGE_VCCINT,
	VOLTAGE_VCC_SOC,
	VOLTAGE_VCC_PSFP,
	VOLTAGE_VCCO_MIO,
	VOLTAGE_VCCAUX,
	VOLTAGE_VCC1V5,
	VOLTAGE_LPDMGTYAVCC,
	VOLTAGE_MGTVCCAUX,
	VOLTAGE_MGTAVCC,
	VOLTAGE_VCCO_502,
	VOLTAGE_UTIL_2V5,
	VOLTAGE_VCC_PSLP_CPM5,
	VOLTAGE_VCC_RAM_VCCINT_GT,
	VOLTAGE_VADJ_FMC,
	VOLTAGE_LPDMGTYAVTT,
	VOLTAGE_VCC1V1_LP4,
	VOLTAGE_MGTAVTT,
	VOLTAGE_MAX,
} Voltage_Index;

Voltages_t VPK120_Voltages = {
	.Numbers = VOLTAGE_MAX,
	.Voltage[VOLTAGE_VCCINT] = {
		.Name = "VCCINT",               // Name of the device referenced by application
		.Part_Name = "IR35215",         // Name of the part that is used
		.Supported_Volt = { 0.7, 0.78, 0.8, -1 }, // List of supported voltages (volt)
		.Maximum_Volt = VOLT_MAX(0.8),  // Maximum voltage (volt)
		.Typical_Volt = 0.8,            // Typical voltage (volt)
		.Minimum_Volt = VOLT_MIN(0.8),  // Minimum voltage (volt)
		.I2C_Bus = "/dev/i2c-2",        // I2C bus of the device
		.I2C_Address = 0x46,            // I2C address of the device
		.Page_Select = 0,               // Device page selection
	},
	.Voltage[VOLTAGE_VCC_SOC] = {
		.Name = "VCC_SOC",
		.Part_Name = "IR35215",
		.Supported_Volt = { 0.8, -1 },
		.Maximum_Volt = VOLT_MAX(0.8),
		.Typical_Volt = 0.8,
		.Minimum_Volt = VOLT_MIN(0.8),
		.I2C_Bus = "/dev/i2c-2",
		.I2C_Address = 0x46,
		.Page_Select = 1,
	},
	.Voltage[VOLTAGE_VCC_PSFP] = {
		.Name = "VCC_PSFP",
		.Part_Name = "IRPS5401",
		.Supported_Volt = { 0.7, 0.8, -1 },
		.Maximum_Volt = VOLT_MAX(0.8),
		.Typical_Volt = 0.8,
		.Minimum_Volt = VOLT_MIN(0.8),
		.I2C_Bus = "/dev/i2c-2",
		.I2C_Address = 0x47,
		.Page_Select = 0,
	},
	.Voltage[VOLTAGE_VCCO_MIO] = {
		.Name = "VCCO_MIO",
		.Part_Name = "IRPS5401",
		.Supported_Volt = { 1.8, -1 },
		.Maximum_Volt = VOLT_MAX(1.8),
		.Typical_Volt = 1.8,
		.Minimum_Volt = VOLT_MIN(1.8),
		.I2C_Bus = "/dev/i2c-2",
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
		.I2C_Bus = "/dev/i2c-2",
		.I2C_Address = 0x47,
		.Page_Select = 2,
	},
	.Voltage[VOLTAGE_VCC1V5] = {
		.Name = "VCC1V5",
		.Part_Name = "IRPS5401",
		.Supported_Volt = { 1.5, -1 },
		.Maximum_Volt = VOLT_MAX(1.5),
		.Typical_Volt = 1.5,
		.Minimum_Volt = VOLT_MIN(1.5),
		.I2C_Bus = "/dev/i2c-2",
		.I2C_Address = 0x47,
		.Page_Select = 3,
	},
	.Voltage[VOLTAGE_LPDMGTYAVCC] = {
		.Name = "LPDMGTYAVCC",
		.Part_Name = "IRPS5401",
		.Supported_Volt = { 0.88, -1 },
		.Maximum_Volt = VOLT_MAX(0.88),
		.Typical_Volt = 0.88,
		.Minimum_Volt = VOLT_MIN(0.88),
		.I2C_Bus = "/dev/i2c-2",
		.I2C_Address = 0x4C,
		.Page_Select = 0,
	},
	.Voltage[VOLTAGE_MGTVCCAUX] = {
		.Name = "MGTVCCAUX",
		.Part_Name = "IRPS5401",
		.Supported_Volt = { 1.5, -1 },
		.Maximum_Volt = VOLT_MAX(1.5),
		.Typical_Volt = 1.5,
		.Minimum_Volt = VOLT_MIN(1.5),
		.I2C_Bus = "/dev/i2c-2",
		.I2C_Address = 0x4C,
		.Page_Select = 1,
	},
	.Voltage[VOLTAGE_MGTAVCC] = {
		.Name = "MGTAVCC",
		.Part_Name = "IRPS5401",
		.Supported_Volt = { 0.88, -1 },
		.Maximum_Volt = VOLT_MAX(0.88),
		.Typical_Volt = 0.88,
		.Minimum_Volt = VOLT_MIN(0.88),
		.I2C_Bus = "/dev/i2c-2",
		.I2C_Address = 0x4C,
		.Page_Select = 2,
	},
	.Voltage[VOLTAGE_VCCO_502] = {
		.Name = "VCCO_502",
		.Part_Name = "IRPS5401",
		.Supported_Volt = { 1.8, -1 },
		.Maximum_Volt = VOLT_MAX(1.8),
		.Typical_Volt = 1.8,
		.Minimum_Volt = VOLT_MIN(1.8),
		.I2C_Bus = "/dev/i2c-2",
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
		.I2C_Bus = "/dev/i2c-2",
		.I2C_Address = 0x4D,
		.Page_Select = 1,
	},
	.Voltage[VOLTAGE_VCC_PSLP_CPM5] = {
		.Name = "VCC_PSLP_CPM5",
		.Part_Name = "IRPS5401",
		.Supported_Volt = { 0.7, 0.8, -1 },
		.Maximum_Volt = VOLT_MAX(0.8),
		.Typical_Volt = 0.8,
		.Minimum_Volt = VOLT_MIN(0.8),
		.I2C_Bus = "/dev/i2c-2",
		.I2C_Address = 0x4D,
		.Page_Select = 2,
	},
	.Voltage[VOLTAGE_VCC_RAM_VCCINT_GT] = {
		.Name = "VCC_RAM_VCCINT_GT",
		.Part_Name = "IR38164",
		.Supported_Volt = { 0.8, -1 },
		.Maximum_Volt = VOLT_MAX(0.8),
		.Typical_Volt = 0.8,
		.Minimum_Volt = VOLT_MIN(0.8),
		.I2C_Bus = "/dev/i2c-2",
		.I2C_Address = 0x43,
		.Page_Select = -1,
	},
	.Voltage[VOLTAGE_VADJ_FMC] = {
		.Name = "VADJ_FMC",
		.Part_Name = "IR38164",
		.Supported_Volt = { 0.0, 1.2, 1.5, -1 },
		.Maximum_Volt = VOLT_MAX(1.5),
		.Typical_Volt = 1.5,
		.Minimum_Volt = 0.0,
		.I2C_Bus = "/dev/i2c-2",
		.I2C_Address = 0x4E,
		.Page_Select = -1,
	},
	.Voltage[VOLTAGE_LPDMGTYAVTT] = {
		.Name = "LPDMGTYAVTT",
		.Part_Name = "IR38164",
		.Supported_Volt = { 1.2, -1 },
		.Maximum_Volt = VOLT_MAX(1.2),
		.Typical_Volt = 1.2,
		.Minimum_Volt = VOLT_MIN(1.2),
		.I2C_Bus = "/dev/i2c-2",
		.I2C_Address = 0x41,
		.Page_Select = -1,
	},
	.Voltage[VOLTAGE_VCC1V1_LP4] = {
		.Name = "VCC1V1_LP4",
		.Part_Name = "IR38164",
		.Supported_Volt = { 1.1, -1 },
		.Maximum_Volt = VOLT_MAX(1.1),
		.Typical_Volt = 1.1,
		.Minimum_Volt = VOLT_MIN(1.1),
		.I2C_Bus = "/dev/i2c-2",
		.I2C_Address = 0x4F,
		.Page_Select = -1,
	},
	.Voltage[VOLTAGE_MGTAVTT] = {
		.Name = "MGTAVTT",
		.Part_Name = "IR38164",
		.Supported_Volt = { 1.2, -1 },
		.Maximum_Volt = VOLT_MAX(1.2),
		.Typical_Volt = 1.2,
		.Minimum_Volt = VOLT_MIN(1.2),
		.I2C_Bus = "/dev/i2c-2",
		.I2C_Address = 0x49,
		.Page_Select = -1,
	},
};

GPIOs_t VPK120_GPIOs = {
	.Numbers = 57,
	.GPIO[0] = { 0, "500 - SYSCTLR_QSPI_CLK", "QSPI_CLK" },
	.GPIO[1] = { 1, "500 - SYSCTLR_QSPI_DQ1", "QSPI_DQ1" },
	.GPIO[2] = { 2, "500 - SYSCTLR_QSPI_DQ2", "QSPI_DQ2" },
	.GPIO[3] = { 3, "500 - SYSCTLR_QSPI_DQ3", "QSPI_DQ3" },
	.GPIO[4] = { 4, "500 - SYSCTLR_QSPI_DQ0", "QSPI_DQ0" },
	.GPIO[5] = { 5, "500 - SYSCTLR_QSPI_CS_B", "QSPI_CS_B" },
	.GPIO[6] = { 8, "500 - SYSCTLR_GPIO", "SYSCTLR_GPIO" },
	.GPIO[7] = { 9, "500 - SYSCTLR_LED", "SYSCTLR_LED" },
	.GPIO[8] = { 10, "500 - SYSCTLR_PB", "SYSCTLR_PB" },
	.GPIO[9] = { 11, "500 - PMC_MIO37_ZU4_TRIGGER", "ZU4_TRIGGER" },
	.GPIO[10] = { 13, "500 - SYSCTLR_EMMC_DAT0", "EMMC_DAT0" },
	.GPIO[11] = { 14, "500 - SYSCTLR_EMMC_DAT1", "EMMC_DAT1" },
	.GPIO[12] = { 15, "500 - SYSCTLR_EMMC_DAT2", "EMMC_DAT2" },
	.GPIO[13] = { 16, "500 - SYSCTLR_EMMC_DAT3", "EMMC_DAT3" },
	.GPIO[14] = { 17, "500 - SYSCTLR_EMMC_DAT4", "EMMC_DAT4" },
	.GPIO[15] = { 18, "500 - SYSCTLR_EMMC_DAT5", "EMMC_DAT5" },
	.GPIO[16] = { 19, "500 - SYSCTLR_EMMC_DAT6", "EMMC_DAT6" },
	.GPIO[17] = { 20, "500 - SYSCTLR_EMMC_DAT7", "EMMC_DAT7" },
	.GPIO[18] = { 21, "500 - SYSCTLR_EMMC_CMD", "EMMC_CMD" },
	.GPIO[19] = { 22, "500 - SYSCTLR_EMMC_CLK", "EMMC_CLK" },
	.GPIO[20] = { 23, "500 - SYSCTLR_EMMC_RST_B", "EMMC_RST_B" },
	.GPIO[21] = { 34, "501 - LP_I2C0_PMC_SCL", "LP_I2C0_PMC_SCL" },
	.GPIO[22] = { 35, "501 - LP_I2C0_PMC_SDA", "LP_I2C0_PMC_SDA" },
	.GPIO[23] = { 36, "501 - LP_I2C1_SCL", "LP_I2C1_SCL" },
	.GPIO[24] = { 37, "501 - LP_I2C1_SDA", "LP_I2C1_SDA" },
	.GPIO[25] = { 38, "501 - SYSCTLR_UART0_RXD_IN", "UART0_RXD_IN" },
	.GPIO[26] = { 39, "501 - SYSCTLR_UART0_TXD_OUT", "UART0_TXD_OUT" },
	.GPIO[27] = { 42, "501 - SYSCTLR_ETH_RESET_B_R", "ETH_RESET_B" },
	.GPIO[28] = { 52, "502 - SYSCTLR_USB0_CLK", "USB0_CLK" },
	.GPIO[29] = { 53, "502 - SYSCTLR_USB0_DIR", "USB0_DIR" },
	.GPIO[30] = { 54, "502 - SYSCTLR_USB0_DATA2", "USB0_DATA2" },
	.GPIO[31] = { 55, "502 - SYSCTLR_USB0_NXT", "USB0_NXT" },
	.GPIO[32] = { 56, "502 - SYSCTLR_USB0_DATA0", "USB0_DATA0" },
	.GPIO[33] = { 57, "502 - SYSCTLR_USB0_DATA1", "USB0_DATA1" },
	.GPIO[34] = { 58, "502 - SYSCTLR_USB0_STP", "USB0_STP" },
	.GPIO[35] = { 59, "502 - SYSCTLR_USB0_DATA3", "USB0_DATA3" },
	.GPIO[36] = { 60, "502 - SYSCTLR_USB0_DATA4", "USB0_DATA4" },
	.GPIO[37] = { 61, "502 - SYSCTLR_USB0_DATA5", "USB0_DATA5" },
	.GPIO[38] = { 62, "502 - SYSCTLR_USB0_DATA6", "USB0_DATA6" },
	.GPIO[39] = { 63, "502 - SYSCTLR_USB0_DATA7", "USB0_DATA7" },
	.GPIO[40] = { 76, "502 - SYSCTLR_ETH_MDC", "ETH_MDC" },
	.GPIO[41] = { 77, "502 - SYSCTLR_ETH_MDIO", "ETH_MDIO" },
	.GPIO[42] = { 90, "43 - SYSCTLR_GPIO0", "SYSCTLR_GPIO0" },
	.GPIO[43] = { 91, "43 - SYSCTLR_GPIO1", "SYSCTLR_GPIO1" },
	.GPIO[44] = { 92, "43 - SYSCTLR_GPIO2", "SYSCTLR_GPIO2" },
	.GPIO[45] = { 93, "43 - SYSCTLR_GPIO3", "SYSCTLR_GPIO3" },
	.GPIO[46] = { 94, "43 - SYSCTLR_GPIO4", "SYSCTLR_GPIO4" },
	.GPIO[47] = { 95, "43 - SYSCTLR_GPIO5", "SYSCTLR_GPIO5" },
	.GPIO[48] = { 78, "44 - SYSCTLR_VERSAL_MODE0", "SYSCTLR_VERSAL_MODE0" },
	.GPIO[49] = { 79, "44 - SYSCTLR_VERSAL_MODE1", "SYSCTLR_VERSAL_MODE1" },
	.GPIO[50] = { 80, "44 - SYSCTLR_VERSAL_MODE2", "SYSCTLR_VERSAL_MODE2" },
	.GPIO[51] = { 81, "44 - SYSCTLR_VERSAL_MODE3", "SYSCTLR_VERSAL_MODE3" },
	.GPIO[52] = { 82, "44 - SYSCTLR_POR_B_LS", "SYSCTLR_POR_B_LS" },
	.GPIO[53] = { 85, "44 - SYSCTLR_JTAG_S0", "SYSCTLR_JTAG_S0" },
	.GPIO[54] = { 86, "44 - SYSCTLR_JTAG_S1", "SYSCTLR_JTAG_S1" },
	.GPIO[55] = { 87, "44 - SYSCTLR_IIC_MUX0_RESET_B", "SYSCTLR_IIC_MUX0_RESET_B" },
	.GPIO[56] = { 88, "44 - SYSCTLR_IIC_MUX1_RESET_B", "SYSCTLR_IIC_MUX1_RESET_B" },
};

/*
 * IO Expander
 */
IO_Exp_t VPK120_IO_Exp = {
	.Name = "TCA6416A",
	.Numbers = 16,
	.Labels = { "Port 0(7) - MAX6643 FULLSPD (AH)",
		    "Port 0(6) - N.C.",
		    "Port 0(5) - N.C.",
		    "Port 0(4) - PMBUS2 INA226 Alert (AL)",
		    "Port 0(3) - QSFPDD2_MODSELL (AL)",
		    "Port 0(2) - QSFPDD1_MODSELL (AL)",
		    "Port 0(1) - MAX6643 Fan Fail (AL)",
		    "Port 0(0) - MAX6643 Fan Over Temp (AL)",
		    "Port 1(7) - PMBUS1 INA226 Alert (AL)",
		    "Port 1(6) - PMBUS Alert (AL)",
		    "Port 1(5) - 8A34001_EXP_RST_B (AL)",
		    "Port 1(4) - VCCINT VRHOT B (AL)",
		    "Port 1(3) - N.C.",
		    "Port 1(2) - FMCP1 FMCP PRSNT M2C B (AL)",
		    "Port 1(1) - N.C.",
		    "Port 1(0) - FMCP1 FMC PRSNT M2C B (AL)" },
	.Directions = { 0, -1, -1, 1, 0, 0, 1, 1,
			1, 1, 0, 1, -1, 1, -1, 1 },
	.I2C_Bus = "/dev/i2c-0",
	.I2C_Address = 0x20,
};

OnBoard_EEPROM_t VPK120_OnBoard_EEPROM = {
	.Name = "onboard",
	.I2C_Bus = "/dev/i2c-10",
	.I2C_Address = 0x54,
};

/*
 * QSFP Transceivers
 */
typedef enum {
	QSFPDD_0,
	QSFPDD_1,
	QSFP_MAX,
} QSFP_Index;

QSFPs_t VPK120_QSFPs = {
	.Numbers = QSFP_MAX,
	.QSFP[QSFPDD_0] = {
		.Name = "QSFPDD1",
		.Type = qsfpdd,
		.I2C_Bus = "/dev/i2c-16",
		.I2C_Address = 0x50,
	},
	.QSFP[QSFPDD_1] = {
		.Name = "QSFPDD2",
		.Type = qsfpdd,
		.I2C_Bus = "/dev/i2c-16",
		.I2C_Address = 0x50,
	},
};

/*
 * FMC Cards
 */
typedef enum {
	FMC_1,
	FMC_MAX,
} FMC_Index;

FMCs_t VPK120_FMCs = {
	.Numbers = FMC_MAX,
	.FMC[FMC_1] = {
		.Name = "FMC",
		.I2C_Bus = "/dev/i2c-11",
		.I2C_Address = 0x50,
	},
};

/*
 * Board Interface Tests
 */
typedef enum {
	BIT_CLOCKS_CHECK,
	BIT_IDCODE_CHECK,
	BIT_EFUSE_CHECK,
	BIT_VOLTAGES_CHECK,
	BIT_RTC_CLOCK_TEST,
	BIT_PL_UART_TEST,
	BIT_PROGRAM_QSPI,
	BIT_MAX,
} BIT_Index;

BITs_t VPK120_BITs = {
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
	.BIT[BIT_VOLTAGES_CHECK] = {
		.Name = "Check Voltages",
		.Plat_BIT_Op = Voltages_Check,
	},
	.BIT[BIT_RTC_CLOCK_TEST] = {
		.Name = "RTC Clock Test",
		.Manual = 1,
		.Instruction = "\n" \
			"1- Connect to the 1st com port (Versal console), baud rate 115200\n" \
			"2- A similar output should be displayed on the console:\n\n" \
			"	Day Convention : 0-Fri, 1-Sat, 2-Sun, 3-Mon, 4-Tue, 5-Wed, 6-Thur\n" \
			"	Last set time for RTC is..\n" \
			"	YEAR:MM:DD HR:MM:SS 	 2016:10:17 00:00:01	 Day = 3\n\n" \
			"	Current RTC time is..\n" \
			"	YEAR:MM:DD HR:MM:SS 	 2016:10:17 00:08:39	 Day = 3\n\n" \
			"	Setting Time = 2016:10:17 00:00:00\n" \
			"	RTC time after set is..\n" \
			"	YEAR:MM:DD HR:MM:SS 	 2016:10:17 00:00:00	 Day = 3\n\n" \
			"	RTC time after delay_1 is..\n" \
			"	YEAR:MM:DD HR:MM:SS 	 2016:10:17 00:00:02	 Day = 3\n\n" \
			"	RTC time after delay_2 is..\n" \
			"	YEAR:MM:DD HR:MM:SS 	 2016:10:17 00:00:04	 Day = 3\n" \
			"	Successfully ran RTC Set time Example Test\n",
		.TCL_File = "rtc_clock/rtc_clock_download.tcl",
		.Plat_BIT_Op = XSDB_BIT,
	},
	.BIT[BIT_PL_UART_TEST] = {
		.Name = "PL UART Test",
		.Manual = 1,
		.Instruction = "\n" \
			"1- Connect to the 2nd com port (PL console), baud rate 9600\n" \
			"2- The following output should be displayed on the console:\n\n" \
			"	Testing UART\n" \
			"	9600,8,N,1\n" \
			"	Hello world!\n" \
			"	UART 02 Test Passed\n",
		.TCL_File = "pl_uart/pl_uart_download.tcl",
		.Plat_BIT_Op = XSDB_BIT,
	},
	.BIT[BIT_PROGRAM_QSPI] = {
		.Name = "Program QSPI",
		.Manual = 1,
		.Instruction = "\n" \
			"1- Connect to the 1st com port (Versal console), baud rate 115200\n" \
			"2- The following output should be displayed on the console:\n\n" \
			"	SF: Detected n25q00a with page size 512 Bytes, erase size 128 KiB, total 256 MiB\n" \
			"	SF: 5242880 bytes @ 0x0 Erased: OK\n" \
			"	device 0 offset 0x0, size 0x500000\n" \
			"	SF: 5242880 bytes @ 0x0 Written: OK\n" \
			"	Versal> \n",
		.TCL_File = "qspi/program_qspi_download.tcl",
		.Plat_BIT_Op = XSDB_BIT,
	},
};

/*
 * Board-specific Devices
 */
Plat_Devs_t VPK120_Devs = {
	.FeatureList = &VPK120_FeatureList,
	.BootModes = &VPK120_BootModes,
	.Clocks = &VPK120_Clocks,
	.Ina226s = &VPK120_Ina226s,
	.Voltages = &VPK120_Voltages,
	.GPIOs = &VPK120_GPIOs,
	.IO_Exp = &VPK120_IO_Exp,
	.OnBoard_EEPROM = &VPK120_OnBoard_EEPROM,
	.QSFPs = &VPK120_QSFPs,
	.FMCs = &VPK120_FMCs,
	.BITs = &VPK120_BITs,
};

void
VPK120_Version_Op(int *Major, int *Minor)
{
	*Major = 1;
	*Minor = 0;
}

int
VPK120_BootMode_Op(BootMode_t *BootMode, int Op)
{
	for (int i = 0; i < 4; i++) {
		if (GPIO_Set(VPK120_BootModes.Mode_Lines[i],
		    ((BootMode->Value >> i) & 0x1)) != 0) {
			SC_ERR("failed to set GPIO line %s",
			       VPK120_BootModes.Mode_Lines[i]);
			return -1;
		}
	}

	return 0;
}

int
VPK120_Reset_Op(void)
{
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

	return 0;
}

int
VPK120_JTAG_Op(int Select)
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
VPK120_XSDB_Op(const char *TCL_File, char *Output, int Length)
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

	(void) VPK120_JTAG_Op(1);
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
	(void) VPK120_JTAG_Op(0);
	return Ret;
}

int
VPK120_IDCODE_Op(char *Output, int Length)
{
	return VPK120_XSDB_Op(IDCODE_TCL, Output, Length);
}

/*
 * Get the board temperature
 */
int
VPK120_Temperature_Op(void)
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

int
VPK120_QSFP_ModuleSelect_Op(QSFP_t *QSFP, int State)
{
	IO_Exp_t *IO_Exp;
	unsigned int Value;

	if (State != 0 && State != 1) {
		SC_ERR("invalid QSFP module select state");
		return -1;
	}

	/* Nothing to do for 'State == 0' */
	if (State == 0) {
		return 0;
	}

	/* State == 1 */
	IO_Exp = Plat_Devs->IO_Exp;

	/* Set direction */
	Value = 0x73DF;
	if (Access_IO_Exp(IO_Exp, 1, 0x6, &Value) != 0) {
		SC_ERR("failed to set IO expander direction");
		return -1;
	}

	/*
	 * Only one QSFP-DD can be referenced at a time since both
	 * have address 0x50 and are on the same I2C bus, so make
	 * sure the other QSFP-DD is not selected.
	 */
	if (strcmp(QSFP->Name, "QSFPDD1") == 0) {
		Value = 0x820;
	} else {
		Value = 0x420;
	}

	if (Access_IO_Exp(IO_Exp, 1, 0x2, &Value) != 0) {
		SC_ERR("failed to set IO expander output");
		return -1;
	}

	return 0;
}

int
VPK120_FMCAutoVadj_Op(void)
{
	int Target_Index = -1;
	IO_Exp_t *IO_Exp;
	unsigned int Value;
	Voltages_t *Voltages;
	Voltage_t *Regulator;
	FMCs_t *FMCs;
	FMC_t *FMC;
	int Plugged = 0;
	float Voltage = 0;
	float Min_Voltage = 0;
	float Max_Voltage = 0;

	IO_Exp = Plat_Devs->IO_Exp;
	if (Access_IO_Exp(IO_Exp, 0, 0x0, &Value) != 0) {
		SC_ERR("failed to read input of IO Expander");
		return -1;
	}

	SC_INFO("IO Expander input: %#x", Value);

	/* If bit[0] is 0, then a FMC module is connected */
	if ((~Value & 0x1) == 0x1) {
		Plugged = 1;
	}

	Voltages  = Plat_Devs->Voltages;
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

	if (Plugged == 1) {
		FMCs = Plat_Devs->FMCs;
		FMC = &FMCs->FMC[0];
		if (FMC_Vadj_Range(FMC, &Min_Voltage, &Max_Voltage) != 0) {
			SC_ERR("failed to get Voltage Adjust range for FMC");
			return -1;
		}
	} else {
		Min_Voltage = 0;
		Max_Voltage = 1.5;
	}

	SC_INFO("Voltage Min: %.2f, Voltage Max: %.2f",
		Min_Voltage, Max_Voltage);

	/*
	 * Start with satisfying the lower target voltage and then see
	 * if it does also satisfy the higher voltage.
	 */
	if (Min_Voltage <= 1.2 && Max_Voltage >= 1.2) {
		Voltage = 1.2;
	}

	if (Min_Voltage <= 1.5 && Max_Voltage >= 1.5) {
		Voltage = 1.5;
	}

	if (Access_Regulator(Regulator, &Voltage, 1) != 0) {
		SC_ERR("failed to set voltage of VADJ_FMC regulator");
		return -1;
	}

	return 0;
}

int
VPK120_IDT_8A34001_Reset(void)
{
	IO_Exp_t *IO_Exp;
	unsigned int Value;

	IO_Exp = Plat_Devs->IO_Exp;

	/*
	 * The '8A34001_EXP_RST_B' line is controlled by bit 5 of register
	 * offset 3.  The output register pair (offsets 2 & 3) are written
	 * at once.
	 */
	Value = 0x0;    // Assert reset - active low
	if (Access_IO_Exp(IO_Exp, 1, 0x2, &Value) != 0) {
		SC_ERR("failed to assert reset of 8A34001 chip");
		return -1;
	}

	sleep (1);

	Value = 0x20;   // De-assert reset
	if (Access_IO_Exp(IO_Exp, 1, 0x2, &Value) != 0) {
		SC_ERR("failed to de-assert reset of 8A34001 chip");
		return -1;
	}

	return 0;
}

/*
 * Board-specific Operations
 */
Plat_Ops_t VPK120_Ops = {
	.Version_Op = VPK120_Version_Op,
	.BootMode_Op = VPK120_BootMode_Op,
	.Reset_Op = VPK120_Reset_Op,
	.IDCODE_Op = VPK120_IDCODE_Op,
	.XSDB_Op = VPK120_XSDB_Op,
	.Temperature_Op = VPK120_Temperature_Op,
	.QSFP_ModuleSelect_Op = VPK120_QSFP_ModuleSelect_Op,
	.FMCAutoVadj_Op = VPK120_FMCAutoVadj_Op,
};
