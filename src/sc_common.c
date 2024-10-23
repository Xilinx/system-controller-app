/*
 * Copyright (c) 2021 - 2022 Xilinx, Inc.  All rights reserved.
 * Copyright (c) 2022 - 2024 Advanced Micro Devices, Inc.  All rights reserved.
 *
 * SPDX-License-Identifier: MIT
 */

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <fcntl.h>
#include <ctype.h>
#include <dirent.h>
#include <string.h>
#include <time.h>
#include <math.h>
#include <libgen.h>
#include <gpiod.h>
#include <sys/stat.h>
#include "sc_app.h"

Plat_Devs_t *Plat_Devs;

char SC_APP_File[SYSCMD_MAX];
extern char Board_Name[];
extern char Silicon_Revision[];
extern OnBoard_EEPROM_t Common_OnBoard_EEPROM;
extern OnBoard_EEPROM_t Legacy_OnBoard_EEPROM;

char *
Appfile(char *Filename)
{
	char Buffer[STRLEN_MAX];

	(void) sprintf(Buffer, "%s/.sc_app", INSTALLDIR);
	if (access(Buffer, F_OK) == -1) {
		if (mkdir(Buffer, 0755) == -1) {
			SC_ERR("mkdir %s failed: %m", Buffer);
			return NULL;
		}
	}

	(void) sprintf(SC_APP_File, "%s/%s", Buffer, Filename);
	return SC_APP_File;
}

static int
Get_Product_Name(OnBoard_EEPROM_t *EEPROM, char *Product_Name)
{
	int FD;
	char In_Buffer[SYSCMD_MAX];
	char Out_Buffer[LSTRLEN_MAX];
	int Offset, Length;
	int Found = 0;

	/*
	 * Referencing EEPROM to obtain 'Product Name' can be overridden by
	 * defining 'Board' parameter in CONFIGFILE.
	 */
	if (Check_Config_File("Board", Out_Buffer, &Found) != 0) {
		return -1;
	}

	if (Found) {
		(void) strcpy(Product_Name, Out_Buffer);
		return 0;
	}

	FD = open(EEPROM->I2C_Bus, O_RDWR);
	if (FD < 0) {
		SC_INFO("unable to access I2C bus %s: %m", EEPROM->I2C_Bus);
		return -1;
	}

	if (ioctl(FD, I2C_SLAVE_FORCE, EEPROM->I2C_Address) < 0) {
		SC_INFO("unable to access onboard EEPROM address %#x",
			EEPROM->I2C_Address);
		(void) close(FD);
		return -1;
	}

	Out_Buffer[0] = 0x0;
	Out_Buffer[1] = 0x0;
	if (write(FD, Out_Buffer, 2) != 2) {
		SC_INFO("unable to set the offset address on onboard EEPROM");
		(void) close(FD);
		return -1;
	}

	(void) memset(In_Buffer, 0, SYSCMD_MAX);
	if (read(FD, In_Buffer, 256) != 256) {
		SC_INFO("unable to read onboard EEPROM");
		(void) close(FD);
		return -1;
	}

	(void) close(FD);

	Offset = 0x15;
	Length = (In_Buffer[Offset] & 0x3F);
	snprintf(Product_Name, Length + 1, "%s", &In_Buffer[Offset + 1]);
	SC_INFO("Product Name: %s", Product_Name);

	return 0;
}

int
Board_Identification(char *Board_Name)
{
	OnBoard_EEPROM_t *OnBoard_EEPROM;
	char Board_File[SYSCMD_MAX];
	char Board_Path[LSTRLEN_MAX];
	char Value[LSTRLEN_MAX];
	char Config_Var[STRLEN_MAX];
	int Found = 0;

	Plat_Devs = (Plat_Devs_t *)calloc(1, sizeof(Plat_Devs_t));

	OnBoard_EEPROM = &Common_OnBoard_EEPROM;
	SC_INFO("Accessing 'Common_OnBoard_EEPROM'");
	if (Get_Product_Name(OnBoard_EEPROM, Board_Name) != 0) {
		OnBoard_EEPROM = &Legacy_OnBoard_EEPROM;
		SC_INFO("Accessing 'Legacy_OnBoard_EEPROM'");
		if (Get_Product_Name(OnBoard_EEPROM, Board_Name) != 0) {
			SC_ERR("failed to identify the board");
			return -1;
		}
	}

	/*
	 * The default location of JSON file can be overridden by defining
	 * 'Board_Path' parameter in CONFIGFILE.
	 */
	if (Check_Config_File("Board_Path", Value, &Found) != 0) {
		return -1;
	}

	(void) strcpy(Board_Path, ((Found) ? Value : BOARD_PATH));
	SC_INFO("Board Path: %s", Board_Path);

	snprintf(Board_File, SYSCMD_MAX, "%s%s.json", Board_Path, Board_Name);
	SC_INFO("Board File: %s", Board_File);
	if (access(Board_File, F_OK) == 0) {
		/*
		 * The task to check silicon revision is enabled by default. It can be
		 * disabled by adding 'Silicon_Revision: 0' entry in CONFIGFILE.
		 */
		if (Check_Config_File("Silicon_Revision", Config_Var, &Found) != 0) {
			return -1;
		}

		/* Identify the silicon */
		if ((!Found && (strcmp(Board_Name, "VM-P-M1369-00") != 0)) ||
		    (Found && (1 <= atoi(Config_Var)))) {
			if (Silicon_Identification(Silicon_Revision,
						   STRLEN_MAX) != 0) {
				return -1;
			}
		}

		if (Parse_JSON(Board_File, Plat_Devs) != 0) {
			SC_ERR("failed to parse JSON file for board '%s'",
			       Board_Name);
			return -1;
		}

		Plat_Devs->OnBoard_EEPROM = OnBoard_EEPROM;
	} else {
		(void) strcpy(Board_Name, "Unknown");
	}

	return 0;
}

int
Silicon_Identification(char *Revision, int Length)
{
	if (Revision[0] == 0) {
		if (Get_IDCODE(Revision, Length) != 0) {
			SC_ERR("failed to get silicon revision");
			return -1;
		}

		(void) strtok(Revision, "\n");
		SC_INFO("Silicon Revision: %s", Revision);
	}

	return 0;
}

int
Shell_Execute(char *Command)
{
	FILE *FP;

	FP = popen(Command, "r");
	if (FP == NULL) {
		SC_ERR("failed to invoke '%s': %m", Command);
		return -1;
	}

	SC_INFO("Shell Command: %s", Command);
	return pclose(FP);
}

