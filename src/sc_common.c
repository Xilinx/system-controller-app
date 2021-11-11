/*
 * Copyright (c) 2021 Xilinx, Inc.  All rights reserved.
 *
 * SPDX-License-Identifier: MIT
 */

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <fcntl.h>
#include <ctype.h>
#include <string.h>
#include <time.h>
#include <math.h>
#include <gpiod.h>
#include "sc_app.h"

extern Plat_Devs_t VCK190_Devs;
extern Plat_Ops_t VCK190_Ops;
extern Plat_Devs_t VPK120_Devs;
extern Plat_Ops_t VPK120_Ops;

extern int Parse_JSON(const char *, Plat_Devs_t *);

Plat_Devs_t *Plat_Devs;
Plat_Ops_t *Plat_Ops;

/*
 * Supported Boards
 */
typedef enum {
	BOARD_VCK190,
	BOARD_VMK180,
	BOARD_VPK120,
	BOARD_MAX,
} Board_Index;

Boards_t Boards = {
	.Numbers = BOARD_MAX,
	.Board_Info[BOARD_VCK190] = {
		.Name = "VCK190",
		.Devs = &VCK190_Devs,
		.Ops = &VCK190_Ops,
	},
	.Board_Info[BOARD_VMK180] = {
		.Name = "VMK180",
		.Devs = &VCK190_Devs,
		.Ops = &VCK190_Ops,
	},
	.Board_Info[BOARD_VPK120] = {
		.Name = "VPK120",
		.Devs = &VPK120_Devs,
		.Ops = &VPK120_Ops,
	},
};

