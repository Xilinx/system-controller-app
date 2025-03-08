/*
 * Copyright (c) 2020 - 2022 Xilinx, Inc.  All rights reserved.
 * Copyright (c) 2022 - 2025 Advanced Micro Devices, Inc.  All rights reserved.
 *
 * SPDX-License-Identifier: MIT
 */

#include <stdio.h>
#include <fcntl.h>
#include <ctype.h>
#include <unistd.h>
#include <stdlib.h>
#include <string.h>
#include "sc_app.h"

extern Plat_Devs_t *Plat_Devs;
extern char Board_Name[];
extern char Silicon_Revision[];

/*
 * Remove BIT log from the last run.
 */
void
Remove_BIT_Log(void)
{
	char Command[SYSCMD_MAX];

	(void) sprintf(Command, "rm %s", BITLOGFILE);
	(void) Shell_Execute(Command);
}

/*
 * This test validates whether the current clock frequency is
 * the same as default value.
 */
int
Clocks_Check(void *Arg1, void *Arg2)
{
	BIT_t *BIT_p = Arg1;
	__attribute__((unused)) void *Ignore = Arg2;
	int FD;
	Clocks_t *Clocks;
	char ReadBuffer[STRLEN_MAX];
	double Freq, Lower, Upper, Delta;

	Remove_BIT_Log();

	Clocks = Plat_Devs->Clocks;
	if (Clocks == NULL) {
		SC_ERR("clock operation is not supported");
		return -1;
	}

	for (int i = 0; i < Clocks->Numbers; i++) {
		if (Clocks->Clock[i].Vendor_Managed) {
			continue;
		}

		SC_INFO("Clock: %s", Clocks->Clock[i].Name);
		FD = open(Clocks->Clock[i].Sysfs_Path, O_RDONLY);
		ReadBuffer[0] = '\0';
		if (read(FD, ReadBuffer, sizeof(ReadBuffer)-1) == -1) {
			SC_ERR("failed to access clock %s",
			       Clocks->Clock[i].Name);
			SC_PRINT("%s: FAIL", BIT_p->Name);
			(void) close(FD);
			return -1;
		}
		(void) close(FD);
		/* Allow up to 1000 Hz delta */
		Delta = 0.001;
		Freq = strtod(ReadBuffer, NULL) / 1000000.0;	// In MHz
		Lower = Clocks->Clock[i].Default_Freq - Delta;
		Upper = Clocks->Clock[i].Default_Freq + Delta;
		if (Freq < Lower || Freq > Upper) {
			SC_ERR("%s: BIT failed for clock %s", BIT_p->Name,
			    Clocks->Clock[i].Name);
			SC_PRINT("%s: FAIL", BIT_p->Name);
			return 0;
		}
	}

	SC_PRINT("%s: PASS", BIT_p->Name);
	return 0;
}

/*
 * Run the test using XSDB.
 */
int
XSDB_BIT(void *Arg1, void *Arg2)
{
	BIT_t *BIT_p = Arg1;
	int Level = *(int *)Arg2;
	char Output[STRLEN_MAX] = { 0 };
	char TCL_File[STRLEN_MAX];
	char TCL_Args[STRLEN_MAX] = { 0 };
	char TclCmd[STRLEN_MAX]; /* TCL file and or argument with space delimter */
	char *TclFile, *TestBitIndex;
	Default_PDI_t *Default_PDI;
	char *ImageID, *UniqueID;
	int Ret;

	Remove_BIT_Log();

	if (Level > BIT_p->Levels) {
		SC_ERR("Invalid level invocation");
		return -1;
	}

	if (BIT_p->Manual) {
		SC_PRINT("%s", BIT_p->Level[Level].Instruction);
	}

	(void) strcpy(TclCmd, BIT_p->Level[Level].TCL_File);
	TclFile = strtok(TclCmd, " ");
	(void) sprintf(TCL_File, "%s%s", BIT_PATH, TclFile);
	TestBitIndex = strtok(NULL, " ");

	Default_PDI = Plat_Devs->Default_PDI;
	if (Default_PDI == NULL) {
		SC_ERR("no default PDI is defined");
		return -1;
	}

	ImageID = Default_PDI->ImageID;
	if (strcmp(Silicon_Revision, "ES1") == 0) {
		UniqueID = Default_PDI->UniqueID_Rev0;
	} else {
		UniqueID = Default_PDI->UniqueID_Rev1;
	}

	if (strcmp(TclFile, BIT_LOAD_TCL) == 0) {
		if (TestBitIndex == NULL) {
			(void) sprintf(TCL_Args, "%s %s %s", Board_Name, ImageID, UniqueID);
		} else {
			(void) sprintf(TCL_Args, "%s %s %s %s", Board_Name, ImageID, UniqueID, TestBitIndex);
		}
	} else {
		if (TestBitIndex != NULL) {
			(void) sprintf(TCL_Args, "%s", TestBitIndex);
		}
	}

	Ret = XSDB_Op(TCL_File, TCL_Args, Output, sizeof(Output));
	if (Ret != 0) {
		SC_ERR("failed the xsdb operation");
		if (!BIT_p->Manual) {
			SC_PRINT("%s: FAIL", BIT_p->Name);
		}

		return -1;
	}

	if (!BIT_p->Manual) {
		SC_PRINT("%s: %s", BIT_p->Name, strtok(Output, "\n"));
	}

	return 0;
}