int
Access_Regulator(Voltage_t *Regulator, float *Voltage, int Access)
{
	int FD;
	char In_Buffer[STRLEN_MAX];
	char Out_Buffer[STRLEN_MAX];
	signed int Exponent;
	short Mantissa;
	int Get_Vout_Mode = 1;
	int Ret = 0;
	float Current_Voltage, New_Voltage;
	int Direction, Register;
	unsigned int Value;
	unsigned int Data_Format = 0;

	if (Regulator == NULL) {
		SC_ERR("Regulator pointer is null!");
		return -1;
	}

	/* Check if setting the requested voltage is within range */
	if ((1 == Access) && (Regulator->Minimum_Volt != -1) &&
	    (Regulator->Maximum_Volt != -1)) {
		if ((*Voltage < Regulator->Minimum_Volt) ||
		    (*Voltage > Regulator->Maximum_Volt)) {
			SC_ERR("valid voltage range is %.2f V - %.2f V",
				Regulator->Minimum_Volt, Regulator->Maximum_Volt);
			return -1;
		}
	}

	FD = open(Regulator->I2C_Bus, O_RDWR);
	if (FD < 0) {
		SC_ERR("unable to access the I2C bus %s: %m", Regulator->I2C_Bus);
		return -1;
	}

	/* Select the page, if the voltage regulator supports it */
	if (Regulator->Page_Select != -1) {
		Out_Buffer[0] = 0x0;
		Out_Buffer[1] = Regulator->Page_Select;
		SC_INFO("Write to select page: 0x%x%x", Out_Buffer[0],
			Out_Buffer[1]);
		I2C_WRITE(FD, Regulator->I2C_Address, 2, Out_Buffer, Ret);
		if (Ret != 0) {
			(void) close(FD);
			return Ret;
		}
	}

	/*
	 * Reading VOUT_MODE indicates what is READ_VOUT format and
	 * its exponent.  The default format is Linear16:
	 *
	 * Voltage =  Mantissa * 2 ^ -(Exponent)
	 */

	/* Regulators that don't support VOUT_MODE PMBus command */
	if (!Regulator->PMBus_VOUT_MODE) {
		Get_Vout_Mode = 0;
	}

	if (1 == Get_Vout_Mode) {
		Out_Buffer[0] = PMBUS_VOUT_MODE;
		(void) memset(In_Buffer, 0, STRLEN_MAX);
		I2C_READ(FD, Regulator->I2C_Address, 1, Out_Buffer, In_Buffer, Ret);
		if (Ret != 0) {
			(void) close(FD);
			return Ret;
		}

		SC_INFO("VOUT_MODE: %#x", In_Buffer[0]);
		Data_Format = ((In_Buffer[0] & 0x80) >> 7);
		Exponent = (In_Buffer[0] & 0x1F) - (sizeof(int) * 8);

	} else {
		/* For non-compliant regulators, use exponent value -8 */
		Exponent = -8;
	}

	/* Get the current VOUT */
	Out_Buffer[0] = PMBUS_READ_VOUT;
	(void) memset(In_Buffer, 0, STRLEN_MAX);
	I2C_READ(FD, Regulator->I2C_Address, 2, Out_Buffer, In_Buffer, Ret);
	if (Ret != 0) {
		(void) close(FD);
		return Ret;
	}

	Mantissa = ((unsigned char)In_Buffer[1] << 8) | (unsigned char)In_Buffer[0];
	Current_Voltage = Mantissa * pow(2, Exponent);
	SC_INFO("Current Voltage(V): %.2f, Mantissa: %#x, Exponent: %#x",
		Current_Voltage, Mantissa, Exponent);

	switch (Access) {
	case 0:
		*Voltage = Current_Voltage;
		break;
	case 1:
		/* 1: voltage is increasing, 0: voltage is decreasing */
		Direction = (*Voltage > Current_Voltage) ? 1 : 0;

		/* Disable VOUT */
		Out_Buffer[0] = PMBUS_OPERATION;
		Out_Buffer[1] = 0x0;
		SC_INFO("OPERATION: %#x %#x", Out_Buffer[0], Out_Buffer[1]);
		I2C_WRITE(FD, Regulator->I2C_Address, 2, Out_Buffer, Ret);
		if (Ret != 0) {
			(void) close(FD);
			return Ret;
		}

		/*
		 * 1: relative data format, 0: absolute data format
		 * Relative data format allows hardware to manage OV/UV limits.
		 * Absolute data format requires software to update OV/UV limits.
		 */
		if (0 == Data_Format) {
			/* Set the fault limit to +/- 10% of new VOUT */
			if (Direction) {
				New_Voltage = *Voltage + (*Voltage * 0.1);
				Register = PMBUS_VOUT_OV_FAULT_LIMIT;
			} else {
				New_Voltage = *Voltage - (*Voltage * 0.1);
				New_Voltage = (New_Voltage < 0) ? 0 : New_Voltage;
				Register = PMBUS_VOUT_UV_FAULT_LIMIT;
			}

			SC_INFO("Adjusted %svoltage Fault Limit(V):\t%.2f",
				((Direction) ? "Over" : "Under"), New_Voltage);

			/* Get the current fault limit */
			Out_Buffer[0] = Register;
			(void) memset(In_Buffer, 0, STRLEN_MAX);
			I2C_READ(FD, Regulator->I2C_Address, 2, Out_Buffer, In_Buffer, Ret);
			if (Ret != 0) {
				(void) close(FD);
				return Ret;
			}

			Mantissa = ((unsigned char)In_Buffer[1] << 8) |
					(unsigned char)In_Buffer[0];
			Current_Voltage = Mantissa * pow(2, Exponent);
			SC_INFO("Current %svoltage Fault Limit(V): %.2f, Mantissa: %#x, \
				Exponent: %#x", ((Direction) ? "Over" : "Under"),
				Current_Voltage, Mantissa, Exponent);

			/* Adjust the limit register only if it is needed */
			if (((Direction == 1) && (Current_Voltage < New_Voltage)) ||
				((Direction == 0) && (Current_Voltage > New_Voltage))) {
				Value = round(New_Voltage / pow(2, Exponent));
				SC_INFO("New %svoltage Fault Limit(V):\t%.2f\t(Reg 0x%x:\t0x%x)",
					((Direction) ? "Over" : "Under"),
					New_Voltage, Register, Value);
				Out_Buffer[0] = Register;
				Out_Buffer[1] = Value & 0xFF;
				Out_Buffer[2] = Value >> 8;
				SC_INFO("Write %svoltage Fault Limit: %#x %#x %#x",
					((Direction) ? "Over" : "Under"), Out_Buffer[0],
					Out_Buffer[1], Out_Buffer[2]);
				I2C_WRITE(FD, Regulator->I2C_Address, 3, Out_Buffer, Ret);
				if (Ret != 0) {
					(void) close(FD);
					return Ret;
				}
			}

			/* Set the warning limit to +/- 3% of new VOUT*/
			if (Direction) {
				New_Voltage = *Voltage + (*Voltage * 0.03);
				Register = PMBUS_VOUT_OV_WARN_LIMIT;
			} else {
				New_Voltage = *Voltage - (*Voltage * 0.03);
				New_Voltage = (New_Voltage < 0) ? 0 : New_Voltage;
				Register = PMBUS_VOUT_UV_WARN_LIMIT;
			}

			SC_INFO("Adjusted %svoltage Warn Limit(V):\t%.2f",
				((Direction) ? "Over" : "Under"), New_Voltage);

			/* Get the current warning limit */
			Out_Buffer[0] = Register;
			(void) memset(In_Buffer, 0, STRLEN_MAX);
			I2C_READ(FD, Regulator->I2C_Address, 2, Out_Buffer, In_Buffer, Ret);
			if (Ret != 0) {
				(void) close(FD);
				return Ret;
			}

			Mantissa = ((unsigned char)In_Buffer[1] << 8) |
					(unsigned char)In_Buffer[0];
			Current_Voltage = Mantissa * pow(2, Exponent);
			SC_INFO("Current %svoltage Warn Limit(V): %.2f, Mantissa: %#x, \
				Exponent: %#x", ((Direction) ? "Over" : "Under"),
				Current_Voltage, Mantissa, Exponent);

			/* Adjust the limit register only if it is needed */
			if (((Direction == 1) && (Current_Voltage < New_Voltage)) ||
				((Direction == 0) && (Current_Voltage > New_Voltage))) {
				Value = round(New_Voltage / pow(2, Exponent));
				SC_INFO("New %svoltage Warn Limit(V):\t%.2f\t(Reg 0x%x:\t0x%x)",
					((Direction) ? "Over" : "Under"),
					New_Voltage, Register, Value);
				Out_Buffer[0] = Register;
				Out_Buffer[1] = Value & 0xFF;
				Out_Buffer[2] = Value >> 8;
				SC_INFO("Write %svoltage Warn Limit: %#x %#x %#x",
					((Direction) ? "Over" : "Under"), Out_Buffer[0],
					Out_Buffer[1], Out_Buffer[2]);
				I2C_WRITE(FD, Regulator->I2C_Address, 3, Out_Buffer, Ret);
				if (Ret != 0) {
					(void) close(FD);
					return Ret;
				}
			}
		}

		/* Set VOUT */
		Value = round(*Voltage / pow(2, Exponent));
		SC_INFO("New Voltage(V):\t%.2f\t(Reg 0x%x:\t0x%x)", *Voltage,
			PMBUS_VOUT_COMMAND, Value);
		Out_Buffer[0] = PMBUS_VOUT_COMMAND;
		Out_Buffer[1] = Value & 0xFF;
		Out_Buffer[2] = Value >> 8;
		SC_INFO("VOUT_COMMAND: %#x %#x %#x", Out_Buffer[0],
			Out_Buffer[1], Out_Buffer[2]);
		I2C_WRITE(FD, Regulator->I2C_Address, 3, Out_Buffer, Ret);
		if (Ret != 0) {
			(void) close(FD);
			return Ret;
		}

		/* Enable VOUT */
		(void) memset(Out_Buffer, 0, STRLEN_MAX);
		Out_Buffer[0] = PMBUS_OPERATION;
		Out_Buffer[1] = 0x80;
		SC_INFO("OPERATION: %#x %#x", Out_Buffer[0], Out_Buffer[1]);
		I2C_WRITE(FD, Regulator->I2C_Address, 2, Out_Buffer, Ret);
		if (Ret != 0) {
			(void) close(FD);
			return Ret;
		}

		break;
	case 2:
		/* Get Overvoltage Fault Limit */
		Out_Buffer[0] = PMBUS_VOUT_OV_FAULT_LIMIT;
		(void) memset(In_Buffer, 0, STRLEN_MAX);
		I2C_READ(FD, Regulator->I2C_Address, 2, Out_Buffer, In_Buffer, Ret);
		if (Ret != 0) {
			(void) close(FD);
			return Ret;
		}

		Mantissa = ((unsigned char)In_Buffer[1] << 8) | (unsigned char)In_Buffer[0];
		*Voltage = Mantissa * pow(2, Exponent);
		if (1 == Data_Format) {
			/*
			 * In relative data format, value calculated from mantissa is the
			 * ratio of the voltage limit to the VOUT value. For actual voltage,
			 * multiply ratio with VOUT value.
			 */
			*Voltage = *Voltage * Current_Voltage;
		}

		SC_PRINT("Overvoltage Fault Limit(V):\t%.2f\t(Reg 0x%x:\t0x%x)",
		       *Voltage, PMBUS_VOUT_OV_FAULT_LIMIT, Mantissa);

		/* Get Overvoltage Warning Limit */
		Out_Buffer[0] = PMBUS_VOUT_OV_WARN_LIMIT;
		(void) memset(In_Buffer, 0, STRLEN_MAX);
		I2C_READ(FD, Regulator->I2C_Address, 2, Out_Buffer, In_Buffer, Ret);
		if (Ret != 0) {
			(void) close(FD);
			return Ret;
		}

		Mantissa = ((unsigned char)In_Buffer[1] << 8) | (unsigned char)In_Buffer[0];
		*Voltage = Mantissa * pow(2, Exponent);
		if (1 == Data_Format) {
			*Voltage = *Voltage * Current_Voltage;
		}

		SC_PRINT("Overvoltage Warning Limit(V):\t%.2f\t(Reg 0x%x:\t0x%x)",
		       *Voltage, PMBUS_VOUT_OV_WARN_LIMIT, Mantissa);

		/* Get Undervoltage Warning Limit */
		Out_Buffer[0] = PMBUS_VOUT_UV_WARN_LIMIT;
		(void) memset(In_Buffer, 0, STRLEN_MAX);
		I2C_READ(FD, Regulator->I2C_Address, 2, Out_Buffer, In_Buffer, Ret);
		if (Ret != 0) {
			(void) close(FD);
			return Ret;
		}

		Mantissa = ((unsigned char)In_Buffer[1] << 8) | (unsigned char)In_Buffer[0];
		*Voltage = Mantissa * pow(2, Exponent);
		if (1 == Data_Format) {
			*Voltage = *Voltage * Current_Voltage;
		}

		SC_PRINT("Undervoltage Warning Limit(V):\t%.2f\t(Reg 0x%x:\t0x%x)",
		       *Voltage, PMBUS_VOUT_UV_WARN_LIMIT, Mantissa);

		/* Get Undervoltage Fault Limit */
		Out_Buffer[0] = PMBUS_VOUT_UV_FAULT_LIMIT;
		(void) memset(In_Buffer, 0, STRLEN_MAX);
		I2C_READ(FD, Regulator->I2C_Address, 2, Out_Buffer, In_Buffer, Ret);
		if (Ret != 0) {
			(void) close(FD);
			return Ret;
		}

		Mantissa = ((unsigned char)In_Buffer[1] << 8) | (unsigned char)In_Buffer[0];
		*Voltage = Mantissa * pow(2, Exponent);
		if (1 == Data_Format) {
			*Voltage = *Voltage * Current_Voltage;
		}

		SC_PRINT("Undervoltage Fault Limit(V):\t%.2f\t(Reg 0x%x:\t0x%x)",
		       *Voltage, PMBUS_VOUT_UV_FAULT_LIMIT, Mantissa);
		break;
	default:
		SC_ERR("invalid regulator access");
		(void) close(FD);
		return -1;
	}

	(void) close(FD);
	return 0;
}

/*
 * Routine to access IO expander chip.
 *
 * Input -
 *      IO_Exp: Pointer to IO expander structure.
 *      Op:     0 for read operation, 1 for write operation.
 *      Offset: 0x2 output register offset, 0x6 direction register offset.
 *      *Data:  Pointer to output value to be written to the device.
 * Output -
 *      *Data:  Pointer to input value read from the device.
 */