static int
Get_Product_Name(OnBoard_EEPROM_t *EEPROM, char *Product_Name)
{
	int FD;
	char In_Buffer[SYSCMD_MAX];
	char Out_Buffer[STRLEN_MAX];
	int Offset, Length;

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
		SC_ERR("unable to read onboard EEPROM");
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
Board_Identification(void)
{
	FILE *FP;
	Board_t *Board;
	char Buffer[SYSCMD_MAX] = { 0 };
	char System_Cmd[SYSCMD_MAX];
	int Ret;

	if (access(BOARDFILE, F_OK) != 0) {
		for (int i = 0; i < Boards.Numbers; i++) {
			Board = &Boards.Board_Info[i];
			if (Board->Devs->OnBoard_EEPROM == NULL) {
				SC_ERR("eeprom info is not available for %s",
				       Board->Name);
				return -1;
			}

			Ret = Get_Product_Name(Board->Devs->OnBoard_EEPROM, Buffer);
			if (Ret == 0) {
				break;
			}
		}

		if (Ret != 0) {
			SC_ERR("failed to identify the board");
			return -1;
		}

		(void) sprintf(System_Cmd, "echo %s > %s; sync", Buffer, BOARDFILE);
		SC_INFO("Command: %s", System_Cmd);
		system(System_Cmd);
	}

	FP = fopen(BOARDFILE, "r");
	if (FP == NULL) {
		SC_ERR("failed to read file %s: %m", BOARDFILE);
		return -1;
	}

	(void) fgets(System_Cmd, SYSCMD_MAX, FP);
	(void) fclose(FP);
	(void) strcpy(Buffer, strtok(System_Cmd, "\n"));

	for (int i = 0; i < Boards.Numbers; i++) {
		Board = &Boards.Board_Info[i];
		if (strcmp(Board->Name, Buffer) == 0) {
			Plat_Devs = (Plat_Devs_t *)malloc(sizeof(Plat_Devs_t));
			Plat_Devs->OnBoard_EEPROM = Board->Devs->OnBoard_EEPROM;
			if (Parse_JSON(Board->Name, Plat_Devs) != 0) {
				SC_ERR("Parsing JSON file failed.");
				return -1;
			}

			Plat_Ops = Board->Ops;
			break;
		}
	}

	if (Plat_Devs == NULL || Plat_Ops == NULL) {
		SC_ERR("Unsupported board");
		return -1;
	}

	return 0;
}

int
Access_Regulator(Voltage_t *Regulator, float *Voltage, int Access)
{
	int FD;
	int Supported = 0;
	int Index = 0;
	char In_Buffer[STRLEN_MAX];
	char Out_Buffer[STRLEN_MAX];
	signed int Exponent;
	short Mantissa;
	int Get_Vout_Mode = 1;
	int Ret = 0;
	unsigned int Vout;
	float Over_Voltage_Limit;

	/* Check if setting requested voltage is supported */
	if (1 == Access) {
		while (!(Index == ITEMS_MAX ||
		    Regulator->Supported_Volt[Index] == -1)) {
			if (*Voltage == Regulator->Supported_Volt[Index]) {
				Supported = 1;
				break;
			}

			Index++;
		}

		if (0 == Supported) {
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

	/* IR38164 does not support VOUT_MODE PMBus command */
	if (0 == strcmp(Regulator->Part_Name, "IR38164")) {
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
		Exponent = In_Buffer[0] - (sizeof(int) * 8);
	} else {
		/* For IR38164, use exponent value -8 */
		Exponent = -8;
	}

	switch (Access) {
	case 0:
		/* Get VOUT */
		Out_Buffer[0] = PMBUS_READ_VOUT;
		(void) memset(In_Buffer, 0, STRLEN_MAX);
		I2C_READ(FD, Regulator->I2C_Address, 2, Out_Buffer, In_Buffer, Ret);
		if (Ret != 0) {
			(void) close(FD);
			return Ret;
		}

		Mantissa = ((unsigned char)In_Buffer[1] << 8) | (unsigned char)In_Buffer[0];
		SC_INFO("Mantissa: %#x", Mantissa);
		*Voltage = Mantissa * pow(2, Exponent);
		break;
	case 1:
		/* Disable VOUT */
		Out_Buffer[0] = PMBUS_OPERATION;
		Out_Buffer[1] = 0x0;
		I2C_WRITE(FD, Regulator->I2C_Address, 2, Out_Buffer, Ret);
		if (Ret != 0) {
			(void) close(FD);
			return Ret;
		}

		/* Set Undervoltage limits to 0 */
		Out_Buffer[0] = PMBUS_VOUT_UV_FAULT_LIMIT;
		Out_Buffer[1] = 0x0;
		Out_Buffer[2] = 0x0;
		SC_INFO("Undervoltage Fault Limit: %#x %#x %#x", Out_Buffer[0],
			Out_Buffer[1], Out_Buffer[2]);
		I2C_WRITE(FD, Regulator->I2C_Address, 3, Out_Buffer, Ret);
		if (Ret != 0) {
			(void) close(FD);
			return Ret;
		}

		Out_Buffer[0] = PMBUS_VOUT_UV_WARN_LIMIT;
		SC_INFO("Undervoltage Warning Limit: %#x %#x %#x", Out_Buffer[0],
			Out_Buffer[1], Out_Buffer[2]);
		I2C_WRITE(FD, Regulator->I2C_Address, 3, Out_Buffer, Ret);
		if (Ret != 0) {
			(void) close(FD);
			return Ret;
		}

		/* Set Overvoltage limits to 30% of VOUT */
		Over_Voltage_Limit = (*Voltage != 0) ? *Voltage : 0.1;
		Over_Voltage_Limit += (Over_Voltage_Limit * 0.3);
		Vout = round(Over_Voltage_Limit / pow(2, Exponent));
		Out_Buffer[0] = PMBUS_VOUT_OV_FAULT_LIMIT;
		Out_Buffer[1] = Vout & 0xFF;
		Out_Buffer[2] = Vout >> 8;
		SC_INFO("Overvoltage Fault Limit: %#x %#x %#x", Out_Buffer[0],
			Out_Buffer[1], Out_Buffer[2]);
		I2C_WRITE(FD, Regulator->I2C_Address, 3, Out_Buffer, Ret);
		if (Ret != 0) {
			(void) close(FD);
			return Ret;
		}

		Out_Buffer[0] = PMBUS_VOUT_OV_WARN_LIMIT;
		SC_INFO("Overvoltage Warning Limit: %#x %#x %#x", Out_Buffer[0],
			Out_Buffer[1], Out_Buffer[2]);
		I2C_WRITE(FD, Regulator->I2C_Address, 3, Out_Buffer, Ret);
		if (Ret != 0) {
			(void) close(FD);
			return Ret;
		}

		/* Set VOUT */
		Vout = round(*Voltage / pow(2, Exponent));
		Out_Buffer[0] = PMBUS_VOUT_COMMAND;
		Out_Buffer[1] = Vout & 0xFF;
		Out_Buffer[2] = Vout >> 8;
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
		SC_INFO("Overvoltage Fault Limit(V):\t%.2f\t(Reg 0x%x:\t0x%x)",
		       *Voltage, PMBUS_VOUT_OV_FAULT_LIMIT, Mantissa);
		printf("Overvoltage Fault Limit(V):\t%.2f\t(Reg 0x%x:\t0x%x)\n",
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

	if (Op == 1 && *Data > 0xFFFF) {
		SC_ERR("invalid data, valid value is between 0 and 0xFFFF");
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
	I2C_READ(FD, FMC->I2C_Address, 0xFF, Out_Buffer, In_Buffer, Ret);
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
GPIO_Get(char *Label, int *State)
{
	FILE *FP;
	char Chip_Name[STRLEN_MAX];
	char Buffer[STRLEN_MAX];
	char Output[STRLEN_MAX] = {'\0'};
	unsigned int Line_Offset;
	struct gpiod_line *GPIO_Line;

	if (gpiod_ctxless_find_line(Label, Chip_Name, STRLEN_MAX,
	    &Line_Offset) != 1) {
		SC_ERR("failed to find GPIO line %s", Label);
		return -1;
	}

	(void) sprintf(Buffer, "gpioget %s %d 2>&1", Chip_Name, Line_Offset);
	SC_INFO("Command: %s", Buffer);
	FP = popen(Buffer, "r");
	if (FP == NULL) {
		SC_ERR("failed to get the state of GPIO line %s", Label);
		return -1;
	}

	(void) fgets(Output, sizeof(Output), FP);
	(void) pclose(FP);
	SC_INFO("Output: %s", Output);
	if ((strcmp(Output, "0\n") != 0) && (strcmp(Output, "1\n") != 0)) {
		SC_ERR("invalid output %s", Output);
		return -1;
	}

	*State = atoi(Output);
	return 0;
}

int
GPIO_Set(char *Label, int State)
{
	FILE *FP;
	char Chip_Name[STRLEN_MAX];
	char Buffer[STRLEN_MAX];
	char Output[STRLEN_MAX] = {'\0'};
	unsigned int Line_Offset;
	struct gpiod_line *GPIO_Line;

	if (gpiod_ctxless_find_line(Label, Chip_Name, STRLEN_MAX,
	    &Line_Offset) != 1) {
		SC_ERR("failed to find GPIO line");
		return -1;
	}

	(void) sprintf(Buffer, "gpioset %s %d=%d 2>&1", Chip_Name, Line_Offset,
		       State);
	SC_INFO("Command: %s", Buffer);
	FP = popen(Buffer, "r");
	if (FP == NULL) {
		SC_ERR("failed to set GPIO line\n");
		return -1;
	}

	(void) fgets(Output, sizeof(Output), FP);
	(void) pclose(FP);
	SC_INFO("Output: %s", Output);
	if (Output[0] != '\0') {
		SC_ERR("invalid output %s", Output);
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
	static struct tm BuildDate;
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

	SC_INFO("0x0B - Manufacturing Date:\t%s", ctime(&Time));
	printf("0x0B - Manufacturing Date:\t%s", ctime(&Time));
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
		SC_PRINT("0x%.2x - FRU ID:\t%.2x", Offset, Buffer[Offset + 1]);
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
	if (PCIe == 1) {
		Length = (Buffer[Offset] & 0x3F);
		SC_INFO("0x%.2x - PCIe Info:\t", (Offset + 1));
		printf("0x%.2x - PCIe Info:\t", (Offset + 1));
		for (int i = 0; i < Length; i++) {
			SC_INFO("%.2x", Buffer[Offset + i + 1]);
			printf("%.2x", Buffer[Offset + i + 1]);
		}

		printf("\n");
		Offset = Offset + Length + 1;
		Length = (Buffer[Offset] & 0x3F);
		SC_INFO("0x%.2x - UUID:\t", (Offset + 1));
		printf("0x%.2x - UUID:\t", (Offset + 1));
		for (int i = 0; i < Length; i++) {
			SC_INFO("%.2x", Buffer[Offset + i + 1]);
			printf("%.2x", Buffer[Offset + i + 1]);
			if (i == 3 || i == 5 || i == 7 || i == 9) {
				printf("-");
			}
		}

		printf("\n");
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

#define DC_OUTPUT	0x1
#define DC_LOAD		0x2
#define OEM_D2		0xD2
#define OEM_D3		0xD3
#define OEM_VITA_57_1	0xFA

int
EEPROM_MultiRecord(char *Buffer)
{
	int Offset;
	int Type;
	int Last_Record;
	int Length;

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
			SC_PRINT("0x%.2x - Record Type:\t%.2x (DC Output)", Offset, Type);
			break;
		case DC_LOAD:
			SC_PRINT("0x%.2x - Record Type:\t%.2x (DC Load)", Offset, Type);
			break;
		case OEM_D2:
			SC_PRINT("0x%.2x - Record Type:\t%.2x (Mac ID)", Offset, Type);
			break;
		case OEM_D3:
			SC_PRINT("0x%.2x - Record Type:\t%.2x (Memory)", Offset, Type);
			break;
		case OEM_VITA_57_1:
			SC_PRINT("0x%.2x - Record Type:\t%.2x (Vita 57.1)", Offset, Type);
			break;
		default:
			SC_ERR("unsupported multirecord type");
			return -1;
		}

		SC_PRINT("0x%.2x - Record Format:\t%.2x", (Offset + 1),
		       Buffer[Offset + 1]);
		SC_PRINT("0x%.2x - Length:\t%.2x", (Offset + 2),
		       Buffer[Offset + 2]);
		SC_PRINT("0x%.2x - Record Check sum:\t%.2x", (Offset + 3),
		       Buffer[Offset + 3]);
		SC_PRINT("0x%.2x - Header Check sum:\t%.2x", (Offset + 4),
		       Buffer[Offset + 4]);
		if (Type == OEM_D2 || Type == OEM_D3) {
			SC_PRINT("0x%.2x - Xilinx IANA ID:\t%.2x%.2x%.2x", (Offset + 5),
			       Buffer[Offset + 5], Buffer[Offset + 6], Buffer[Offset + 7]);
		}

		switch (Type) {
		case DC_OUTPUT:
			SC_PRINT("0x%.2x - Output Number:\t%.2x (Power Rail)",
			       (Offset + 5), Buffer[Offset + 5]);
			SC_PRINT("0x%.2x - Nominal Voltage:\t%.2x%.2x (%.2fV)",
			       (Offset + 6), Buffer[Offset + 6], Buffer[Offset + 7],
			       (float)(Buffer[Offset + 7] << 8 | Buffer[Offset + 6]) / 100.0);
			SC_PRINT("0x%.2x - Spec'd Min Voltage:\t%.2x%.2x (%.2fV)",
			       (Offset + 8), Buffer[Offset + 8], Buffer[Offset + 9],
			       (float)(Buffer[Offset + 9] << 8 | Buffer[Offset + 8]) / 100.0);
			SC_PRINT("0x%.2x - Spec'd Max Voltage:\t%.2x%.2x (%.2fV)",
			       (Offset + 10), Buffer[Offset + 10], Buffer[Offset + 11],
			       (float)(Buffer[Offset + 11] << 8 | Buffer[Offset + 10]) / 100.0);
			SC_PRINT("0x%.2x - Spec'd Ripple Noise:\t%.2x%.2x (%dmV)",
			       (Offset + 12), Buffer[Offset + 12], Buffer[Offset + 13],
			       (Buffer[Offset + 13] << 8 | Buffer[Offset + 12]));
			SC_PRINT("0x%.2x - Min Current Load:\t%.2x%.2x (%dmA)",
			       (Offset + 14), Buffer[Offset + 14], Buffer[Offset + 15],
			       (Buffer[Offset + 15] << 8 | Buffer[Offset + 14]));
			SC_PRINT("0x%.2x - Max Current Load:\t%.2x%.2x (%dmA)",
			       (Offset + 16), Buffer[Offset + 16], Buffer[Offset + 17],
			       (Buffer[Offset + 17] << 8 | Buffer[Offset + 16]));
			break;
		case DC_LOAD:
			if (Buffer[Offset + 5] == 0x0) {
				SC_PRINT("0x%.2x - Output Number:\t%.2x (Voltage Adjust)",
				       (Offset + 5), Buffer[Offset + 5]);
			} else if (Buffer[Offset + 5] <= 0xF) {
				SC_PRINT("0x%.2x - Output Number:\t%.2x (Power Rail)",
				       (Offset + 5), Buffer[Offset + 5]);
			} else {
				SC_ERR("unsupported DC Load output number");
				return -1;
			}

			SC_PRINT("0x%.2x - Nominal Voltage:\t%.2x%.2x (%.2fV)",
			       (Offset + 6), Buffer[Offset + 6], Buffer[Offset + 7],
			       (float)(Buffer[Offset + 7] << 8 | Buffer[Offset + 6]) / 100.0);
			SC_PRINT("0x%.2x - Spec'd Min Voltage:\t%.2x%.2x (%.2fV)",
			       (Offset + 8), Buffer[Offset + 8], Buffer[Offset + 9],
			       (float)(Buffer[Offset + 9] << 8 | Buffer[Offset + 8]) / 100.0);
			SC_PRINT("0x%.2x - Spec'd Max Voltage:\t%.2x%.2x (%.2fV)",
			       (Offset + 10), Buffer[Offset + 10], Buffer[Offset + 11],
			       (float)(Buffer[Offset + 11] << 8 | Buffer[Offset + 10]) / 100.0);
			SC_PRINT("0x%.2x - Spec'd Ripple Noise:\t%.2x%.2x (%dmV)",
			       (Offset + 12), Buffer[Offset + 12], Buffer[Offset + 13],
			       (Buffer[Offset + 13] << 8 | Buffer[Offset + 12]));
			SC_PRINT("0x%.2x - Min Current Load:\t%.2x%.2x (%dmA)",
			       (Offset + 14), Buffer[Offset + 14], Buffer[Offset + 15],
			       (Buffer[Offset + 15] << 8 | Buffer[Offset + 14]));
			SC_PRINT("0x%.2x - Max Current Load:\t%.2x%.2x (%dmA)",
			       (Offset + 16), Buffer[Offset + 16], Buffer[Offset + 17],
			       (Buffer[Offset + 17] << 8 | Buffer[Offset + 16]));
			break;
		case OEM_D2:
			if (Buffer[Offset + 8] == 0x11) {
				SC_PRINT("0x%.2x - Version Number:\t%.2x (SC Mac ID)",
				       (Offset + 8), Buffer[Offset + 8]);
				SC_PRINT("0x%.2x - Mac ID 0:\t%.2x:%.2x:%.2x:%.2x:%.2x:%.2x",
				       (Offset + 9), Buffer[Offset + 9],
				       Buffer[Offset + 10], Buffer[Offset + 11],
				       Buffer[Offset + 12], Buffer[Offset + 13],
				       Buffer[Offset + 14]);
			} else if (Buffer[Offset + 8] == 0x31) {
				SC_PRINT("0x%.2x - Version Number:\t%.2x (Veral Mac ID)",
				       (Offset + 8), Buffer[Offset + 8]);
				SC_PRINT("0x%.2x - Mac ID 0:\t%.2x:%.2x:%.2x:%.2x:%.2x:%.2x",
				       (Offset + 9), Buffer[Offset + 9],
				       Buffer[Offset + 10], Buffer[Offset + 11],
				       Buffer[Offset + 12], Buffer[Offset + 13],
				       Buffer[Offset + 14]);
				SC_PRINT("0x%.2x - Mac ID 1:\t%.2x:%.2x:%.2x:%.2x:%.2x:%.2x",
				       (Offset + 15), Buffer[Offset + 15],
				       Buffer[Offset + 16], Buffer[Offset + 17],
				       Buffer[Offset + 18], Buffer[Offset + 19],
				       Buffer[Offset + 20]);
			} else {
				SC_ERR("unsupported D2 version number");
				return -1;
			}

			break;
		case OEM_D3:
			SC_PRINT("0x%.2x - Memory Type:\t%s", (Offset + 8),
			       &Buffer[Offset + 8]);
			Length = strlen(&Buffer[Offset + 8]) + 1;
			SC_PRINT("0x%.2x - Voltage Supply:\t%s", (Offset + 8 + Length),
			       &Buffer[Offset + 8 + Length]);
			break;
		case OEM_VITA_57_1:
			SC_PRINT("0x%.2x - Organizationally Unique Identifier:\t%.2x%.2x%.2x",
			       (Offset + 5), Buffer[Offset + 5], Buffer[Offset + 6],
			       Buffer[Offset + 7]);
			SC_PRINT("0x%.2x - Subtype Version:\t%.2x", (Offset + 8),
			       Buffer[Offset + 8]);
			SC_PRINT("0x%.2x - Connector Type:\t%.2x", (Offset + 9),
			       Buffer[Offset + 9]);
			SC_PRINT("0x%.2x - P1 Bank A Number Signals:\t%.2x", (Offset + 10),
			       Buffer[Offset + 10]);
			SC_PRINT("0x%.2x - P1 Bank B Number Signals:\t%.2x", (Offset + 11),
			       Buffer[Offset + 11]);
			SC_PRINT("0x%.2x - P2 Bank A Number Signals:\t%.2x", (Offset + 12),
			       Buffer[Offset + 12]);
			SC_PRINT("0x%.2x - P2 Bank B Number Signals:\t%.2x", (Offset + 13),
			       Buffer[Offset + 13]);
			SC_PRINT("0x%.2x - P1 GBT B Number Signals:\t%.2x", (Offset + 14),
			       Buffer[Offset + 14]);
			SC_PRINT("0x%.2x - Max Clock for TCK:\t%.2x (%dMhz)", (Offset + 15),
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
			SC_PRINT(" ");
		}
	} while (!Last_Record);

	return 0;
}

static int
Check_Clock_Files(char *Clock_Files, char *TCS_File, char *TXT_File)
{
	char *CP1 = NULL;
	char *CP2 = NULL;
	int Begin = 0;
	int Length = strlen(Clock_Files);

	for (int i = 0; i < Length; i++) {
		if (isspace(Clock_Files[i]) != 0) {
			Begin = 0;
			Clock_Files[i] = '\0';
		} else {
			if (Begin == 0) {
				if (CP1 == NULL) {
					CP1 = &Clock_Files[i];
				} else {
					CP2 = &Clock_Files[i];
				}

				Begin = 1;
			}
		}
	}

	if ((CP1 == NULL) || (CP2 == NULL)) {
		SC_ERR("two clock files are required");
		return -1;
	}

	if (TCS_File != NULL) {
		(void) sprintf(TCS_File, "%s/clock_files/%s", BIT_PATH, CP1);
		if (strstr(TCS_File, ".tcs") == NULL) {
			SC_ERR("argument %s is not a .tcs file", CP1);
			return -1;
		}

		if (access(TCS_File, F_OK) != 0) {
			SC_ERR("file %s doesn't exist: %m", TCS_File);
			return -1;
		}
	}

	if (TXT_File != NULL) {
		(void) sprintf(TXT_File, "%s/clock_files/%s", BIT_PATH, CP2);
		if (strstr(TXT_File, ".txt") == NULL) {
			SC_ERR("argument %s is not a .txt file", CP2);
			return -1;
		}

		if (access(TXT_File, F_OK) != 0) {
			SC_ERR("file %s doesn't exist: %m", TXT_File);
			return -1;
		}
	}

	return 0;
}

int
Get_IDT_8A34001(Clock_t *Clock)
{
	FILE *FP;
	char Buffer[SYSCMD_MAX];
	char Label[STRLEN_MAX];
	char Frequency[STRLEN_MAX];
	IDT_8A34001_Data_t *Clock_Data;
	char TCS_File[SYSCMD_MAX];

	if (Clock->Type_Data == NULL) {
		SC_ERR("no data is available for 8A34001 clock");
		return -1;
	}

	Clock_Data = (IDT_8A34001_Data_t *)Clock->Type_Data;

	/*
	 * If there is no '8A34001' file, that means the chip has not
	 * been set since boot time.
	 */
	if (access(IDT8A34001FILE, F_OK) != 0) {
		for (int i = 0; i < Clock_Data->Number_Label; i++) {
			SC_PRINT("%s:\t0 MHz/PPS", Clock_Data->Display_Label[i]);
		}

		return 0;
	}

	FP = fopen(IDT8A34001FILE, "r");
	if (FP == NULL) {
		SC_ERR("failed to read file %s: %m", IDT8A34001FILE);
		return -1;
	}

	(void) fgets(Buffer, SYSCMD_MAX, FP);
	(void) fclose(FP);
	if (Check_Clock_Files(Buffer, TCS_File, NULL) != 0) {
		return -1;
	}

	FP = fopen(TCS_File, "r");
	if (FP == NULL) {
		SC_ERR("failed to open %s: %m", TCS_File);
		return -1;
	}

	while (fgets(Buffer, SYSCMD_MAX, FP)) {
		if ((strstr(Buffer, "_Frequency:") == NULL) &&
		    (strstr(Buffer, "DesiredFrequency:") == NULL)) {
			continue;
		}

		(void) strcpy(Label, strtok(Buffer, ":"));
		(void) strcpy(Frequency, strtok(NULL, "|"));
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

	return 0;
}

int
Set_IDT_8A34001(Clock_t *Clock, char *Clock_Files, int Mode)
{
	FILE *FP;
	int FD;
	char Buffer[SYSCMD_MAX];
	int Size;
	char Data_String[SYSCMD_MAX];
	char Data[SYSCMD_MAX];
	char TXT_File[SYSCMD_MAX];
	char *Walk;
	char Offset, Nibble_1, Nibble_2;
	int j;
	int Ret = 0;

	(void) strcpy(Buffer, Clock_Files);
	if (Check_Clock_Files(Buffer, NULL, TXT_File) != 0) {
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
		(void) sscanf(strtok(NULL, ":"), "%x", &Offset);
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
	SC_INFO("Command: %s", Buffer);
	system(Buffer);

	/* Update the 'clock' file for the case of setboot mode */
	if (Mode == 1) {
		/* Remove the old entry, if any */
		(void) sprintf(Buffer, "sed -i -e \'/^%s:/d\' %s 2> /dev/NULL",
			       Clock->Name, CLOCKFILE);
		SC_INFO("Command: %s", Buffer);
		system(Buffer);

		(void) sprintf(Buffer, "%s:\t%s\n", Clock->Name, Clock_Files);
		FP = fopen(CLOCKFILE, "a");
		if (FP == NULL) {
			SC_ERR("failed to append clock file: %m");
			return -1;
		}

		SC_INFO("Append: %s", Buffer);
		(void) fprintf(FP, "%s", Buffer);
		(void) fflush(FP);
		(void) fsync(fileno(FP));
		(void) fclose(FP);
	}

	return 0;
}

int
Restore_IDT_8A34001(Clock_t *Clock)
{
	char Buffer[SYSCMD_MAX];
	IDT_8A34001_Data_t *Clock_Data;

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
	SC_INFO("Command: %s", Buffer);
	system(Buffer);

	/* Remove the previous entry from 'clock' file, if any */
	(void) sprintf(Buffer, "sed -i -e \'/^%s:/d\' %s 2> /dev/NULL; sync",
		       Clock->Name, CLOCKFILE);
	SC_INFO("Command: %s", Buffer);
	system(Buffer);

	return 0;
}
