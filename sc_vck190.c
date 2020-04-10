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

#include <stdio.h>
#include <stdlib.h>
#include <fcntl.h>
#include <unistd.h>
#include <time.h>
#include <sys/ioctl.h>
#include <linux/i2c-dev.h>
#include "sc_app.h"

int Plat_Reset_Ops(void);
int Plat_Workaround_Ops(void);

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

static int
Vccaux_Workaround(int State)
{
	int fd;
	char WriteBuffer[10];

	fd = open("/dev/i2c-3", O_RDWR);
	if (fd < 0) {
		printf("ERROR: cannot open the I2C device\n");
		return -1;
	}

	// Select IRPS5401
	if (ioctl(fd, I2C_SLAVE_FORCE, 0x47) < 0) {
		printf("ERROR: unable to access IRPS5401 address\n");
		return -1;
	}

	// Set the page for the IRPS5401
	WriteBuffer[0] = 0x00;
	WriteBuffer[1] = 0x03;
	if (write(fd, WriteBuffer, 2) != 2) {
		printf("ERROR: unable to select page for IRPS5401\n");
		return -1;
	}

	// Change the state of VOUT (ON: State = 1, OFF: State = 0)
	WriteBuffer[0] = 0x01;
	WriteBuffer[1] = (1 == State) ? 0x80 : 0x00;
	if (write(fd, WriteBuffer, 2) != 2) {
		printf("ERROR: unable to change VOUT for IRPS5401\n");
		return -1;
	}

	return 0;
}

int
Plat_Reset_Ops(void)
{
	char System_Cmd[SYSCMD_MAX];

	// Turn VCCINT_RAM off
	if (Vccaux_Workaround(0) != 0) {
		printf("ERROR: failed to turn VCCINT_RAM off\n");
		return -1;
	}

	// Assert POR
	sprintf(System_Cmd, "gpioset gpiochip0 82=0");
	system(System_Cmd);

	sleep(1);

	// De-assert POR
	sprintf(System_Cmd, "gpioset gpiochip0 82=1");
	system(System_Cmd);

	return 0;
}

int
Plat_Workaround_Ops(void)
{
	// Turn VCCINT_RAM on
	if (Vccaux_Workaround(1) != 0) {
		printf("ERROR: failed to turn VCCINT_RAM on\n");
		return -1;
	}

	return 0;
}

/*
 * Base date for manufacturing.
 */
struct tm BuildDate = {
	.tm_year = 66,
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
		return -1;
	}

	WriteBuffer[0] = 0x0;
	WriteBuffer[1] = 0x0;
	if (write(fd, WriteBuffer, 2) != 2) {
		printf("ERROR: unable to set address for M24128 EEPROM\n");
		return -1;
	}

	if (read(fd, ReadBuffer, 0xFF) != 0xFF) {
		printf("ERROR: unable to read from M24128 EEPROM\n");
		return -1;
	}

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
}