int
Access_IO_Exp(IO_Exp_t *IO_Exp, int Op, int Offset, unsigned int *Data)
{
	int FD;
	char In_Buffer[STRLEN_MAX];
	char Out_Buffer[STRLEN_MAX];
	int Ret = 0;

	if (Op == 1 && *Data > ((1 << IO_Exp->Numbers) - 1)) {
		SC_ERR("invalid data, valid value is between 0 and %#x",
		       ((1 << IO_Exp->Numbers) - 1));
		return -1;
	}

	FD = open(IO_Exp->I2C_Bus, O_RDWR);
	if (FD < 0) {
		SC_ERR("unable to access I2C bus %s: %m", IO_Exp->I2C_Bus);
		return -1;
	}

	(void) memset(Out_Buffer, 0, STRLEN_MAX);
	(void) memset(In_Buffer, 0, STRLEN_MAX);
	if (Op == 0) {	// Read operation
		Out_Buffer[0] = Offset;
		I2C_READ(FD, IO_Exp->I2C_Address, 2, Out_Buffer, In_Buffer, Ret);
		if (Ret != 0) {
			(void) close(FD);
			return Ret;
		}

		SC_INFO("Read (%#x): %#x %#x", Offset, In_Buffer[0],
			In_Buffer[1]);
		*Data = ((In_Buffer[0] << 8) | In_Buffer[1]);

	} else if (Op == 1) {	// Write operation
		Out_Buffer[0] = Offset;
		Out_Buffer[1] = ((*Data >> 8) & 0xFF);
		Out_Buffer[2] = (*Data & 0xFF);
		SC_INFO("Write (%#x): %#x %#x", Offset, Out_Buffer[1],
			Out_Buffer[2]);
		I2C_WRITE(FD, IO_Exp->I2C_Address, 3, Out_Buffer, Ret);
		if (Ret != 0) {
			(void) close(FD);
			return Ret;
		}

	} else {
		SC_ERR("invalid access operation");
		(void) close(FD);
		return -1;
	}

	(void) close(FD);
	return 0;
}

int
FMC_Vadj_Range(FMC_t *FMC, float *Min_Voltage, float *Max_Voltage)
{
	char In_Buffer[SYSCMD_MAX];
	char Out_Buffer[SYSCMD_MAX];
	int FD;
	int Offset;
	int Found;
	int Ret = 0;

	/* Read FMC's EEPROM */
	FD = open(FMC->I2C_Bus, O_RDWR);
	if (FD < 0) {
		SC_ERR("unable to access I2C bus %s: %m", FMC->I2C_Bus);
		return -1;
	}

	(void) memset(Out_Buffer, 0, SYSCMD_MAX);
	(void) memset(In_Buffer, 0, SYSCMD_MAX);
	Out_Buffer[0] = 0x0;    // EEPROM offset 0
	I2C_TRY_READ(FD, FMC->I2C_Address, 0xFF, Out_Buffer, In_Buffer, Ret);
	if (Ret != 0) {
		(void) close(FD);
		return Ret;
	}

	/* Common Header offset 0x5 points to Multirecord areas */
	Offset = In_Buffer[5] * 8;

	/*
	 * 'Record Type' for DC Load is 0x2, bit 7 of 'Record Format' indicates
	 * the end of Multirecord, and don't go over amount of data read from
	 * EEPROM.
	 */ 
	Found = 0;
	while ((In_Buffer[Offset] == 0x2) &&
	       ((In_Buffer[Offset + 1] & 0x80) != 0x80) && (Offset < 0xFF)) {
		/*
		 * In Multirecord area of DC Load, 'Output Number' (Offset + 5)
		 * should have value of 0 (for Vadj).  Other values belong
		 * to other power supplies.
		 */
		if (In_Buffer[Offset + 5] == 0x0) {
			Found = 1;
			break;
		}

		/*
		 * Skip to the next DC Load record.  There are 5 bytes of
		 * header in this record plus length of data in offset 0x2.
		 */
		Offset += (5 + In_Buffer[Offset + 2]);
	}

	if (Found == 1) {
		/* Unit of reading is per 10mV */
		*Min_Voltage = (float)((In_Buffer[Offset + 9] << 8) |
					In_Buffer[Offset + 8]) / 100;
		*Max_Voltage = (float)((In_Buffer[Offset + 11] << 8) |
					In_Buffer[Offset + 10]) / 100;
	} else {
		*Min_Voltage = *Max_Voltage = 0;
	}

	SC_INFO("Min Voltage: %.2f, Max Voltage: %.2f", *Min_Voltage,
		*Max_Voltage);

	return 0;
}

int
FMCAutoVadj_Op(void)
{
	int Target_Index = -1;
	IO_Exp_t *IO_Exp;
	unsigned int Value;
	Voltages_t *Voltages;
	Voltage_t *Regulator;
	FMCs_t *FMCs;
	FMC_t *FMC;
	int Present[2] = { 0 };
	float Voltage = 0;
	float Min_Volt[2] = { 0 };
	float Max_Volt[2] = { 0 };
	float Min_Combined = 0;
	float Max_Combined = 0;
	int Legacy_Approach = 0;
	int State;
	char *Regulator_Name;
	float Target_Volt, Default_Volt;

	FMCs = Plat_Devs->FMCs;
	if (FMCs == NULL) {
		SC_INFO("FMC operation is not supported");
		return 0;
	}

	/* Current boards support up to 2 FMC modules */
	if (FMCs->Numbers > 2) {
		SC_ERR("unsupported number of FMC modules");
		return -1;
	}

	/* Determine if gpiod knows about 'presence' lines */
	for (int i = 0; i < FMCs->Numbers; i++) {
		FMC = &FMCs->FMC[i];
		for (int j = 0; j < FMC->Label_Numbers; j++) {
			if (Get_GPIO(FMC->Presence_Labels[j], &State) != 0) {
				Legacy_Approach = 1;
				break;
			}
		}
	}

	if (Legacy_Approach == 1) {
		SC_INFO("Read IO Expander to determine FMC presence");
		IO_Exp = Plat_Devs->IO_Exp;
		if (Access_IO_Exp(IO_Exp, 0, 0x0, &Value) != 0) {
			SC_ERR("failed to read input of IO Expander");
			return -1;
		}

		SC_INFO("IO Expander input: %#x", Value);

		/*
		 * Presence of FMC modules are detected by input lines connected
		 * to pins 13 & 14 of TCA6416A.  If direction of these pins (Directions
		 * index 15 & 14) are configured as input and they are driven low
		 * (reading value 0), then the FMC module is present.
		 */
		if ((IO_Exp->Directions[15] == 1) && ((~Value & 0x1) == 0x1)) {
			SC_INFO("FMC 0 is present");
			Present[0] = 1;
		}

		if ((IO_Exp->Directions[14] == 1) && ((~Value & 0x2) == 0x2)) {
			SC_INFO("FMC 1 is present");
			Present[1] = 1;
		}
	} else {
		SC_INFO("Read GPIO line to determine FMC presence");
		for (int i = 0; i < FMCs->Numbers; i++) {
			FMC = &FMCs->FMC[i];
			if (Get_GPIO(FMC->Presence_Labels[0], &State) != 0) {
				SC_ERR("failed to determine presence of FMC %d", i);
				return -1;
			}

			/* Presence line is active low */
			Present[i] = !State;
			SC_INFO("FMC %d is %spresent", i, (Present[i] ? "" : "not "));
		}
	}

	/*
	 * Assumption here is that both FMC modules are powered by the same
	 * voltage regulator and with the same voltage requirement.
	 */
	Regulator_Name = FMCs->FMC[0].Voltage_Regulator;
	Default_Volt = FMCs->FMC[0].Default_Volt;

	for (int i = 0; i < FMCs->Numbers; i++) {
		if (Present[i] == 1) {
			FMC = &FMCs->FMC[i];
			if (FMC_Vadj_Range(FMC, &Min_Volt[i], &Max_Volt[i]) != 0) {
				SC_PRINT("WARNING: unable to obtain voltage range "
					 "from FMC %d.  Voltage regulator '%s' "
					 "needs to be set manually.", i, Regulator_Name);
				return 0;
			}
		}
	}

	if (Present[0] == 1 && Present[1] == 0) {
		Min_Combined = Min_Volt[0];
		Max_Combined = Max_Volt[0];
	} else if (Present[0] == 0 && Present[1] == 1) {
		Min_Combined = Min_Volt[1];
		Max_Combined = Max_Volt[1];
	} else if (Present[0] == 1 && Present[1] == 1) {
		Min_Combined = MAX(Min_Volt[0], Min_Volt[1]);
		Max_Combined = MIN(Max_Volt[0], Max_Volt[1]);
	}

	SC_INFO("Combined Min: %.2f, Combined Max: %.2f",
		Min_Combined, Max_Combined);

	/*
	 * Both FMC modules are constrained to the same target voltage
	 * restriction defined by the board.
	 */
	for (int i = 0; i < FMCs->FMC[0].Volt_Numbers; i++) {
		Target_Volt = FMCs->FMC[0].Supported_Volts[i];
		if (Min_Combined <= Target_Volt && Max_Combined >= Target_Volt) {
			Voltage = Target_Volt;
		}
	}

	/*
	 * If no FMC module is present, or voltage range that is read from the
	 * modules don't satisfy any of target voltages supported by the board,
	 * then set the regulator to the default voltage level.
	 */
	if ((Present[0] == 0 && Present[1] == 0) || (Voltage == 0)) {
		Voltage = Default_Volt;
	}

	Voltages  = Plat_Devs->Voltages;
	for (int i = 0; i < Voltages->Numbers; i++) {
		if (strcmp(Regulator_Name, Voltages->Voltage[i].Name) == 0) {
			Target_Index = i;
			Regulator = &Voltages->Voltage[Target_Index];
			break;
		}
	}

	if (Target_Index == -1) {
		SC_ERR("no regulator exists for %s", Regulator_Name);
		return -1;
	}

	SC_INFO("Set %s voltage regulator to %.2f volts", Regulator_Name, Voltage);
	if (Access_Regulator(Regulator, &Voltage, 1) != 0) {
		SC_ERR("failed to set voltage of %s regulator", Regulator_Name);
	}

	return 0;
}

int
Get_GPIO(char *Label, int *State)
{
	FILE *FP;
	char Chip_Name[STRLEN_MAX];
	char Buffer[SYSCMD_MAX];
	char Output[STRLEN_MAX] = {'\0'};
	unsigned int Line_Offset;

	if (gpiod_ctxless_find_line(Label, Chip_Name, STRLEN_MAX,
	    &Line_Offset) != 1) {
		SC_INFO("failed to find GPIO line %s", Label);
		return -1;
	}

	(void) sprintf(Buffer, "gpioget %s %u 2>&1", Chip_Name, Line_Offset);
	SC_INFO("Command: %s", Buffer);
	FP = popen(Buffer, "r");
	if (FP == NULL) {
		SC_ERR("failed to start process '%s': %m", Buffer);
		return -1;
	}

	if (fgets(Output, sizeof(Output), FP) == NULL) {
		SC_ERR("failed to get the state of GPIO line '%s'", Label);
		(void) pclose(FP);
		return -1;
	}

	if (pclose(FP) != 0) {
		SC_ERR("failed to get the state of GPIO line '%s'", Label);
		return -1;
	}

	SC_INFO("Output: %s", Output);
	if ((strcmp(Output, "0\n") != 0) && (strcmp(Output, "1\n") != 0)) {
		SC_ERR("invalid output %s", Output);
		return -1;
	}

	*State = atoi(Output);
	return 0;
}

