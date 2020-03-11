/******************************************************************************
*
* Copyright (C) 2020 Xilinx, Inc.  All rights reserved.
*
* Permission is hereby granted, free of charge, to any person obtaining a copy
* of this software and associated documentation files (the "Software"), to deal
* in the Software without restriction, including without limitation the rights
* to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
* copies of the Software, and to permit persons to whom the Software is
* furnished to do so, subject to the following conditions:
*
* The above copyright notice and this permission notice shall be included in
* all copies or substantial portions of the Software.
*
* THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
* IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
* FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL
* THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
* LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
* OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
* THE SOFTWARE.
*
******************************************************************************/

#include "sc_app.h"

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
		.Upper_Freq = 180000000,
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
		.Upper_Freq = 180000000,
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
		.Upper_Freq = 180000000,
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
		.Upper_Freq = 180000000,
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
		.Sensor_Name = "ina226_vccint-isa-0000", // Name of sensor
		.I2C_Route.Bus_Id = I2C_BUS_0,	// Main I2C bus connected to the device
		.I2C_Route.Mux_Levels = 1,	// Number of muxes in route to the device
		.I2C_Route.Mux_Route = {{0x74, 0x02}}, // <I2C address, channel select> pair for each mux
		.I2C_Address = 0x40,		// I2C address of the device
	},
	.Ina226[INA226_VCC_SOC] = {
		.Name = "VCC_SOC",
		.Sensor_Name = "ina226_vcc_soc-isa-0000",
		.I2C_Route.Bus_Id = I2C_BUS_0,
		.I2C_Route.Mux_Levels = 1,
		.I2C_Route.Mux_Route = {{0x74, 0x02}},
		.I2C_Address = 0x41,
	},
	.Ina226[INA226_VCC_PMC] = {
		.Name = "VCC_PMC",
		.Sensor_Name = "ina226_vcc_pmc-isa-0000",
		.I2C_Route.Bus_Id = I2C_BUS_0,
		.I2C_Route.Mux_Levels = 1,
		.I2C_Route.Mux_Route = {{0x74, 0x02}},
		.I2C_Address = 0x42,
	},
	.Ina226[INA226_VCCINT_RAM] = {
		.Name = "VCCINT_RAM",
		.Sensor_Name = "ina226_vcc_ram-isa-0000",
		.I2C_Route.Bus_Id = I2C_BUS_0,
		.I2C_Route.Mux_Levels = 1,
		.I2C_Route.Mux_Route = {{0x74, 0x02}},
		.I2C_Address = 0x43,
	},
	.Ina226[INA226_VCCINT_PSLP] = {
		.Name = "VCCINT_PSLP",
		.Sensor_Name = "ina226_vcc_pslp-isa-0000",
		.I2C_Route.Bus_Id = I2C_BUS_0,
		.I2C_Route.Mux_Levels = 1,
		.I2C_Route.Mux_Route = {{0x74, 0x02}},
		.I2C_Address = 0x44,
	},
	.Ina226[INA226_VCCINT_PSFP] = {
		.Name = "VCCINT_PSFP",
		.Sensor_Name = "ina226_vcc_psfp-isa-0000",
		.I2C_Route.Bus_Id = I2C_BUS_0,
		.I2C_Route.Mux_Levels = 1,
		.I2C_Route.Mux_Route = {{0x74, 0x02}},
		.I2C_Address = 0x45,
	},
	.Ina226[INA226_VCCAUX] = {
		.Name = "VCCAUX",
		.Sensor_Name = "ina226_vccaux-isa-0000",
		.I2C_Route.Bus_Id = I2C_BUS_0,
		.I2C_Route.Mux_Levels = 1,
		.I2C_Route.Mux_Route = {{0x74, 0x08}},
		.I2C_Address = 0x40,
	},
	.Ina226[INA226_VCCAUX_PMC] = {
		.Name = "VCCAUX_PMC",
		.Sensor_Name = "ina226_vccaux_pmc-isa-0000",
		.I2C_Route.Bus_Id = I2C_BUS_0,
		.I2C_Route.Mux_Levels = 1,
		.I2C_Route.Mux_Route = {{0x74, 0x08}},
		.I2C_Address = 0x41,
	},
	.Ina226[INA226_VCC_MIO] = {
		.Name = "VCC_MIO",
		.Sensor_Name = "ina226_vcco_503-isa-0000",
		.I2C_Route.Bus_Id = I2C_BUS_0,
		.I2C_Route.Mux_Levels = 1,
		.I2C_Route.Mux_Route = {{0x74, 0x08}},
		.I2C_Address = 0x45,
	},
	.Ina226[INA226_VCC1V8] = {
		.Name = "VCC1V8",
		.Sensor_Name = "ina226_vcc_1v8-isa-0000",
		.I2C_Route.Bus_Id = I2C_BUS_0,
		.I2C_Route.Mux_Levels = 1,
		.I2C_Route.Mux_Route = {{0x74, 0x08}},
		.I2C_Address = 0x46,
	},
	.Ina226[INA226_VCC3V3] = {
		.Name = "VCC3V3",
		.Sensor_Name = "ina226_vcc_3v3-isa-0000",
		.I2C_Route.Bus_Id = I2C_BUS_0,
		.I2C_Route.Mux_Levels = 1,
		.I2C_Route.Mux_Route = {{0x74, 0x08}},
		.I2C_Address = 0x47,
	},
	.Ina226[INA226_VCC1V2_DDR4] = {
		.Name = "VCC1V2_DDR4",
		.Sensor_Name = "ina226_vcc_1v2_ddr4-isa-0000",
		.I2C_Route.Bus_Id = I2C_BUS_0,
		.I2C_Route.Mux_Levels = 1,
		.I2C_Route.Mux_Route = {{0x74, 0x08}},
		.I2C_Address = 0x48,
	},
	.Ina226[INA226_VCC1V1_LP4] = {
		.Name = "VCC1V1_LP4",
		.Sensor_Name = "ina226_vcc_1v1_lp4-isa-0000",
		.I2C_Route.Bus_Id = I2C_BUS_0,
		.I2C_Route.Mux_Levels = 1,
		.I2C_Route.Mux_Route = {{0x74, 0x08}},
		.I2C_Address = 0x49,
	},
	.Ina226[INA226_VADJ_FMC] = {
		.Name = "VADJ_FMC",
		.Sensor_Name = "ina226_vadj_fmc-isa-0000",
		.I2C_Route.Bus_Id = I2C_BUS_0,
		.I2C_Route.Mux_Levels = 1,
		.I2C_Route.Mux_Route = {{0x74, 0x08}},
		.I2C_Address = 0x4A,
	},
	.Ina226[INA226_MGTYAVCC] = {
		.Name = "MGTYAVCC",
		.Sensor_Name = "ina226_mgtyavcc-isa-0000",
		.I2C_Route.Bus_Id = I2C_BUS_0,
		.I2C_Route.Mux_Levels = 1,
		.I2C_Route.Mux_Route = {{0x74, 0x08}},
		.I2C_Address = 0x4B,
	},
	.Ina226[INA226_MGTYAVTT] = {
		.Name = "MGTYAVTT",
		.Sensor_Name = "ina226_mgtyavtt-isa-0000",
		.I2C_Route.Bus_Id = I2C_BUS_0,
		.I2C_Route.Mux_Levels = 1,
		.I2C_Route.Mux_Route = {{0x74, 0x08}},
		.I2C_Address = 0x4C,
	},
	.Ina226[INA226_MGTYVCCAUX] = {
		.Name = "MGTYVCCAUX",
		.Sensor_Name = "ina226_mgtyavtt-isa-0000",
		.I2C_Route.Bus_Id = I2C_BUS_0,
		.I2C_Route.Mux_Levels = 1,
		.I2C_Route.Mux_Route = {{0x74, 0x08}},
		.I2C_Address = 0x4D,
	},
};

