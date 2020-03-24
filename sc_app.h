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

#ifndef SC_APP_H_
#define SC_APP_H_

#define STRLEN_MAX	32
#define	ITEMS_MAX	20
#define	SYSCMD_MAX	128
#define	MUX_MAX		4

/*
 * I2C Buses
 */
typedef struct {
	char	Name[STRLEN_MAX];
} I2C_Bus_t;

typedef struct I2C_Buses {
	int	Numbers;
	I2C_Bus_t	I2C_Bus[ITEMS_MAX];
} I2C_Buses_t;

typedef struct {
	int	Mux_Levels;
	int	Bus_Id;
	int	Mux_Route[MUX_MAX][2];
} I2C_Route_t;

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
	Oscillator,
	I2C,
} Clock_Type;

typedef struct {
	char	Name[STRLEN_MAX];
	unsigned int	Default_Freq;
	unsigned int	Upper_Freq;
	unsigned int	Lower_Freq;
	Clock_Type	Type;
	char	Sysfs_Path[SYSCMD_MAX];
	I2C_Route_t	I2C_Route;
	int	I2C_Address;
} Clock_t;

typedef struct Clocks {
	int	Numbers;
	Clock_t	Clock[ITEMS_MAX];
} Clocks_t;

/*
 * INA226
 */
typedef struct {
	char	Name[STRLEN_MAX];
	char	Sensor_Name[STRLEN_MAX];
	I2C_Route_t	I2C_Route;
	int	I2C_Address;
} Ina226_t;

typedef struct Ina226s {
	int	Numbers;
	Ina226_t	Ina226[ITEMS_MAX];
} Ina226s_t;

/*
 * Voltages
 */
typedef struct {
	char	Name[STRLEN_MAX];
	unsigned int	Default_Volt;
	unsigned int	Upper_Volt;
	unsigned int	Lower_Volt;
	int		Page_Select;
	I2C_Route_t	I2C_Route;
	int	I2C_Address;
} Voltage_t;

typedef struct Voltages {
	int	Numbers;
	Voltage_t	Voltage[ITEMS_MAX];
} Voltages_t;

#endif	/* SC_APP_H_ */

