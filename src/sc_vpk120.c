/*
 * Copyright (c) 2021 Xilinx, Inc.  All rights reserved.
 *
 * SPDX-License-Identifier: MIT
 */

#include <stdio.h>
#include <unistd.h>
#include "sc_app.h"

extern int GPIO_Set(char *, int);
extern int Clocks_Check(void *);
extern int Voltages_Check(void *);

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
typedef enum {
	SI570_USER1_FMC,
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
	.Name = "EEPROM",
	.I2C_Bus = "/dev/i2c-10",
	.I2C_Address = 0x54,
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
	BIT_VOLTAGES_CHECK,
	BIT_MAX,
} BIT_Index;

BITs_t VPK120_BITs = {
	.Numbers = BIT_MAX,
	.BIT[BIT_CLOCKS_CHECK] = {
		.Name = "Check Clocks",		// Name of BIT to run
		.Plat_BIT_Op = Clocks_Check,	// BIT routine to invoke
	},
	.BIT[BIT_VOLTAGES_CHECK] = {
		.Name = "Check Voltages",
		.Plat_BIT_Op = Voltages_Check,
	},
};

/*
 * Board-specific Devices
 */
Plat_Devs_t VPK120_Devs = {
	.BootModes = &VPK120_BootModes,
	.Clocks = &VPK120_Clocks,
	.Ina226s = &VPK120_Ina226s,
	.Voltages = &VPK120_Voltages,
	.GPIOs = &VPK120_GPIOs,
	.IO_Exp = &VPK120_IO_Exp,
	.OnBoard_EEPROM = &VPK120_OnBoard_EEPROM,
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
/*
 * Board-specific Operations
 */
Plat_Ops_t VPK120_Ops = {
	.Version_Op = VPK120_Version_Op,
	.BootMode_Op = VPK120_BootMode_Op,
	.Reset_Op = VPK120_Reset_Op,
};