int
Set_GPIO(char *Label, int State)
{
	char Chip_Name[STRLEN_MAX];
	char Buffer[SYSCMD_MAX];
	unsigned int Line_Offset;

	if (gpiod_ctxless_find_line(Label, Chip_Name, STRLEN_MAX,
	    &Line_Offset) != 1) {
		SC_ERR("failed to find GPIO line");
		return -1;
	}

	(void) sprintf(Buffer, "gpioset %s %u=%d 2>&1", Chip_Name, Line_Offset,
		       State);
	if (Shell_Execute(Buffer) != 0) {
		SC_ERR("failed to set GPIO line '%s': %m", Label);
		return -1;
	}

	return 0;
}

int
EEPROM_Common(char *Buffer)
{
	SC_PRINT("0x00 - Version:\t%.2x", Buffer[0x0]);
	SC_PRINT("0x01 - Internal User Area:\t%.2x", Buffer[0x1]);
	SC_PRINT("0x02 - Chassis Info Area:\t%.2x", Buffer[0x2]);
	SC_PRINT("0x03 - Board Area:\t%.2x", Buffer[0x3]);
	SC_PRINT("0x04 - Product Info Area:\t%.2x", Buffer[0x4]);
	SC_PRINT("0x05 - Multi Record Area:\t%.2x", Buffer[0x5]);
	SC_PRINT("0x06 - Pad and Check sum:\t%.2x %.2x", Buffer[0x6],
		 Buffer[0x7]);
	return 0;
}

int
EEPROM_Board(char *Buffer, int PCIe)
{
	char Buf[STRLEN_MAX];
	struct tm BuildDate = { 0 };
	time_t Time;
	int Offset, Length;

	SC_PRINT("0x08 - Version:\t%.2x", Buffer[0x8]);
	SC_PRINT("0x09 - Length:\t%.2x", Buffer[0x9]);
	SC_PRINT("0x0A - Language Code:\t%.2x", Buffer[0xA]);

	/* Base build date for manufacturing is 1/1/1996 */
	BuildDate.tm_year = 96;
	BuildDate.tm_mday = 1;
	BuildDate.tm_min = (Buffer[0xD] << 16 | Buffer[0xC] << 8 |
			    Buffer[0xB]);
	Time = mktime(&BuildDate);
	if (Time == -1) {
		SC_ERR("invalid manufacturing date");
		return -1;
	}

	SC_PRINT_N("0x0B - Manufacturing Date:\t%s", ctime(&Time));
	Offset = 0xE;
	Length = (Buffer[Offset] & 0x3F);
	snprintf(Buf, Length + 1, "%s", &Buffer[Offset + 1]);
	SC_PRINT("0x%.2x - Manufacturer:\t%s", (Offset + 1), Buf);
	Offset = Offset + Length + 1;
	Length = (Buffer[Offset] & 0x3F);
	snprintf(Buf, Length + 1, "%s", &Buffer[Offset + 1]);
	SC_PRINT("0x%.2x - Product Name:\t%s", (Offset + 1), Buf);
	Offset = Offset + Length + 1;
	Length = (Buffer[Offset] & 0x3F);
	snprintf(Buf, Length + 1, "%s", &Buffer[Offset + 1]);
	SC_PRINT("0x%.2x - Serial Number:\t%s", (Offset + 1), Buf);
	Offset = Offset + Length + 1;
	Length = (Buffer[Offset] & 0x3F);
	snprintf(Buf, Length + 1, "%s", &Buffer[Offset + 1]);
	SC_PRINT("0x%.2x - Part Number:\t%s", (Offset + 1), Buf);
	Offset = Offset + Length + 1;
	Length = (Buffer[Offset] & 0x3F);
	if (Length == 1) {
		SC_PRINT("0x%.2x - FRU ID:\t%.2x", (Offset + 1), Buffer[Offset + 1]);
	} else {
		snprintf(Buf, Length + 1, "%s", &Buffer[Offset + 1]);
		SC_PRINT("0x%.2x - FRU ID:\t%s", (Offset + 1), Buf);
		Offset = Offset + Length + 1;
		if (Buffer[Offset] != 0xC1) {
			SC_ERR("End-of-Record was not found");
			return -1;
		} else {
			SC_PRINT("0x%.2x - EoR:\t%.2x", Offset, Buffer[Offset]);
			return 0;
		}
	}

	Offset = Offset + Length + 1;
	Length = (Buffer[Offset] & 0x3F);
	snprintf(Buf, Length + 1, "%s", &Buffer[Offset + 1]);
	SC_PRINT("0x%.2x - Revision:\t%s", (Offset + 1), Buf);
	Offset = Offset + Length + 1;
	/* Non-PCIe boards have a 'End of Field' value (0xC1) at this Offset */
	if ((PCIe == 1) && (Buffer[Offset] != 0xC1)) {
		Length = (Buffer[Offset] & 0x3F);
		SC_PRINT_N("0x%.2x - PCIe Info:\t", (Offset + 1));
		for (int i = 0; i < Length; i++) {
			SC_PRINT_N("%.2x", Buffer[Offset + i + 1]);
		}

		SC_PRINT_N("\n");
		Offset = Offset + Length + 1;
		Length = (Buffer[Offset] & 0x3F);
		SC_PRINT_N("0x%.2x - UUID:\t", (Offset + 1));
		for (int i = 0; i < Length; i++) {
			SC_PRINT_N("%.2x", Buffer[Offset + i + 1]);
			if (i == 3 || i == 5 || i == 7 || i == 9) {
				SC_PRINT_N("-");
			}
		}

		SC_PRINT_N("\n");
		Offset = Offset + Length + 1;
		SC_PRINT("0x%.2x - EoR and Check sum:\t%.2x %.2x", Offset,
		       Buffer[Offset], Buffer[Offset + 1]);
	} else {
		SC_PRINT("0x%.2x - EoR, Pad, Check sum:\t%.2x %.2x%.2x %.2x",
		       Offset, Buffer[Offset], Buffer[Offset + 1],
		       Buffer[Offset + 2], Buffer[Offset + 3]);
	}

	return 0;
}

int Print_Filter;

