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
		.I2C_Bus = "/dev/i2c-9",// I2C bus address behind the mux
		.I2C_Address = 0x5f,	// I2C device address
	},
	.Clock[SI570_VERSAL_SYS] = {
		.Name = "Versal Sys Clk Si570",
		.Type = Si570,
		.Sysfs_Path = "/sys/devices/platform/si570_ref_clk/set_rate",
		.Default_Freq = 33.333,
		.Upper_Freq = 160.0,
		.Lower_Freq = 10.0,
		.I2C_Bus = "/dev/i2c-11",
		.I2C_Address = 0x5d,
	},
	.Clock[SI570_LPDDR4_CLK1] = {
		.Name = "LPDDR4 Clk1 Si570",
		.Type = Si570,
		.Sysfs_Path = "/sys/devices/platform/si570_lpddr4_clk1/set_rate",
		.Default_Freq = 200.0,
		.Upper_Freq = 810.0,
		.Lower_Freq = 10.0,
		.I2C_Bus = "/dev/i2c-16",
		.I2C_Address = 0x60,
	},
	.Clock[SI570_LPDDR4_CLK2] = {
		.Name = "LPDDR4 Clk2 Si570",
		.Type = Si570,
		.Sysfs_Path = "/sys/devices/platform/si570_lpddr4_clk2/set_rate",
		.Default_Freq = 200.0,
		.Upper_Freq = 810.0,
		.Lower_Freq = 10.0,
		.I2C_Bus = "/dev/i2c-15",
		.I2C_Address = 0x60,
	},
	.Clock[SI570_LPDDR4_CLK3] = {
		.Name = "LPDDR4 Clk3 Si570",
		.Type = Si570,
		.Sysfs_Path = "/sys/devices/platform/si570_lpddr4_clk3/set_rate",
		.Default_Freq = 200.0,
		.Upper_Freq = 810.0,
		.Lower_Freq = 10.0,
		.I2C_Bus = "/dev/i2c-14",
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
		.I2C_Bus = "/dev/i2c-4",	// I2C bus of the device
		.I2C_Address = 0x40,		// I2C address of the device
		.Shunt_Resistor = 500,		// Value of shunt resistor in Micro-Ohm
		.Phase_Multiplier = 4,		// Sensor for # of phases
	},
	.Ina226[INA226_VCC_SOC] = {
		.Name = "VCC_SOC",
		.I2C_Bus = "/dev/i2c-4",
		.I2C_Address = 0x41,
		.Shunt_Resistor = 500,
		.Phase_Multiplier = 1,
	},
	.Ina226[INA226_VCC_PSFP] = {
		.Name = "VCCINT_PSFP",
		.I2C_Bus = "/dev/i2c-4",
		.I2C_Address = 0x45,
		.Shunt_Resistor = 5000,
		.Phase_Multiplier = 1,
	},
	.Ina226[INA226_VCC_PMC] = {
		.Name = "VCC_PMC",
		.I2C_Bus = "/dev/i2c-4",
		.I2C_Address = 0x42,
		.Shunt_Resistor = 5000,
		.Phase_Multiplier = 1,
	},
	.Ina226[INA226_VCC_RAM_VCCINT_GT] = {
		.Name = "VCC_RAM_VCCINT_GT",
		.I2C_Bus = "/dev/i2c-4",
		.I2C_Address = 0x43,
		.Shunt_Resistor = 2000,
		.Phase_Multiplier = 1,
	},
	.Ina226[INA226_VCC_PSLP_CPM5] = {
		.Name = "VCC_PSLP_CPM5",
		.I2C_Bus = "/dev/i2c-4",
		.I2C_Address = 0x44,
		.Shunt_Resistor = 5000,
		.Phase_Multiplier = 1,
	},
	.Ina226[INA226_VCCO_MIO] = {
		.Name = "VCCO_MIO",
		.I2C_Bus = "/dev/i2c-6",
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
	.Ina226[INA226_VCC1V5] = {
		.Name = "VCC1V5",
		.I2C_Bus = "/dev/i2c-6",
		.I2C_Address = 0x43,
		.Shunt_Resistor = 5000,
		.Phase_Multiplier = 1,
	},
	.Ina226[INA226_LPDMGTYAVCC] = {
		.Name = "LPDMGTYAVCC",
		.I2C_Bus = "/dev/i2c-6",
		.I2C_Address = 0x4B,
		.Shunt_Resistor = 5000,
		.Phase_Multiplier = 1,
	},
	.Ina226[INA226_MGTVCCAUX] = {
		.Name = "MGTVCCAUX",
		.I2C_Bus = "/dev/i2c-6",
		.I2C_Address = 0x48,
		.Shunt_Resistor = 5000,
		.Phase_Multiplier = 1,
	},
	.Ina226[INA226_MGTAVCC] = {
		.Name = "MGTAVCC",
		.I2C_Bus = "/dev/i2c-6",
		.I2C_Address = 0x42,
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
	.Ina226[INA226_VCCO_502] = {
		.Name = "VCCO_502",
		.I2C_Bus = "/dev/i2c-6",
		.I2C_Address = 0x47,
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
	.Ina226[INA226_VCC1V1_LP4] = {
		.Name = "VCC1V1_LP4",
		.I2C_Bus = "/dev/i2c-6",
		.I2C_Address = 0x49,
		.Shunt_Resistor = 2000,
		.Phase_Multiplier = 1,
	},
	.Ina226[INA226_LPDMGTYVCCAUX] = {
		.Name = "LPDMGTYVCCAUX",
		.I2C_Bus = "/dev/i2c-6",
		.I2C_Address = 0x4D,
		.Shunt_Resistor = 5000,
		.Phase_Multiplier = 1,
	},
	.Ina226[INA226_LPDMGTYAVTT] = {
		.Name = "LPDMGTYAVTT",
		.I2C_Bus = "/dev/i2c-6",
		.I2C_Address = 0x4C,
		.Shunt_Resistor = 2000,
		.Phase_Multiplier = 1,
	},
	.Ina226[INA226_MGTAVTT] = {
		.Name = "MGTAVTT",
		.I2C_Bus = "/dev/i2c-6",
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
		.I2C_Bus = "/dev/i2c-3",        // I2C bus of the device
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
		.I2C_Bus = "/dev/i2c-3",
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
		.I2C_Bus = "/dev/i2c-3",
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
	.Voltage[VOLTAGE_VCC1V5] = {
		.Name = "VCC1V5",
		.Part_Name = "IRPS5401",
		.Supported_Volt = { 1.5, -1 },
		.Maximum_Volt = VOLT_MAX(1.5),
		.Typical_Volt = 1.5,
		.Minimum_Volt = VOLT_MIN(1.5),
		.I2C_Bus = "/dev/i2c-3",
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
		.I2C_Bus = "/dev/i2c-3",
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
		.I2C_Bus = "/dev/i2c-3",
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
		.I2C_Bus = "/dev/i2c-3",
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
	.Voltage[VOLTAGE_VCC_PSLP_CPM5] = {
		.Name = "VCC_PSLP_CPM5",
		.Part_Name = "IRPS5401",
		.Supported_Volt = { 0.7, 0.8, -1 },
		.Maximum_Volt = VOLT_MAX(0.8),
		.Typical_Volt = 0.8,
		.Minimum_Volt = VOLT_MIN(0.8),
		.I2C_Bus = "/dev/i2c-3",
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
		.I2C_Bus = "/dev/i2c-3",
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
		.I2C_Bus = "/dev/i2c-3",
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
		.I2C_Bus = "/dev/i2c-3",
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
		.I2C_Bus = "/dev/i2c-3",
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
		.I2C_Bus = "/dev/i2c-3",
		.I2C_Address = 0x49,
		.Page_Select = -1,
	},
};

OnBoard_EEPROM_t VPK120_OnBoard_EEPROM = {
	.Name = "EEPROM",
	.I2C_Bus = "/dev/i2c-11",
	.I2C_Address = 0x54,
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
	.OnBoard_EEPROM = &VPK120_OnBoard_EEPROM,
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