/*
 * Voltages - power rails
 */
typedef enum {
	VOLTAGE_VCCINT,
	VOLTAGE_VCCINT_SOC,
	VOLTAGE_MAX,
} Voltage_Index;

Voltages_t Voltages = {
	.Numbers = VOLTAGE_MAX,
	.Voltage[VOLTAGE_VCCINT] = {
		.Name = "vccint",		// Name of the device referenced by application
		.Default_Volt = 7800,		// Factory default voltage in millivolt
		.Page_Select = 0,		// Device page selection
		.I2C_Route.Bus_Id = I2C_BUS_0,	// Main I2C bus connected to the device
		.I2C_Route.Mux_Levels = 1,	// Number of muxes in route to the device
		.I2C_Route.Mux_Route = {{0x74, 0x02}}, // <I2C address, channel select> pair for each mux
		.I2C_Address = 0x41,		// I2C address of the device
	},
	.Voltage[VOLTAGE_VCCINT_SOC] = {
		.Name = "vccint_soc",
		.Default_Volt = 7800,
		.Page_Select = 6,
		.I2C_Route.Bus_Id = I2C_BUS_0,
		.I2C_Route.Mux_Levels = 1,
		.I2C_Route.Mux_Route = {{0x74, 0x02}},
		.I2C_Address = 0x41,
	},
};