#define DC_OUTPUT	0x1
#define DC_LOAD		0x2
#define OEM_D2		0xD2
#define OEM_D3		0xD3
#define OEM_VITA_57_1	0xFA
#define SC_PRINT_F(msg, ...) \
	if (!Print_Filter) { \
		SC_PRINT(msg, ##__VA_ARGS__); \
	}

int
EEPROM_MultiRecord(char *Buffer, int MAC_Address)
{
	int Offset;
	int Type;
	int Last_Record;
	int Length;

	Print_Filter = (MAC_Address) ? 1 : 0;

	/* Common Header offset 0x5 points to Multirecord areas */
	Offset = Buffer[5] * 8;

	/*
	 * XXX - Some early VCK190/VMK180 boards have incorrect offset
	 * value programmed.  If 'Type' is not one of the expected codes
	 * for 'Multi Record Area' field, adjust the offset to reach to
	 * the correct area.
	 */
	Type = Buffer[Offset];
	if (!(Type == DC_OUTPUT || Type == DC_LOAD || Type == OEM_D2 ||
	      Type == OEM_D3 || Type == OEM_VITA_57_1)) {
		Offset = 0x68;
	}

	do {
		Type = Buffer[Offset];
		Last_Record = Buffer[Offset + 1] & 0x80;
		switch (Type) {
		case DC_OUTPUT:
			SC_PRINT_F("0x%.2x - Record Type:\t%.2x (DC Output)", Offset, Type);
			break;
		case DC_LOAD:
			SC_PRINT_F("0x%.2x - Record Type:\t%.2x (DC Load)", Offset, Type);
			break;
		case OEM_D2:
			SC_PRINT_F("0x%.2x - Record Type:\t%.2x (Mac ID)", Offset, Type);
			break;
		case OEM_D3:
			SC_PRINT_F("0x%.2x - Record Type:\t%.2x (Memory)", Offset, Type);
			break;
		case OEM_VITA_57_1:
			SC_PRINT_F("0x%.2x - Record Type:\t%.2x (Vita 57.1)", Offset, Type);
			break;
		default:
			SC_ERR("unsupported multirecord type");
			return -1;
		}

		SC_PRINT_F("0x%.2x - Record Format:\t%.2x", (Offset + 1),
			   Buffer[Offset + 1]);
		SC_PRINT_F("0x%.2x - Length:\t%.2x", (Offset + 2),
			   Buffer[Offset + 2]);
		Length = Buffer[Offset + 2];
		SC_PRINT_F("0x%.2x - Record Check sum:\t%.2x", (Offset + 3),
			   Buffer[Offset + 3]);
		SC_PRINT_F("0x%.2x - Header Check sum:\t%.2x", (Offset + 4),
			   Buffer[Offset + 4]);
		if (Type == OEM_D2 || Type == OEM_D3) {
			SC_PRINT_F("0x%.2x - Xilinx IANA ID:\t%.2x%.2x%.2x", (Offset + 5),
				   Buffer[Offset + 5], Buffer[Offset + 6], Buffer[Offset + 7]);
		}

		switch (Type) {
		case DC_OUTPUT:
			SC_PRINT_F("0x%.2x - Output Number:\t%.2x (Power Rail)",
				   (Offset + 5), Buffer[Offset + 5]);
			SC_PRINT_F("0x%.2x - Nominal Voltage:\t%.2x%.2x (%.2fV)",
				   (Offset + 6), Buffer[Offset + 6], Buffer[Offset + 7],
				   (float)(Buffer[Offset + 7] << 8 | Buffer[Offset + 6]) / 100.0);
			SC_PRINT_F("0x%.2x - Spec'd Min Voltage:\t%.2x%.2x (%.2fV)",
				   (Offset + 8), Buffer[Offset + 8], Buffer[Offset + 9],
				   (float)(Buffer[Offset + 9] << 8 | Buffer[Offset + 8]) / 100.0);
			SC_PRINT_F("0x%.2x - Spec'd Max Voltage:\t%.2x%.2x (%.2fV)",
				   (Offset + 10), Buffer[Offset + 10], Buffer[Offset + 11],
				   (float)(Buffer[Offset + 11] << 8 | Buffer[Offset + 10]) / 100.0);
			SC_PRINT_F("0x%.2x - Spec'd Ripple Noise:\t%.2x%.2x (%dmV)",
				   (Offset + 12), Buffer[Offset + 12], Buffer[Offset + 13],
				   (Buffer[Offset + 13] << 8 | Buffer[Offset + 12]));
			SC_PRINT_F("0x%.2x - Min Current Load:\t%.2x%.2x (%dmA)",
				   (Offset + 14), Buffer[Offset + 14], Buffer[Offset + 15],
				   (Buffer[Offset + 15] << 8 | Buffer[Offset + 14]));
			SC_PRINT_F("0x%.2x - Max Current Load:\t%.2x%.2x (%dmA)",
				   (Offset + 16), Buffer[Offset + 16], Buffer[Offset + 17],
				   (Buffer[Offset + 17] << 8 | Buffer[Offset + 16]));
			break;
		case DC_LOAD:
			if (Buffer[Offset + 5] == 0x0) {
				SC_PRINT_F("0x%.2x - Output Number:\t%.2x (Voltage Adjust)",
					   (Offset + 5), Buffer[Offset + 5]);
			} else if (Buffer[Offset + 5] <= 0xF) {
				SC_PRINT_F("0x%.2x - Output Number:\t%.2x (Power Rail)",
					   (Offset + 5), Buffer[Offset + 5]);
			} else {
				SC_ERR("unsupported DC Load output number");
				return -1;
			}

			SC_PRINT_F("0x%.2x - Nominal Voltage:\t%.2x%.2x (%.2fV)",
				   (Offset + 6), Buffer[Offset + 6], Buffer[Offset + 7],
				   (float)(Buffer[Offset + 7] << 8 | Buffer[Offset + 6]) / 100.0);
			SC_PRINT_F("0x%.2x - Spec'd Min Voltage:\t%.2x%.2x (%.2fV)",
				   (Offset + 8), Buffer[Offset + 8], Buffer[Offset + 9],
				   (float)(Buffer[Offset + 9] << 8 | Buffer[Offset + 8]) / 100.0);
			SC_PRINT_F("0x%.2x - Spec'd Max Voltage:\t%.2x%.2x (%.2fV)",
				   (Offset + 10), Buffer[Offset + 10], Buffer[Offset + 11],
				   (float)(Buffer[Offset + 11] << 8 | Buffer[Offset + 10]) / 100.0);
			SC_PRINT_F("0x%.2x - Spec'd Ripple Noise:\t%.2x%.2x (%dmV)",
				   (Offset + 12), Buffer[Offset + 12], Buffer[Offset + 13],
				   (Buffer[Offset + 13] << 8 | Buffer[Offset + 12]));
			SC_PRINT_F("0x%.2x - Min Current Load:\t%.2x%.2x (%dmA)",
				   (Offset + 14), Buffer[Offset + 14], Buffer[Offset + 15],
				   (Buffer[Offset + 15] << 8 | Buffer[Offset + 14]));
			SC_PRINT_F("0x%.2x - Max Current Load:\t%.2x%.2x (%dmA)",
				   (Offset + 16), Buffer[Offset + 16], Buffer[Offset + 17],
				   (Buffer[Offset + 17] << 8 | Buffer[Offset + 16]));
			break;
		case OEM_D2:
			if (Buffer[Offset + 8] == 0x11) {
				SC_PRINT_F("0x%.2x - Version Number:\t%.2x (SC Mac ID)",
					   (Offset + 8), Buffer[Offset + 8]);
				SC_PRINT_F("0x%.2x - Mac ID 0:\t%.2x:%.2x:%.2x:%.2x:%.2x:%.2x",
					   (Offset + 9), Buffer[Offset + 9],
					   Buffer[Offset + 10], Buffer[Offset + 11],
					   Buffer[Offset + 12], Buffer[Offset + 13],
					   Buffer[Offset + 14]);
				if (MAC_Address) {
					SC_PRINT("SC MAC: %.2x:%.2x:%.2x:%.2x:%.2x:%.2x",
						 Buffer[Offset + 9], Buffer[Offset + 10],
						 Buffer[Offset + 11], Buffer[Offset + 12],
						 Buffer[Offset + 13], Buffer[Offset + 14]);
				}

			} else if (Buffer[Offset + 8] == 0x31) {
				SC_PRINT_F("0x%.2x - Version Number:\t%.2x (Veral Mac ID)",
					   (Offset + 8), Buffer[Offset + 8]);
				SC_PRINT_F("0x%.2x - Mac ID 0:\t%.2x:%.2x:%.2x:%.2x:%.2x:%.2x",
					   (Offset + 9), Buffer[Offset + 9],
					   Buffer[Offset + 10], Buffer[Offset + 11],
					   Buffer[Offset + 12], Buffer[Offset + 13],
					   Buffer[Offset + 14]);
				if (MAC_Address) {
					SC_PRINT("Versal MAC%s: %.2x:%.2x:%.2x:%.2x:%.2x:%.2x",
						 ((Length > 0xA) ? " 1" : ""),
						 Buffer[Offset + 9], Buffer[Offset + 10],
						 Buffer[Offset + 11], Buffer[Offset + 12],
						 Buffer[Offset + 13], Buffer[Offset + 14]);
				}

				if (Length > 0xA) {
					SC_PRINT_F("0x%.2x - Mac ID 1:\t%.2x:%.2x:%.2x:%.2x:%.2x:%.2x",
						   (Offset + 15), Buffer[Offset + 15],
						   Buffer[Offset + 16], Buffer[Offset + 17],
						   Buffer[Offset + 18], Buffer[Offset + 19],
						   Buffer[Offset + 20]);
					if (MAC_Address) {
						SC_PRINT("Versal MAC 2: %.2x:%.2x:%.2x:%.2x:%.2x:%.2x",
							 Buffer[Offset + 15], Buffer[Offset + 16],
							 Buffer[Offset + 17], Buffer[Offset + 18],
							 Buffer[Offset + 19], Buffer[Offset + 20]);
					}
				}

			} else {
				SC_ERR("unsupported D2 version number");
				return -1;
			}

			break;
		case OEM_D3:
			SC_PRINT_F("0x%.2x - Memory Type:\t%s", (Offset + 8),
				   &Buffer[Offset + 8]);
			Length = strlen(&Buffer[Offset + 8]) + 1;
			SC_PRINT_F("0x%.2x - Voltage Supply:\t%s", (Offset + 8 + Length),
				   &Buffer[Offset + 8 + Length]);
			break;
		case OEM_VITA_57_1:
			SC_PRINT_F("0x%.2x - Organizationally Unique Identifier:\t%.2x%.2x%.2x",
				   (Offset + 5), Buffer[Offset + 5], Buffer[Offset + 6],
				   Buffer[Offset + 7]);
			SC_PRINT_F("0x%.2x - Subtype Version:\t%.2x", (Offset + 8),
				   Buffer[Offset + 8]);
			SC_PRINT_F("0x%.2x - Connector Type:\t%.2x", (Offset + 9),
				   Buffer[Offset + 9]);
			SC_PRINT_F("0x%.2x - P1 Bank A Number Signals:\t%.2x", (Offset + 10),
				   Buffer[Offset + 10]);
			SC_PRINT_F("0x%.2x - P1 Bank B Number Signals:\t%.2x", (Offset + 11),
				   Buffer[Offset + 11]);
			SC_PRINT_F("0x%.2x - P2 Bank A Number Signals:\t%.2x", (Offset + 12),
				   Buffer[Offset + 12]);
			SC_PRINT_F("0x%.2x - P2 Bank B Number Signals:\t%.2x", (Offset + 13),
				   Buffer[Offset + 13]);
			SC_PRINT_F("0x%.2x - P1 GBT B Number Signals:\t%.2x", (Offset + 14),
				   Buffer[Offset + 14]);
			SC_PRINT_F("0x%.2x - Max Clock for TCK:\t%.2x (%dMhz)", (Offset + 15),
				   Buffer[Offset + 15], Buffer[Offset + 15]);
			break;
		default:
			SC_ERR("unsupported multirecord type");
			return -1;
		}

		if (!Last_Record) {
			/*
			 * Skip to the next multi-record.  There are 5 bytes of
			 * header in this record plus length of data in offset 0x2.
			 */
			Offset += (5 + Buffer[Offset + 2]);
			SC_PRINT_F(" ");
		}

	} while (!Last_Record);

	return 0;
}

int
Get_Measured_Clock(char *Counter_Reg, char *Label)
{
	char TCL_Path[SYSCMD_MAX], TCL_Args[STRLEN_MAX];
	char Output[STRLEN_MAX] = { 0 };

	(void) sprintf(TCL_Path, "%s%s", SCRIPT_PATH, TCL_CMD_TCL);
	(void) sprintf(TCL_Args, "%s %s %s", Board_Name, READ_CLOCK_CMD, Counter_Reg);
	if (XSDB_Op(TCL_Path, TCL_Args, Output, sizeof(Output)) != 0) {
		SC_ERR("failed to get measured clock");
		return -1;
	}

	SC_PRINT("%s%.3f", Label, atof(Output));
	return 0;
}

static int
Check_Clock_Files(char *Clock_Files, char *TCS_File, char *TXT_File, char *BIN_File)
{
	char TCS[SYSCMD_MAX];
	char TXT[SYSCMD_MAX];
	char BIN[SYSCMD_MAX] = { 0 };
	char Buffer[SYSCMD_MAX];
	char Temp_Buffer[SYSCMD_MAX];

	/* If there is any Carriage Return at the end, remove it */
	(void) strcpy(Temp_Buffer, strtok(Clock_Files, "\n"));

	(void) strcpy(Buffer, strtok(Temp_Buffer, " "));
	if (strstr(Buffer, ".tcs") != NULL) {
		(void) strcpy(TCS, Buffer);
		(void) strcpy(Buffer, strtok(NULL, " "));
		if (strstr(Buffer, ".txt") != NULL) {
			(void) strcpy(TXT, Buffer);
		} else {
			SC_ERR("missing '.txt' clock file");
			return -1;
		}

		(void) sprintf(TCS_File, "%s%s", CFS_PATH, TCS);
		(void) sprintf(TXT_File, "%s%s", CFS_PATH, TXT);
	} else {
		(void) sprintf(TCS, "%s.tcs", Buffer);
		(void) sprintf(TXT, "%s.txt", Buffer);
		(void) sprintf(BIN, "%s.bin", Buffer);
		(void) sprintf(TCS_File, "%s%s", CFS_PATH, TCS);
		(void) sprintf(TXT_File, "%s%s", CFS_PATH, TXT);
		(void) sprintf(BIN_File, "%s%s", CFS_PATH, BIN);
	}

	if (access(TCS_File, F_OK) != 0) {
		(void) sprintf(TCS_File, "%s%s", CUSTOM_CFS_PATH, TCS);
		if (access(TCS_File, F_OK) != 0) {
			SC_ERR("file %s doesn't exist: %m", TCS);
			return -1;
		}
	}

	if (access(TXT_File, F_OK) != 0) {
		(void) sprintf(TXT_File, "%s%s", CUSTOM_CFS_PATH, TXT);
		if (access(TXT_File, F_OK) != 0) {
			SC_ERR("file %s doesn't exist: %m", TXT);
			return -1;
		}
	}

	if (BIN[0] != '\0') {
		if (access(BIN_File, F_OK) != 0) {
			(void) sprintf(BIN_File, "%s%s", CUSTOM_CFS_PATH, BIN);
			if (access(BIN_File, F_OK) != 0) {
				SC_INFO("file %s doesn't exist: %m", BIN);
				BIN_File[0] = '\0';
			}
		}
	}

	SC_INFO("TCS file: %s", TCS_File);
	SC_INFO("TXT file: %s", TXT_File);
	SC_INFO("BIN file: %s", BIN_File);

	return 0;
}

#define EEPROM_IDT_8A34001_Program(c, b)	EEPROM_IDT_8A34001(c, b, 0)
#define EEPROM_IDT_8A34001_Verify(c, b)		EEPROM_IDT_8A34001(c, b, 1)

int
EEPROM_IDT_8A34001(Clock_t *Clock, char *BIN_File, int Verify)
{
	FILE *FP;
	char Buffer[SYSCMD_MAX];
	char Arg[STRLEN_MAX];
	char Message[STRLEN_MAX];
	char *Bus;

	(void) sprintf(Buffer, "%s%s", SCRIPT_PATH, PROGRAM_8A34001);
	if (access(Buffer, F_OK) != 0) {
		SC_ERR("failed to access file %s: %m", Buffer);
		return -1;
	}

	if (Verify) {
		(void) strcpy(Arg, "-v");
		(void) strcpy(Message, "Verification Complete");
	} else {
		(void) strcpy(Arg, "");
		(void) strcpy(Message, "Programming Complete");
	}

	Bus = strtok(Clock->I2C_Bus, "/dev/i2c-");
	(void) sprintf(Buffer, "cd %s; python3 %s -f %s -b %i -d %i %s",
		       SCRIPT_PATH, PROGRAM_8A34001, BIN_File, atoi(Bus),
		       Clock->I2C_Address, Arg);
	SC_INFO("Command: %s", Buffer);
	FP = popen(Buffer, "r");
	if (FP == NULL) {
		SC_ERR("failed to invoke %s: %m", Buffer);
		return -1;
	}

	while (fgets(Buffer, sizeof(Buffer), FP)) {
		if (strstr(Buffer, Message) != NULL) {
			SC_INFO("Got '%s' output for '%s'", Message, BIN_File);
		}
	}

	return pclose(FP);
}

int
Get_IDT_8A34001(Clock_t *Clock)
{
	DIR *DP;
	FILE *FP;
	char Buffer[SYSCMD_MAX];
	char Label[SYSCMD_MAX];
	char Frequency[SYSCMD_MAX];
	IDT_8A34001_Data_t *Clock_Data;
	char Clock_File[LSTRLEN_MAX];
	char TCS_File[SYSCMD_MAX];
	char TXT_File[SYSCMD_MAX];
	char BIN_File[SYSCMD_MAX] = { 0 };
	int Found = 0;
	struct dirent *File;
	char CFS_Dirs[][LSTRLEN_MAX] = { CFS_PATH, CUSTOM_CFS_PATH };

	if (Clock->Type_Data == NULL) {
		SC_ERR("no data is available for 8A34001 clock");
		return -1;
	}

	Clock_Data = (IDT_8A34001_Data_t *)Clock->Type_Data;

	/*
	 * If there is no '8A34001' file, that means the chip has not been
	 * programmed with a design since boot time.
	 */
	if (access(IDT8A34001FILE, F_OK) != 0) {
		/*
		 * If the content of EEPROM is blank, it indicates that the clock
		 * outputs are 0.
		 */
		(void) sprintf(BIN_File, "%s%s", CFS_PATH, "blank_image");
		if (EEPROM_IDT_8A34001_Verify(Clock, BIN_File) == 0) {
			for (int i = 0; i < Clock_Data->Number_Label; i++) {
				SC_PRINT("%s:\t0 MHz/PPS", Clock_Data->Display_Label[i]);
			}

			SC_PRINT("\nNOTE: these frequencies represent the last " \
				 "clock set operation and not the actual output " \
				 "frequencies generated by the device.\n");

			return 0;
		}

		/*
		 * The content EEPROM is not blank so determine which image it holds.
		 */
		for (int i = 0; i < (sizeof(CFS_Dirs)/sizeof(CFS_Dirs[0])) && !Found; i++) {
			DP = opendir(CFS_Dirs[i]);
			if (DP == NULL) {
				SC_ERR("failed to open directory: %m");
				return -1;
			}

			File = readdir(DP);
			while (File != NULL) {
				if (strstr(File->d_name, ".bin") != NULL) {
					(void) sprintf(BIN_File, "%s%s", CFS_Dirs[i], File->d_name);
					if (EEPROM_IDT_8A34001_Verify(Clock, BIN_File) == 0) {
						(void) sprintf(Clock_File, "%s", strtok(File->d_name, ".bin"));
						if (Set_IDT_8A34001(Clock, Clock_File, 0) != 0) {
							SC_ERR("failed to configure 8A34001");
							(void) closedir(DP);
							return -1;
						}

						Found = 1;
						break;
					}
				}

				File = readdir(DP);
			}

			(void) closedir(DP);
		}

		if (!Found) {
			SC_ERR("failed to get clock information");
			return -1;
		}
	}

	FP = fopen(IDT8A34001FILE, "r");
	if (FP == NULL) {
		SC_ERR("failed to open file %s: %m", IDT8A34001FILE);
		return -1;
	}

	if (fgets(Clock_File, sizeof(Clock_File), FP) == NULL) {
		SC_ERR("failed to read file %s: %m", IDT8A34001FILE);
		(void) fclose(FP);
		return -1;
	}

	(void) fclose(FP);
	SC_INFO("File '%s' contains '%s'", IDT8A34001FILE, Clock_File);

	if (Check_Clock_Files(Clock_File, TCS_File, TXT_File, BIN_File) != 0) {
		return -1;
	}

	FP = fopen(TCS_File, "r");
	if (FP == NULL) {
		SC_ERR("failed to open %s: %m", TCS_File);
		return -1;
	}

	while (fgets(Buffer, sizeof(Buffer), FP)) {
		if ((strstr(Buffer, "_Frequency:") == NULL) &&
		    (strstr(Buffer, "DesiredFrequency:") == NULL)) {
			continue;
		}

		(void) strncpy(Label, strtok(Buffer, ":"), (sizeof(Label) - 1));
		(void) strncpy(Frequency, strtok(NULL, "|"), (sizeof(Frequency) - 1));
		for (int i = 0; i < Clock_Data->Number_Label; i++) {
			if (strcmp(Label, Clock_Data->Internal_Label[i]) == 0) {
				if ((strcmp(Frequency, " ") != 0) &&
				    !((strstr(Frequency, "MHz") != NULL) ||
				     (strstr(Frequency, "PPS") != NULL))) {
					SC_PRINT("%s:\t%sMHz", Clock_Data->Display_Label[i],
					       Frequency);
				} else {
					SC_PRINT("%s:\t%s", Clock_Data->Display_Label[i],
					       Frequency);
				}

				break;
			}
		}
	}

	SC_PRINT("\nNOTE: these frequencies represent the last clock set operation " \
		 "and not the actual output frequencies generated by the device.\n");

	return 0;
}

int
Get_Measured_IDT_8A34001(Clock_t *Clock)
{
	char Label[STRLEN_MAX];
	IDT_8A34001_Data_t *Clock_Data;

	if (Clock->Type_Data == NULL) {
		SC_ERR("no data is available for 8A34001 clock");
		return -1;
	}

	Clock_Data = (IDT_8A34001_Data_t *)Clock->Type_Data;
	/* The IDT_8A34001 clock chip has 12 outputs */
	for (int i = 0; i < 12; i++) {
		if (Clock_Data->FPGA_Counter_Reg[i][0] != '\0') {
			(void) sprintf(Label, "Q%d - Frequency(MHz):\t", i);
			if (Get_Measured_Clock(Clock_Data->FPGA_Counter_Reg[i], Label) != 0) {
				SC_ERR("failed to get measured clock");
				return -1;
			}
		} else {
			SC_PRINT("Q%d - Not Available", i);
		}
	}

	return 0;
}

int
Set_IDT_8A34001(Clock_t *Clock, char *Clock_Files, int Mode)
{
	FILE *FP;
	int FD;
	char Buffer[SYSCMD_MAX];
	unsigned int Size;
	char Data_String[SYSCMD_MAX];
	char Data[SYSCMD_MAX];
	char TCS_File[SYSCMD_MAX];
	char TXT_File[SYSCMD_MAX];
	char BIN_File[SYSCMD_MAX] = { 0 };
	char *Walk;
	unsigned char Offset;
	char Nibble_1 = 0;
	char Nibble_2 = 0;
	int j;
	int Ret = 0;

	(void) strncpy(Buffer, Clock_Files, XLSTRLEN_MAX);
	if (Check_Clock_Files(Buffer, TCS_File, TXT_File, BIN_File) != 0) {
		return -1;
	}

	FD = open(Clock->I2C_Bus, O_RDWR);
	if (FD < 0) {
		SC_ERR("unable to access I2C bus %s: %m", Clock->I2C_Bus);
		return -1;
	}

	(void) memset(Buffer, 0, SYSCMD_MAX);
	/* Set the IDT 8A340001 chip to 2 byte addressing mode */
	Buffer[0] = 0xFF;
	Buffer[1] = 0xFD;
	Buffer[2] = 0x0;
	Buffer[3] = 0x10;
	Buffer[4] = 0x20;
	I2C_WRITE(FD, Clock->I2C_Address, 5, Buffer, Ret);
	if (Ret != 0) {
		(void) close(FD);
		return Ret;
	}

	FP = fopen(TXT_File, "r");
	if (FP == NULL) {
		SC_ERR("failed to open file %s: %m", TXT_File);
		return -1;
	}

	while (fgets(Buffer, SYSCMD_MAX, FP)) {
		if (strstr(Buffer, "Size:") == NULL) {
			continue;
		}

		/* Replace '0x' and ',' with ' ' */
		Walk = Buffer;
		for (int i = 0; i < strlen(Buffer); i++) {
			if (*Walk == 'x') {
				*(Walk - 1) = *Walk = ' ';
			}

			if (*Walk == ',') {
				*Walk = ' ';
			}

			Walk++;
		}

		(void) strtok(Buffer, ":");
		(void) sscanf(strtok(NULL, ":"), "%x", &Size);
		(void) sscanf(strtok(NULL, ":"), "%hhx", &Offset);
		(void) strcpy(Data_String, strtok(NULL, "\n"));

		j = 0;
		Data[j++] = Offset;
		Walk = Data_String;
		while (*Walk != '\0') {
			/* Skip the leading white spaces */
			if (isspace(*Walk) != 0) {
				Walk++;
				continue;
			}

			/* Convert 0-9, a-f, and A-F char to int */
			if (*Walk >= 'a') {
				Nibble_1 = (*Walk - 0x57);
			} else if (*Walk >= 'A') {
				Nibble_1 = (*Walk - 0x37);
			} else if (*Walk >= '0') {
				Nibble_1 = (*Walk - 0x30);
			}

			if (*(Walk + 1) >= 'a') {
				Nibble_2 = (*(Walk + 1) - 0x57);
			} else if (*(Walk + 1) >= 'A') {
				Nibble_2 = (*(Walk + 1) - 0x37);
			} else if (*(Walk + 1) >= '0') {
				Nibble_2 = (*(Walk + 1) - 0x30);
			}

			Data[j++] = (Nibble_1 << 4) | Nibble_2;
			Walk += 2;
		}

		I2C_WRITE(FD, Clock->I2C_Address, (Size + 1), Data, Ret);
		if (Ret != 0) {
			(void) close(FD);
			return Ret;
		}
	}

	(void) fclose(FP);

	/*
	 * Create the '8A34001' file indicating that the chip has been
	 * set.  The file is removed when 'restore' command is called
	 * or when the system boots.
	 */
	(void) sprintf(Buffer, "echo '%s' > %s; sync", Clock_Files,
		       IDT8A34001FILE);
	if (Shell_Execute(Buffer) != 0) {
		SC_ERR("failed to update '8A34001' config file: %m");
		return -1;
	}

	/* Update the 'clock' file for the case of setboot mode */
	if (Mode == 1) {
		if (BIN_File[0] != '\0') {
			if (EEPROM_IDT_8A34001_Program(Clock, BIN_File) != 0) {
				SC_ERR("failed to program 8A34001 eeprom");
				return -1;
			}
		} else {
			/* No bin file is available, use generic set boot clock approach */
			(void) sprintf(Buffer, "sed -i -e \'/^%s:/d\' %s 2> /dev/NULL; sync",
				       Clock->Name, CLOCKFILE);
			(void) Shell_Execute(Buffer);
			(void) sprintf(Buffer, "%s: %s\n", Clock->Name, Clock_Files);
			FP = fopen(CLOCKFILE, "a");
			if (FP == NULL) {
				SC_ERR("failed to append clock file %s: %m", CLOCKFILE);
				return -1;
			}

			SC_INFO("Append: %s", Buffer);
			(void) fprintf(FP, "%s", Buffer);
			(void) fflush(FP);
			(void) fsync(fileno(FP));
			(void) fclose(FP);
		}
	}

	return 0;
}

int
Restore_IDT_8A34001(Clock_t *Clock)
{
	char Buffer[SYSCMD_MAX];
	IDT_8A34001_Data_t *Clock_Data;
	DIR *DP;
	int Found = 0;
	struct dirent *File;
	char Clock_File[LSTRLEN_MAX];

	if (Clock->Type_Data == NULL) {
		SC_ERR("no data is available for 8A34001 clock");
		return -1;
	}

	Clock_Data = (IDT_8A34001_Data_t *)Clock->Type_Data;
	if (Clock_Data->Chip_Reset() != 0) {
		return -1;
	}

	/* Remove the '8A34001' file, if any */
	(void) sprintf(Buffer, "rm %s 2> /dev/NULL; sync", IDT8A34001FILE);
	if (Shell_Execute(Buffer) != 0) {
		SC_ERR("failed to remove '%s' file: %m", IDT8A34001FILE);
		return -1;
	}

	/* Remove the previous entry from 'clock' file, if any */
	(void) sprintf(Buffer, "sed -i -e \'/^%s:/d\' %s 2> /dev/NULL; sync",
		       Clock->Name, CLOCKFILE);
	if (Shell_Execute(Buffer) != 0) {
		SC_ERR("failed to update 'clock' config file: %m");
		return -1;
	}

	/* Restore the clock and set its EEPROM to the default image */
	DP = opendir(CFS_PATH);
	if (DP == NULL) {
		SC_ERR("failed to open '%s' directory: %m", CFS_PATH);
		return -1;
	}

	File = readdir(DP);
	while (File != NULL) {
		if (strstr(File->d_name, ".bin") != NULL) {
			(void) snprintf(Clock_File, sizeof(Clock_File), "%s", strtok(File->d_name, ".bin"));
			if (Set_IDT_8A34001(Clock, Clock_File, 1) != 0) {
				SC_ERR("failed to restore 8A34001");
				(void) closedir(DP);
				return -1;
			} else {
				Found = 1;
				break;
			}
		}

		File = readdir(DP);
	}

	(void) closedir(DP);
	if (!Found) {
		SC_ERR("failed to find the default '.bin' image");
		return -1;
	}

	return 0;
}

int
Reset_IDT_8A34001(void)
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

	sleep(1);

	Value = 0x20;   // De-assert reset
	if (Access_IO_Exp(IO_Exp, 1, 0x2, &Value) != 0) {
		SC_ERR("failed to de-assert reset of 8A34001 chip");
		return -1;
	}

	return 0;
}

