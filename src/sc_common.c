/*
 * Copyright (c) 2021 Xilinx, Inc.  All rights reserved.
 *
 * SPDX-License-Identifier: MIT
 */

#include <stdio.h>
#include <unistd.h>
#include <fcntl.h>
#include <string.h>
#include <math.h>
#include <gpiod.h>
#include "sc_app.h"

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
		printf("ERROR: unable to open the voltage regulator\n");
		return -1;
	}

	/* Select the page, if the voltage regulator supports it */
	if (Regulator->Page_Select != -1) {
		Out_Buffer[0] = 0x0;
		Out_Buffer[1] = Regulator->Page_Select;
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

		/* Set Under-Voltage limits to 0 */
		Out_Buffer[0] = PMBUS_VOUT_UV_FAULT_LIMIT;
		Out_Buffer[1] = 0x0;
		Out_Buffer[2] = 0x0;
		I2C_WRITE(FD, Regulator->I2C_Address, 3, Out_Buffer, Ret);
		if (Ret != 0) {
			(void) close(FD);
			return Ret;
		}

		Out_Buffer[0] = PMBUS_VOUT_UV_WARN_LIMIT;
		I2C_WRITE(FD, Regulator->I2C_Address, 3, Out_Buffer, Ret);
		if (Ret != 0) {
			(void) close(FD);
			return Ret;
		}

		/* Set Over-Voltage limits to 30% of VOUT */
		Over_Voltage_Limit = (*Voltage != 0) ? *Voltage : 0.1;
		Over_Voltage_Limit += (Over_Voltage_Limit * 0.3);
		Vout = round(Over_Voltage_Limit / pow(2, Exponent));
		Out_Buffer[0] = PMBUS_VOUT_OV_FAULT_LIMIT;
		Out_Buffer[1] = Vout & 0xFF;
		Out_Buffer[2] = Vout >> 8;
		I2C_WRITE(FD, Regulator->I2C_Address, 3, Out_Buffer, Ret);
		if (Ret != 0) {
			(void) close(FD);
			return Ret;
		}

		Out_Buffer[0] = PMBUS_VOUT_OV_WARN_LIMIT;
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
		I2C_WRITE(FD, Regulator->I2C_Address, 3, Out_Buffer, Ret);
		if (Ret != 0) {
			(void) close(FD);
			return Ret;
		}

		/* Enable VOUT */
		(void) memset(Out_Buffer, 0, STRLEN_MAX);
		Out_Buffer[0] = PMBUS_OPERATION;
		Out_Buffer[1] = 0x80;
		I2C_WRITE(FD, Regulator->I2C_Address, 2, Out_Buffer, Ret);
		if (Ret != 0) {
			(void) close(FD);
			return Ret;
		}

		break;
	default:
		printf("ERROR: invalid regulator access\n");
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
 *      *Out:   Pointer to output value to be written to device.
 * Output -
 *      *In:    Pointer to input value read from the device.
 */
int
Access_IO_Exp(IO_Exp_t *IO_Exp, int Op, int Offset, unsigned int *Out,
    unsigned int *In)
{
	int FD;
	char In_Buffer[STRLEN_MAX];
	char Out_Buffer[STRLEN_MAX];
	int Ret = 0;

	FD = open(IO_Exp->I2C_Bus, O_RDWR);
	if (FD < 0) {
		printf("ERROR: unable to open IO expander\n");
		return -1;
	}

	(void) memset(Out_Buffer, 0, STRLEN_MAX);
	(void) memset(In_Buffer, 0, STRLEN_MAX);
	if (Op == 0) {
		Out_Buffer[0] = Offset;
		I2C_READ(FD, IO_Exp->I2C_Address, 2, Out_Buffer, In_Buffer, Ret);
		if (Ret != 0) {
			(void) close(FD);
			return Ret;
		}

		*In = ((In_Buffer[0] << 8) | In_Buffer[1]);

	} else if (Op == 1) {
		Out_Buffer[0] = Offset;
		Out_Buffer[1] = ((*Out >> 8) & 0xFF);
		Out_Buffer[2] = (*Out & 0xFF);
		I2C_WRITE(FD, IO_Exp->I2C_Address, 3, Out_Buffer, Ret);
		if (Ret != 0) {
			(void) close(FD);
			return Ret;
		}

	} else {
		printf("ERROR: invalid access operation\n");
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
		printf("ERROR: unable to open FMC EEPROM\n");
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
		printf("ERROR: failed to find GPIO line %s\n", Label);
		return -1;
	}

	(void) sprintf(Buffer, "gpioget %s %d 2>&1", Chip_Name, Line_Offset);
	FP = popen(Buffer, "r");
	if (FP == NULL) {
		printf("ERROR: failed to get the state of GPIO line %s\n",
		       Label);
		return -1;
	}

	(void) fgets(Output, sizeof(Output), FP);
	(void) pclose(FP);
	if ((strcmp(Output, "0\n") != 0) && (strcmp(Output, "1\n") != 0)) {
		printf("ERROR: %s", Output);
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
		printf("ERROR: failed to find GPIO line.\n");
		return -1;
	}

	(void) sprintf(Buffer, "gpioset %s %d=%d 2>&1", Chip_Name, Line_Offset,
		       State);
	FP = popen(Buffer, "r");
	if (FP == NULL) {
		printf("ERROR: failed to set GPIO line\n");
		return -1;
	}

	(void) fgets(Output, sizeof(Output), FP);
	(void) pclose(FP);
	if (Output[0] != '\0') {
		printf("ERROR: %s", Output);
		return -1;
	}

	return 0;
}