/*
 * This routine checks whether EEPROM of EBM daughter card is accessible.
 */
int
EBM_EEPROM_Check(void *Arg1, __attribute__((unused)) void *Arg2)
{
	BIT_t *BIT_p = Arg1;
	Daughter_Card_t *Daughter_Card;
	int FD;
	char Buffer[1];

	Remove_BIT_Log();

	Daughter_Card = Plat_Devs->Daughter_Card;
	if (Daughter_Card == NULL) {
		SC_ERR("EBM operation is not supported");
		return -1;
	}

	FD = open(Daughter_Card->I2C_Bus, O_RDWR);
	if (FD < 0) {
		SC_ERR("unable to open I2C bus %s: %m", Daughter_Card->I2C_Bus);
		SC_PRINT("%s: FAIL", BIT_p->Name);
		return -1;
	}

	if (ioctl(FD, I2C_SLAVE_FORCE, Daughter_Card->I2C_Address) < 0) {
		SC_ERR("failed to configure I2C bus for access to "
		       "device address %#x: %m", Daughter_Card->I2C_Address);
		SC_PRINT("%s: FAIL", BIT_p->Name);
		(void) close(FD);
		return -1;
	}

	if (read(FD, Buffer, 1) != 1) {
		SC_ERR("unable to access EEPROM device %#x",
		       Daughter_Card->I2C_Address);
		SC_PRINT("%s: FAIL", BIT_p->Name);
		(void) close(FD);
		return -1;
	}

	(void) close(FD);
	SC_PRINT("%s: PASS", BIT_p->Name);
	return 0;
}

/*
 * This routine reads DRAM type from DIMM EEPROM to validate that
 * it is accessible.
 */
int
DIMM_EEPROM_Check(void *Arg1, __attribute__((unused)) void *Arg2)
{
	BIT_t *BIT_p = Arg1;
	int FD;
	DIMMs_t *DIMMs;
	DIMM_t *DIMM;
	char In_Buffer[STRLEN_MAX];
	char Out_Buffer[STRLEN_MAX];
	int Ret = 0;

	Remove_BIT_Log();

	DIMMs = Plat_Devs->DIMMs;
	if (DIMMs == NULL) {
		SC_ERR("ddr operation is not supported");
		return -1;
	}

	for (int i = 0; i < DIMMs->Numbers; i++) {
		DIMM = &DIMMs->DIMM[i];
		SC_INFO("DIMM: %s", DIMM->Name);
		FD = open(DIMM->I2C_Bus, O_RDWR);
		if (FD < 0) {
			SC_ERR("unable to open I2C bus %s: %m", DIMM->I2C_Bus);
			SC_PRINT("%s: FAIL", BIT_p->Name);
			return -1;
		}

		(void) memset(Out_Buffer, 0, STRLEN_MAX);
		(void) memset(In_Buffer, 0, STRLEN_MAX);
		Out_Buffer[0] = 0x2;	// Byte 2: DRAM Device Type
		I2C_READ(FD, DIMM->I2C_Address_SPD, 1, Out_Buffer, In_Buffer, Ret);
		if (Ret != 0) {
			SC_PRINT("%s: FAIL", BIT_p->Name);
			(void) close(FD);
			return Ret;
		}

		if (In_Buffer[0] != 0xC) {
			SC_ERR("DIMM is not DDR4");
			SC_PRINT("%s: FAIL", BIT_p->Name);
			(void) close(FD);
			return -1;
		}

		(void) close(FD);
	}

	SC_PRINT("%s: PASS", BIT_p->Name);
	return 0;
}

/*
 * This routine gets the voltage from a regulator and compares it against
 * expected minimum and maximum values.
 */