int
Reset_Op(void)
{
	FILE *FP;
	int State;
	char Buffer[SYSCMD_MAX] = { 0 };
	BootModes_t *BootModes;
	BootMode_t *BootMode;

	if (access(SILICONFILE, F_OK) == 0) {
		FP = fopen(SILICONFILE, "r");
		if (FP == NULL) {
			SC_ERR("failed to open silicon file %s: %m", SILICONFILE);
			return -1;
		}

		if (fgets(Buffer, SYSCMD_MAX, FP) == NULL) {
			SC_ERR("failed to read silicon file %s: %m", SILICONFILE);
			(void) fclose(FP);
			return -1;
		}

		(void) fclose(FP);
		SC_INFO("%s: %s", SILICONFILE, Buffer);
		if (strcmp(Buffer, "ES1\n") == 0) {
			// Turn VCCINT_RAM off
			State = 0;
			if (VCK190_ES1_Vccaux_Workaround(&State) != 0) {
				SC_ERR("failed to turn VCCINT_RAM off");
				return -1;
			}
		}
	}

	/* Assert POR */
	if (Set_GPIO("SYSCTLR_POR_B_LS", 0) != 0) {
		SC_ERR("failed to assert power-on-reset");
		return -1;
	}

	sleep(1);

	/* De-assert POR */
	if (Set_GPIO("SYSCTLR_POR_B_LS", 1) != 0) {
		SC_ERR("failed to de-assert power-on-reset");
		return -1;
	}

	/* If a boot mode is defined, set it after POR */
	if (access(BOOTMODEFILE, F_OK) == 0) {
		FP = fopen(BOOTMODEFILE, "r");
		if (FP == NULL) {
			SC_ERR("failed to open boot_mode file %s: %m", BOOTMODEFILE);
			return -1;
		}

		if (fgets(Buffer, sizeof(Buffer), FP) == NULL) {
			SC_ERR("failed to read boot_mode file %s: %m", BOOTMODEFILE);
			(void) fclose(FP);
			return -1;
		}

		(void) fclose(FP);
		SC_INFO("%s: %s", BOOTMODEFILE, Buffer);
		BootModes = Plat_Devs->BootModes;
		for (int i = 0; i < BootModes->Numbers; i++) {
			BootMode = &BootModes->BootMode[i];
			if (strcmp(Buffer, (char *)BootMode->Name) == 0) {
				if (Set_AltBootMode(BootMode->Value) != 0) {
					SC_ERR("failed to set alternative boot mode");
					return -1;
				}

				break;
			}
		}
	}

	return 0;
}

