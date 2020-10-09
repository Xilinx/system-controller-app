#include <stdio.h>
#include <stdlib.h>
#include <fcntl.h>
#include <unistd.h>
#include <time.h>
#include <string.h>
#include <sys/ioctl.h>
#include <linux/i2c-dev.h>
#include "sc_app.h"

#define BOARDNAME	"vck190"

int Plat_Board_Name(char *Name);
int Plat_Reset_Ops(void);
int Workaround_Vccaux(void *Arg);

/*
 * I2C Buses
 */
typedef enum {
	I2C_BUS_0,
	I2C_BUS_1,
	I2C_BUS_MAX,
} I2C_Bus_Index;

I2C_Buses_t I2C_Buses = {
	.Numbers = I2C_BUS_MAX,
	.I2C_Bus[I2C_BUS_0] = {
		.Name = "i2c-0",		// Name of the bus referenced by application
	},
	.I2C_Bus[I2C_BUS_1] = {
		.Name = "i2c-1",
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

BootModes_t BootModes = {
	.Numbers = BOOTMODE_MAX,
	.Mode_Lines = {
		 "gpiochip0 78", "gpiochip0 79",
		 "gpiochip0 80", "gpiochip0 81"
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
	SI570_ZSFP,
	SI570_USER1_FMC,
	SI570_REF,
	SI570_DIMM1,
	SI570_LPDDR4_CLK1,
	SI570_LPDDR4_CLK2,
	SI570_HSPD,
	CLOCK_MAX,
} Clock_Index;

Clocks_t Clocks = {
	.Numbers = CLOCK_MAX,
	.Clock[SI570_ZSFP] = {
		.Name = "zSFP Si570",		// Name of the device referenced by application
		.Type = Oscillator,		// Clock generators accessible through sysfs
		.Sysfs_Path = "/sys/devices/platform/si570_zsfp_clk/set_rate",
		.Default_Freq = 156250000,	// Factory default frequency in Hz
		.Upper_Freq = 810000000,	// Upper-bound frequency in Hz
		.Lower_Freq = 10000000,		// Lower-bound frequency in Hz
		.I2C_Route.Bus_Id = I2C_BUS_0,	// Main I2C bus connected to the device
		.I2C_Route.Mux_Levels = 1,	// Number of muxes in route to the device
		.I2C_Route.Mux_Route = {{0x74, 0x20}}, // <I2C address, channel select> pair for each mux
		.I2C_Address = 0x5d,		// I2C address of the device
	},
	.Clock[SI570_USER1_FMC] = {
		.Name = "User1 FMC1 Si570",
		.Type = Oscillator,
		.Sysfs_Path = "/sys/devices/platform/si570_user1_clk/set_rate",
		.Default_Freq = 100000000,
		.Upper_Freq = 1250000000,
		.Lower_Freq = 10000000,
		.I2C_Route.Bus_Id = I2C_BUS_0,
		.I2C_Route.Mux_Levels = 1,
		.I2C_Route.Mux_Route = {{0x74, 0x40}},
		.I2C_Address = 0x5f,
	},
	/* Missing 8A34001 FMC1 */
	.Clock[SI570_REF] = {
		.Name = "Versal Sys Clk Si570",
		.Type = Oscillator,
		.Sysfs_Path = "/sys/devices/platform/ref_clk/set_rate",
		.Default_Freq = 33333333,
		.Upper_Freq = 160000000,
		.Lower_Freq = 10000000,
		.I2C_Route.Bus_Id = I2C_BUS_1,
		.I2C_Route.Mux_Levels = 1,
		.I2C_Route.Mux_Route = {{0x74, 0x01}},
		.I2C_Address = 0x5d,
	},
	.Clock[SI570_DIMM1] = {
		.Name = "Dimm1 Si570",
		.Type = Oscillator,
		.Sysfs_Path = "/sys/devices/platform/si570_ddrdimm1_clk/set_rate",
		.Default_Freq = 200000000,
		.Upper_Freq = 810000000,
		.Lower_Freq = 10000000,
		.I2C_Route.Bus_Id = I2C_BUS_1,
		.I2C_Route.Mux_Levels = 1,
		.I2C_Route.Mux_Route = {{0x74, 0x08}},
		.I2C_Address = 0x60,
	},
	.Clock[SI570_LPDDR4_CLK1] = {
		.Name = "LPDDR4 CLK1 Si570",
		.Type = Oscillator,
		.Sysfs_Path = "/sys/devices/platform/si570_lpddr4_clk1/set_rate",
		.Default_Freq = 200000000,
		.Upper_Freq = 810000000,
		.Lower_Freq = 10000000,
		.I2C_Route.Bus_Id = I2C_BUS_1,
		.I2C_Route.Mux_Levels = 1,
		.I2C_Route.Mux_Route = {{0x74, 0x20}},
		.I2C_Address = 0x60,
	},
	.Clock[SI570_LPDDR4_CLK2] = {
		.Name = "LPDDR4 CLK2 Si570",
		.Type = Oscillator,
		.Sysfs_Path = "/sys/devices/platform/si570_lpddr4_clk2/set_rate",
		.Default_Freq = 200000000,
		.Upper_Freq = 810000000,
		.Lower_Freq = 10000000,
		.I2C_Route.Bus_Id = I2C_BUS_1,
		.I2C_Route.Mux_Levels = 1,
		.I2C_Route.Mux_Route = {{0x74, 0x10}},
		.I2C_Address = 0x60,
	},
	.Clock[SI570_HSPD] = {
		.Name = "HSPD Si570",
		.Type = Oscillator,
		.Sysfs_Path = "/sys/devices/platform/si570_hsdp_clk/set_rate",
		.Default_Freq = 156250000,
		.Upper_Freq = 810000000,
		.Lower_Freq = 10000000,
		.I2C_Route.Bus_Id = I2C_BUS_1,
		.I2C_Route.Mux_Levels = 1,
		.I2C_Route.Mux_Route = {{0x74, 0x40}},
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

Ina226s_t Ina226s = {
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

Power_Domains_t Power_Domains = {
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

#define VOLT_MIN(VOLT)	(VOLT - (0.03 * VOLT))
#define VOLT_MAX(VOLT)	(VOLT + (0.03 * VOLT))

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

Voltages_t Voltages = {
	.Numbers = VOLTAGE_MAX,
	.Voltage[VOLTAGE_VCCINT] = {
		.Name = "VCCINT",		// Name of the device referenced by application
		.Part_Name = "IR35215",		// Name of the part that is used
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
		.Maximum_Volt = VOLT_MAX(1.1),
		.Typical_Volt = 1.1,
		.Minimum_Volt = VOLT_MIN(1.1),
		.I2C_Bus = "/dev/i2c-3",
		.I2C_Address = 0x4D,
		.Page_Select = 3,
	},
};

/*
 * IO Expander
 */
IO_Exp_t IO_Exp = {
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
	.I2C_Bus = "/dev/i2c-1",
	.I2C_Address = 0x20,
};

/*
 * Daughter Card
 */
Daughter_Card_t Daughter_Card = {
	.Name = "EBM",
	.I2C_Bus = "/dev/i2c-11",
	.I2C_Address = 0x52,
};

/*
 * SFP Connectors
 */
typedef enum {
	SFP_0,
	SFP_1,
	SFP_MAX,
} SFP_Index;

SFPs_t SFPs = {
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
 * QSFP Connectors
 */
typedef enum {
	QSFP_0,
	QSFP_MAX,
} QSFP_Index;

QSFPs_t QSFPs = {
	.Numbers = QSFP_MAX,
	.QSFP[QSFP_0] = {
		.Name = "zQSFP1",
		.I2C_Bus = "/dev/i2c-21",
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

Workarounds_t Workarounds = {
	.Numbers = WORKAROUND_MAX,
	.Workaround[WORKAROUND_VCCAUX] = {
		.Name = "vccaux",		// Name of workaround for this platform
		.Arg_Needed = 1,		// Whether following workaround routine needs argument
		.Plat_Workaround_Op = Workaround_Vccaux, // Platform workaround routine
	},
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
		printf("ERROR: cannot open the I2C device\n");
		return -1;
	}

	// Select IRPS5401
	if (ioctl(fd, I2C_SLAVE_FORCE, 0x47) < 0) {
		printf("ERROR: unable to access IRPS5401 address\n");
		(void) close(fd);
		return -1;
	}

	// Set the page for the IRPS5401
	WriteBuffer[0] = 0x00;
	WriteBuffer[1] = 0x03;
	if (write(fd, WriteBuffer, 2) != 2) {
		printf("ERROR: unable to select page for IRPS5401\n");
		(void) close(fd);
		return -1;
	}

	// Change the state of VOUT (ON: State = 1, OFF: State = 0)
	WriteBuffer[0] = 0x01;
	WriteBuffer[1] = (1 == *State) ? 0x80 : 0x00;
	if (write(fd, WriteBuffer, 2) != 2) {
		printf("ERROR: unable to change VOUT for IRPS5401\n");
		(void) close(fd);
		return -1;
	}

	(void) close(fd);
	return 0;
}

int
Plat_Board_Name(char *Board_Name)
{
	if (Board_Name == NULL) {
		printf("ERROR: need a pre-allocated string array\n");
		return -1;
	}

	(void) strcpy(Board_Name, BOARDNAME);
	return 0;
}

int
Plat_Version_Ops(int *Major, int *Minor)
{
	// Same version number as common code.
	*Major = -1;
	*Minor = -1;
	return 0;
}

int
Plat_Reset_Ops(void)
{
	FILE *FP;
	int State;
	char Buffer[SYSCMD_MAX];

	if (access(SILICONFILE, F_OK) == 0) {
		FP = fopen(SILICONFILE, "r");
		if (FP == NULL) {
			printf("ERROR: failed to read silicon file\n");
			return -1;
		}

		(void) fgets(Buffer, SYSCMD_MAX, FP);
		(void) fclose(FP);
		if (strcmp(Buffer, "ES1\n") == 0) {
			// Turn VCCINT_RAM off
			State = 0;
			if (Workaround_Vccaux(&State) != 0) {
				printf("ERROR: failed to turn VCCINT_RAM off\n");
				return -1;
			}
		}
	}

	// Assert POR
	sprintf(Buffer, "gpioset gpiochip0 82=0");
	system(Buffer);

	sleep(1);

	// De-assert POR
	sprintf(Buffer, "gpioset gpiochip0 82=1");
	system(Buffer);

	return 0;
}

/*
 * Base build date for manufacturing is 1/1/1996.
 */
struct tm BuildDate = {
	.tm_year = 96,
	.tm_mday = 1,
};

int
Plat_EEPROM_Ops(void)
{
	int fd;
	char ReadBuffer[0xFF] = {0};
	char WriteBuffer[3];
	char Buffer[STRLEN_MAX];
	time_t Time;

	fd = open("/dev/i2c-11", O_RDWR);
	if (fd < 0) {
		printf("ERROR: cannot open the I2C device\n");
		return -1;
	}

	// Select M24128 EEPROM
	if (ioctl(fd, I2C_SLAVE_FORCE, 0x54) < 0) {
		printf("ERROR: unable to access M24128 EEPROM address\n");
		(void) close(fd);
		return -1;
	}

	WriteBuffer[0] = 0x0;
	WriteBuffer[1] = 0x0;
	if (write(fd, WriteBuffer, 2) != 2) {
		printf("ERROR: unable to set address for M24128 EEPROM\n");
		(void) close(fd);
		return -1;
	}

	if (read(fd, ReadBuffer, 0xFF) != 0xFF) {
		printf("ERROR: unable to read from M24128 EEPROM\n");
		(void) close(fd);
		return -1;
	}

	(void) close(fd);

	// Language Code
	printf("Language: %d\n", ReadBuffer[0xA]);

	// Manufacturing Date
	BuildDate.tm_min = (ReadBuffer[0xB] << 16 | ReadBuffer[0xC] << 8 |
	    ReadBuffer[0xD]);
	Time = mktime(&BuildDate);
	if (Time == -1) {
		printf("ERROR: invalid manufacturing date\n");
		return -1;
	}

	printf("Manufacturing Date: %s", ctime(&Time));

	// Manufacturer
	snprintf(Buffer, 6+1, "%s", &ReadBuffer[0xF]);
	printf("Manufacturer: %s\n", Buffer);

	// Product Name
	snprintf(Buffer, 16+1, "%s", &ReadBuffer[0x16]);
	printf("Product Name: %s\n", Buffer);

	// Board Serial Number
	snprintf(Buffer, 16+1, "%s", &ReadBuffer[0x27]);
	printf("Board Serial Number: %s\n", Buffer);

	// Board Part Number
	snprintf(Buffer, 9+1, "%s", &ReadBuffer[0x38]);
	printf("Board Part Number: %s\n", Buffer);

	// Board Revision
	snprintf(Buffer, 8+1, "%s", &ReadBuffer[0x44]);
	printf("Board Reversion: %s\n", Buffer);

	// MAC Address 0
	printf("MAC Address 0: %x:%x:%x:%x:%x:%x\n", ReadBuffer[0x80],
	    ReadBuffer[0x81], ReadBuffer[0x82], ReadBuffer[0x83],
	    ReadBuffer[0x84], ReadBuffer[0x85]);

	// MAC Address 1
	printf("MAC Address 1: %x:%x:%x:%x:%x:%x\n", ReadBuffer[0x86],
	    ReadBuffer[0x87], ReadBuffer[0x88], ReadBuffer[0x89],
	    ReadBuffer[0x8A], ReadBuffer[0x8B]);

	return 0;
}

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
struct ddr_dimm1 Dimm1 = {
	.I2C_Bus = "/dev/i2c-14",
	.Spd   = { .Bus_addr = 0x50, .Reg_addr = 0, .Read_len = 16 },
	.Therm = { .Bus_addr = 0x18, .Reg_addr = 5, .Read_len = 2 }
};

/*
 * We are using spaces in .Name to be consistent with 'getclock' code.
 * Using spaces or tabs as a part of an option argument is problematic,
 * because they have special meaning for every Linux shell.
 * A better way is to split the name in to bank and gpio_label, print
 * both and accept both forms "xxx - gpio_label" and gpio_label.
 * Use Plat_Gpio_match() to accept both forms knowing that
 * that the gpio_label starts at .Name[6]
 */
#define GPIO_LN(Num, name) { .Line = Num, .Name = name }
const struct Gpio_line_name Gpio_target[] = {
	GPIO_LN(12, "500 - SYSCTLR_PB"),
	GPIO_LN(10, "500 - DC_SYS_CTRL3"),
	GPIO_LN(9, "500 - DC_SYS_CTRL2"),
	GPIO_LN(8, "500 - DC_SYS_CTRL1"),
	GPIO_LN(7, "500 - DC_SYS_CTRL0"),
	GPIO_LN(51, "501 - SYSCTLR_SD1_CLK"),
	GPIO_LN(50, "501 - SYSCTLR_SD1_CMD"),
	GPIO_LN(49, "501 - SYSCTLR_SD1_DATA3"),
	GPIO_LN(48, "501 - SYSCTLR_SD1_DATA2"),
	GPIO_LN(47, "501 - SYSCTLR_SD1_DATA1"),
	GPIO_LN(46, "501 - SYSCTLR_SD1_DATA0"),
	GPIO_LN(45, "501 - SYSCTLR_SD1_CD_B"),
	GPIO_LN(42, "501 - SYSCTLR_ETH_RESET_B_R"),
	GPIO_LN(39, "501 - SYSCTLR_UART0_TXD_OUT"),
	GPIO_LN(38, "501 - SYSCTLR_UART0_RXD_IN"),
	GPIO_LN(37, "501 - LP_I2C1_SDA"),
	GPIO_LN(36, "501 - LP_I2C1_SCL"),
	GPIO_LN(35, "501 - LP_I2C0_PMC_SDA"),
	GPIO_LN(34, "501 - LP_I2C0_PMC_SCL"),
	GPIO_LN(77, "502 - SYSCTLR_ETH_MDIO"),
	GPIO_LN(76, "502 - SYSCTLR_ETH_MDC")
};

int Plat_Gpio_match(int Idx, char *Target)
{
	return !strncmp(Gpio_target[Idx].Name, Target, STRLEN_MAX) ||
		!strncmp(&Gpio_target[Idx].Name[6], Target, STRLEN_MAX);
}

int Plat_Gpio_target_size(void)
{
	return sizeof(Gpio_target) / sizeof(Gpio_target[0]);
}

/*
 * The QSFP must be selected and not held in Reset (High) in order to be
 * accessed.  These lines are driven by Vesal.  The QSFP1_RESETL_LS high
 * has a pull up, but QSFP1_MODSKLL_LS has no pull down.  If Versal is not
 * programmed to drive QSFP1_MODSKLL_LS low, the QSFP will not respond.
 */
int
Plat_QSFP_Init(void)
{
	char System_Cmd[SYSCMD_MAX];

	(void) Plat_Reset_Ops();
	sprintf(System_Cmd, "%s; %s %s%s", XSDB_ENV, XSDB_CMD, BIT_PATH,
	    "qsfp_set_modsel/qsfp_download.tcl");
	system(System_Cmd);

	return 0;
}