int
Voltages_Check(void *Arg1, __attribute__((unused)) void *Arg2)
{
	BIT_t *BIT_p = Arg1;
	Voltages_t *Voltages;
	Voltage_t *Regulator;
	float Voltage;

	Remove_BIT_Log();

	Voltages = Plat_Devs->Voltages;
	if (Voltages == NULL) {
		SC_ERR("voltage operation is not supported");
		return -1;
	}

	for (int i = 0; i < Voltages->Numbers; i++) {
		Regulator = &Voltages->Voltage[i];
		SC_INFO("Voltage: %s", Regulator->Name);
		if (Access_Regulator(Regulator, &Voltage, 0) != 0) {
			SC_ERR("failed to get voltage for %s",
			    Regulator->Name);
			SC_PRINT("%s: FAIL", BIT_p->Name);
			return -1;
		}

		if (((Regulator->Minimum_Volt != -1) &&
		     (Voltage < Regulator->Minimum_Volt)) ||
		    ((Regulator->Maximum_Volt != -1) &&
		     (Voltage > Regulator->Maximum_Volt))) {
			SC_ERR("voltage for %s is out-of-range",
			   Regulator->Name);
			SC_PRINT("%s: FAIL", BIT_p->Name);
			return -1;
		}
	}

	SC_PRINT("%s: PASS", BIT_p->Name);
	return 0;
}

/*
 * This routine is used by manual tests that require displaying
 * instructions without performing any action.
 */
int
Display_Instruction(void *Arg1, void *Arg2)
{
	BIT_t *BIT_p = Arg1;
	int Level = *(int *)Arg2;

	Remove_BIT_Log();

	if (!BIT_p->Manual) {
		SC_ERR("BIT is not a manual test");
		return -1;
	}

	if (Level > BIT_p->Levels) {
		SC_ERR("invalid level invocation for a multi-level manual BIT");
		return -1;
	}

	SC_PRINT("%s", BIT_p->Level[Level].Instruction);
	return 0;
}

/*
 * This routine asserts reset
 */
int
Assert_Reset(void *Arg1, void *Arg2)
{
	BIT_t *BIT_p = Arg1;
	int Level  = *(int *)Arg2;
	int Ret = 0;

	if (Level > BIT_p->Levels) {
		SC_ERR("Invalid level invocation");
		return -1;
	}

	if (BIT_p->Manual) {
		SC_PRINT("%s", BIT_p->Level[Level].Instruction);
	}

	Ret = Reset_Op();
	if (Ret != 0) {
		SC_ERR("failed the reset operation");
		if (!BIT_p->Manual) {
			SC_PRINT("%s: FAIL", BIT_p->Name);
		}

		return -1;
	}

	if (!BIT_p->Manual) {
		SC_PRINT("%s: PASS", BIT_p->Name);
	}

	return 0;
}

int
DDRMC_Test(void *Arg1, __attribute__((unused)) void *Arg2)
{
	BIT_t *BIT_p = Arg1;
	FILE *FP;
	char System_Cmd[SYSCMD_MAX];
	char Buffer[STRLEN_MAX];
	int DDRMC;
	Default_PDI_t *Default_PDI;
	char *ImageID, *UniqueID;
	int Ret = 0;

	Remove_BIT_Log();

	(void) sprintf(Buffer, "%s", BIT_PATH);
	(void) sprintf(System_Cmd, "%sddrmc_check.py", Buffer);
	if (access(System_Cmd, F_OK) != 0) {
		SC_ERR("failed to access file %s: %m", System_Cmd);
		return -1;
	}

	DDRMC = atoi(BIT_p->Level[0].Arg);

	Default_PDI = Plat_Devs->Default_PDI;
	if (Default_PDI == NULL) {
		SC_ERR("no default PDI is defined");
		return -1;
	}

	ImageID = Default_PDI->ImageID;
	if (strcmp(Silicon_Revision, "ES1") == 0) {
		UniqueID = Default_PDI->UniqueID_Rev0;
	} else {
		UniqueID = Default_PDI->UniqueID_Rev1;
	}

	(void) JTAG_Op(1);
	(void) sprintf(System_Cmd, "cd %s; python3 ddrmc_check.py %d %s %s %s 2>&1 | tee %s",
		       Buffer, DDRMC, Board_Name, ImageID, UniqueID, BITLOGFILE);
	SC_INFO("Command: %s", System_Cmd);
	FP = popen(System_Cmd, "r");
	if (FP == NULL) {
		SC_ERR("failed to invoke %s: %m", System_Cmd);
		Ret = -1;
		goto Out;
	}

	while (fgets(Buffer, STRLEN_MAX, FP)) {
		SC_INFO("%s", Buffer);
		if ((strstr(Buffer, "ERROR: ") != NULL)) {
			SC_PRINT_N("%s", Buffer);
			Ret = -1;
			(void) pclose(FP);
			goto Out;
		}

		if ((strstr(Buffer, "Calibration Status: ") != NULL)) {
			break;
		}
	}

	if ((strstr(Buffer, "PASS") != NULL)) {
		SC_PRINT("%s: PASS", BIT_p->Name);
	} else {
		SC_PRINT("%s: FAIL", BIT_p->Name);
		Ret = -1;
	}

	(void) pclose(FP);

Out:
	(void) JTAG_Op(0);
	return Ret;
}