int
JTAG_Op(int Select)
{
	int State;

	switch (Select) {
	case 1:
		/* Overwrite the default JTAG switch */
		if (Set_GPIO("SYSCTLR_JTAG_S0", 0) != 0) {
			SC_ERR("failed to set JTAG 0");
			return -1;
		}

		if (Set_GPIO("SYSCTLR_JTAG_S1", 0) != 0) {
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
		if (Get_GPIO("SYSCTLR_JTAG_S0", &State) != 0) {
			SC_ERR("failed to release JTAG 0");
			return -1;
		}

		if (Get_GPIO("SYSCTLR_JTAG_S1", &State) != 0) {
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
XSDB_Op(const char *TCL_File, const char *TCL_Args, char *Output, int Length)
{
	FILE *FP;
	char System_Cmd[SYSCMD_MAX];
	char Buffer[LSTRLEN_MAX];
	char *Directory, *Filename;
	int Ret = 0;

	if (access(TCL_File, F_OK) != 0) {
		SC_ERR("failed to access file %s: %m", TCL_File);
		return -1;
	}

	if (Output == NULL) {
		SC_ERR("unallocated output buffer");
		return -1;
	}

	(void) JTAG_Op(1);
	Directory = strdup(TCL_File);
	Filename = strdup(TCL_File);
	/* System_Cmd: cd TCL_FILE directory; XSDB_ENV; XSDB_CMD TCL_FILE TCL_Args */
	if (TCL_Args == NULL) {
		(void) sprintf(System_Cmd, "cd %s; %s; %s %s 2>&1 | tee %s",
			       dirname(Directory), XSDB_ENV, XSDB_CMD, Filename, BITLOGFILE);
	} else {
		(void) sprintf(System_Cmd, "cd %s; %s; %s %s %s 2>&1 | tee %s",
			       dirname(Directory), XSDB_ENV, XSDB_CMD, Filename, TCL_Args, BITLOGFILE);
	}

	SC_INFO("Command: %s", System_Cmd);
	FP = popen(System_Cmd, "r");
	if (FP == NULL) {
		SC_ERR("failed to invoke xsdb");
		Ret = -1;
		goto Out;
	}

	while (fgets(Buffer, sizeof(Buffer), FP) != NULL) {
		SC_INFO("XSDB Output: %s", Buffer);
		(void) strncpy(Output, Buffer, Length);
	}

	if (pclose(FP) != 0) {
		SC_INFO("Command: %s failed!", System_Cmd);
		Ret = -1;
	}

Out:
	(void) JTAG_Op(0);
	free(Directory);
	free(Filename);
	return Ret;
}

int
Get_IDCODE(char *Output, int Length)
{
	char TCL_File[STRLEN_MAX];
	char TCL_Args[STRLEN_MAX];

	(void) sprintf(TCL_File, "%s%s", BIT_PATH, IDCODE_TCL);
	(void) strcpy(TCL_Args, "0x0");
	return XSDB_Op(TCL_File, TCL_Args, Output, Length);
}

int
Get_Temperature(Temperature_t *Temperature)
{
	FILE *FP;
	char Buffer[SYSCMD_MAX];
	char *Temp;
	float Float_Temp;

	(void) sprintf(Buffer, "/usr/bin/sensors %s 2>&1", Temperature->Sensor);
	FP = popen(Buffer, "r");
	if (FP == NULL) {
		SC_ERR("failed to invoke '%s': %m", Buffer);
		return -1;
	}

	while (fgets(Buffer, sizeof(Buffer), FP) != NULL) {
		if (strstr(Buffer, "ERROR: ") != NULL) {
			SC_ERR("temperature is not available");
			(void) pclose(FP);
			return -1;
		}

		if (strstr(Buffer, "temp1") == NULL) {
			continue;
		}

		(void) strtok(Buffer, " ");
		Temp = strtok(NULL, " ");
		Float_Temp = strtof(Temp, NULL);
		SC_PRINT("Temperature(C):\t%3.1f", Float_Temp);
	}

	(void) pclose(FP);
	return 0;
}

int
Get_BootMode_Switch(unsigned int *Value)
{
	char Buffer[SYSCMD_MAX];
	char Chip_Name[STRLEN_MAX];
	unsigned int Line_Offset;
	int State;

	*Value = 0;
	for (int i = 0; i < 4; i++) {
		sprintf(Buffer, "SYSCTLR_VERSAL_MODE%d_READBACK", i);
		if (gpiod_ctxless_find_line(Buffer, Chip_Name, STRLEN_MAX,
					    &Line_Offset) != 1) {
			return -1;
		}

		if (Get_GPIO(Buffer, &State) != 0) {
			return -1;
		}

		*Value |= (State << i);
	}

	return 0;
}

int
Get_BootMode(int Method)
{
	FILE *FP;
	char Buffer[SYSCMD_MAX];
	unsigned int Value;
	BootModes_t *BootModes;

	/*
	 * Supported methods to get the boot mode:
	 *	0: External Boot Mode
	 *	1: Alternative Boot Mode
	 */
	if (Method == 0) {
		if (Get_BootMode_Switch(&Value) != 0) {
			SC_ERR("unable to read boot mode switch");
			return -1;
		}

		SC_INFO("Value 0x%x is read from boot mode switch", Value);
		BootModes = Plat_Devs->BootModes;
		for (int i = 0; i < BootModes->Numbers; i++) {
			if (Value == BootModes->BootMode[i].Value) {
				SC_PRINT("%s", BootModes->BootMode[i].Name);
				return 0;
			}
		}

		SC_ERR("unsupported boot mode value %#x", Value);
		return -1;
	}

	if (Method == 1) {
		if (access(BOOTMODEFILE, F_OK) != 0) {
			SC_ERR("no alternative boot mode is set");
			return -1;
		}

		FP = fopen(BOOTMODEFILE, "r");
		if (FP == NULL) {
			SC_ERR("failed to read file %s: %m", BOOTMODEFILE);
			return -1;
		}

		if (fgets(Buffer, SYSCMD_MAX, FP) == NULL) {
			SC_ERR("failed to read 'boot_mode' config file");
			(void) fclose(FP);
			return -1;
		}

		SC_PRINT_N("%s", Buffer);
		(void) fclose(FP);
		return 0;
	}

	SC_ERR("invalid Get_BootMode method");
	return -1;
}

int
Set_AltBootMode(int Value)
{
	char TCL_File[STRLEN_MAX];
	char TCL_Args[STRLEN_MAX];
	char Output[STRLEN_MAX] = { 0 };

	(void) sprintf(TCL_File, "%s%s", BIT_PATH, BOOTMODE_TCL);
	(void) sprintf(TCL_Args, "%x", Value);
	return XSDB_Op(TCL_File, TCL_Args, Output, sizeof(Output));
}

int
Set_BootMode(BootMode_t *BootMode, int Method)
{
	FILE *FP;
	char Buffer[SYSCMD_MAX];
	unsigned int Value;

	/*
	 * Supported methods to set the boot mode:
	 *	0: External Boot Mode
	 * 	1: Alternative Boot Mode
	 */
	if (Method == 0) {
		/* Clear previous boot mode setting first, if any */
		for (int i = 0; i < 4; i++) {
			if (Set_GPIO(Plat_Devs->BootModes->Mode_Lines[i],
			    0x1) != 0) {
				SC_ERR("failed to set GPIO line %s",
				       Plat_Devs->BootModes->Mode_Lines[i]);
				return -1;
			}
		}

		/*
		 * VCK190/VMK180 boards don't have hardware support to read
		 * back the current position of boot mode switch.
		 */
		if (Get_BootMode_Switch(&Value) != 0) {
			SC_PRINT("WARNING: SW1 needs to be in OFF positions");
			Value = 0xF;
		}

		if ((BootMode->Value & Value) != BootMode->Value) {
			SC_ERR("unable to set boot mode to '%s' because "
			       "SW1 is set to '%s %s %s %s' position",
			       BootMode->Name,
			       ((Value & 0x8) ? "OFF" : "ON"),
			       ((Value & 0x4) ? "OFF" : "ON"),
			       ((Value & 0x2) ? "OFF" : "ON"),
			       ((Value & 0x1) ? "OFF" : "ON"));
			return -1;
		}

		for (int i = 0; i < 4; i++) {
			if (Set_GPIO(Plat_Devs->BootModes->Mode_Lines[i],
			    ((BootMode->Value >> i) & 0x1)) != 0) {
				SC_ERR("failed to set GPIO line %s",
				       Plat_Devs->BootModes->Mode_Lines[i]);
				return -1;
			}
		}

		return 0;
	}

	if (Method == 1) {
		/* Record the boot mode */
		FP = fopen(BOOTMODEFILE, "w");
		if (FP == NULL) {
			SC_ERR("failed to open boot mode file %s: %m",
			       BOOTMODEFILE);
			return -1;
		}

		(void) sprintf(Buffer, "%s\n", BootMode->Name);
		SC_INFO("Alternatve Boot Mode: %s", Buffer);
		(void) fputs(Buffer, FP);
		(void) fclose(FP);
		return 0;
	}

	SC_ERR("invalid Set_BootMode method");
	return -1;
}

int
QSFP_ModuleSelect(SFP_t *SFP, int State)
{
	IO_Exp_t *IO_Exp;
	int i, j;
	int Found = 0;
	unsigned char Upper_Mask = -1;
	unsigned char Lower_Mask = -1;
	unsigned int Mask;
	unsigned int Value;

	if (State != 0 && State != 1) {
		SC_ERR("invalid SFP module select state");
		return -1;
	}

	/*
	 * In current board designs, the SFP modules of type 'sfp' and
	 * 'osfp' don't require the module to be selected before it is
	 * accessed.
	 */
	if (SFP->Type == sfp || SFP->Type == osfp) {
		return 0;
	}

	/*
	 * In boards that need to use a PDI to enable SFP module select,
	 * there is nothing to do for 'State == 0'.
	 */
	if (SFP->Type == qsfp) {
		if (State == 1) {
			return VCK190_QSFP_ModuleSelect(SFP, State);
		} else {
			return 0;
		}
	}

	/*
	 * The following code enables the SFP module select for type
	 * 'qsfpdd' in current board designs.
	 */

	IO_Exp = Plat_Devs->IO_Exp;

	/*
	 * IO expander outputs are labeled to follow schematics for
	 * ease of referencing.  The layout of Output Port registers are
	 * as follow:
	 *
	 * Upper Byte: P07 P06 P05 P04 P03 P02 P01 P00
	 * Lower Byte: P17 P16 P15 P14 P13 P12 P11 P10
	 *
	 * From output labels find which bit of which byte should be
	 * set to 0 (Active Low) for (State == 1).  For (State == 0),
	 * revert the same bit to 1.
	 */
	for (i = 0, j = 8; i < 8; i++, j--) {
		if (strstr(IO_Exp->Labels[i], SFP->Name) != NULL) {
			Found = 1;
			break;
		}
	}

	if (j > 0) {
		Upper_Mask &= ~(1 << (j - 1));
	} else {
		Upper_Mask = 0xff;
	}

	if (!Found) {
		for (i = 8, j = 8; i < 16; i++, j--) {
			if (strstr(IO_Exp->Labels[i], SFP->Name) != NULL) {
				break;
			}
		}

		if (j > 0) {
			Lower_Mask &= ~(1 << (j - 1));
		} else {
			Lower_Mask = 0xff;
		}
	}

	/*
	 * Read the current output value, modify the desired bit, and
	 * write back the new output value.
	 */
	if (Access_IO_Exp(IO_Exp, 0, 0x2, &Value) != 0) {
		SC_ERR("failed to get IO expander output");
		return -1;
	}

	SC_INFO("Current Output Port Registers: %#x", Value);

	Mask = (Upper_Mask << 8) | Lower_Mask;
	if (State == 1) {
		Value &= Mask;
	} else {
		Value = Value | (~Mask & ((1 << IO_Exp->Numbers) - 1));
	}

	SC_INFO("Modify Output Port Registers: %#x", Value);

	if (Access_IO_Exp(IO_Exp, 1, 0x2, &Value) != 0) {
		SC_ERR("failed to set IO expander output");
		return -1;
	}

	return 0;
}

int
Check_Config_File(char *Name, char *Value, int *Found)
{
	FILE *FP;
	char Name_String[STRLEN_MAX];
	char Buffer[LSTRLEN_MAX];

	*Found = 0;
	if (access(CONFIGFILE, F_OK) == 0) {
		FP = fopen(CONFIGFILE, "r");
		if (FP == NULL) {
			SC_ERR("failed to read file %s: %m", CONFIGFILE);
			return -1;
		}

		(void) sprintf(Name_String, "%s:", Name);
		while (fgets(Buffer, LSTRLEN_MAX, FP)) {
			if (strstr(Buffer, Name_String) != NULL) {
				SC_INFO("%s: %s", CONFIGFILE, Buffer);
				(void) strtok(Buffer, ":");
				if (strcmp(Buffer, Name) != 0) {
					continue;
				}

				(void) strcpy(Value, strtok(NULL, " \n"));
				*Found = 1;
				break;
			}
		}

		(void) fclose(FP);
	}

	return 0;
}

int
Boot_Config_PDI(char *Entry)
{
	char PDI_File[STRLEN_MAX];
	char Buffer[SYSCMD_MAX];

	/*
	 * If PDIFILE does already exist, that indicates a manual PDI boot
	 * configuration has been perfomed on this system.  A manual configuration
	 * overrides any default setting read from the JSON file.
	 */
	if (access(PDIFILE, F_OK) == 0) {
		return 0;
	}

	/* For default PDI, adjust the name based on version of silicon */
	(void) strcpy(PDI_File, Entry);
	if ((strcmp(PDI_File, DEFAULT_PDI) == 0) &&
	    (strcmp(Silicon_Revision, "ES1") == 0)) {
		(void) sprintf(PDI_File, "es1_%s", DEFAULT_PDI);
	}

	(void) sprintf(Buffer, "echo '%s' > %s; sync", PDI_File, PDIFILE);
	if (Shell_Execute(Buffer) != 0) {
		SC_ERR("failed to update 'PDI' config file: %m");
		return -1;
	}

	return 0;
}
