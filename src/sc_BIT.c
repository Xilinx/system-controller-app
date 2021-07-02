/*
 * Copyright (c) 2020 - 2021 Xilinx, Inc.  All rights reserved.
 *
 * SPDX-License-Identifier: MIT
 */

#include <stdio.h>
#include <fcntl.h>
#include <unistd.h>
#include <stdlib.h>
#include <string.h>
#include "sc_app.h"

extern Plat_Devs_t *Plat_Devs;
extern Plat_Ops_t *Plat_Ops;

int Clocks_Check(void *);
int XSDB_BIT(void *);
int EBM_EEPROM_Check(void *);
int DIMM_EEPROM_Check(void *);
int Voltages_Check(void *);
extern int Access_Regulator(Voltage_t *, float *, int);

/*
 * BITs
 */
typedef enum {
	BIT_CLOCKS_CHECK,
	BIT_IDCODE_CHECK,
	BIT_EFUSE_CHECK,
	BIT_EBM_EEPROM_CHECK,
	BIT_DIMM_EEPROM_CHECK,
	BIT_VOLTAGES_CHECK,
	BIT_MAX,
} BIT_Index;

BITs_t BITs = {
	.Numbers = BIT_MAX,
	.BIT[BIT_CLOCKS_CHECK] = {
		.Name = "Check Clocks",		// Name of BIT to run
		.Plat_BIT_Op = Clocks_Check,	// BIT routine to invoke
	},
	.BIT[BIT_IDCODE_CHECK] = {
		.Name = "IDCODE Check",
		.TCL_File = "idcode/idcode_check.tcl",
		.Plat_BIT_Op = XSDB_BIT,
	},
	.BIT[BIT_EFUSE_CHECK] = {
		.Name = "EFUSE Check",
		.TCL_File = "efuse/read_efuse.tcl",
		.Plat_BIT_Op = XSDB_BIT,
	},
	.BIT[BIT_EBM_EEPROM_CHECK] = {
		.Name = "X-EBM EEPROM Check",
		.Plat_BIT_Op = EBM_EEPROM_Check,
	},
	.BIT[BIT_DIMM_EEPROM_CHECK] = {
		.Name = "DIMM EEPROM Check",
		.Plat_BIT_Op = DIMM_EEPROM_Check,
	},
	.BIT[BIT_VOLTAGES_CHECK] = {
		.Name = "Check Voltages",
		.Plat_BIT_Op = Voltages_Check,
	},
};

/*
 * This test validates whether the current clock frequency is
 * the same as default value.
 */
int
Clocks_Check(void *Arg)
{
	Clocks_t *Clocks;
	BIT_t *BIT_p = Arg;
	int FD;
	char ReadBuffer[STRLEN_MAX];
	double Freq, Lower, Upper, Delta;

	Clocks = Plat_Devs->Clocks;
	if (Clocks == NULL) {
		SC_ERR("clock operation is not supported");
		return -1;
	}

	for (int i = 0; i < Clocks->Numbers; i++) {
		if (Clocks->Clock[i].Type == IDT_8A34001) {
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
XSDB_BIT(void *Arg)
{
	BIT_t *BIT_p = Arg;
	char Output[STRLEN_MAX] = { 0 };

	if (Plat_Ops->XSDB_Op == NULL) {
		SC_ERR("xsdb operation is not supported");
		return -1;
	}

	if (Plat_Ops->XSDB_Op(BIT_p->TCL_File, Output, STRLEN_MAX) != 0) {
		SC_PRINT("%s: FAIL", BIT_p->Name);
	} else {
		SC_PRINT("%s: PASS", BIT_p->Name);
	}

	return 0;
}

/*
 * This routine checks whether EEPROM of EBM daughter card is accessible.
 */
int
EBM_EEPROM_Check(void *Arg)
{
	BIT_t *BIT_p = Arg;
	Daughter_Card_t *Daughter_Card;
	int FD;

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
DIMM_EEPROM_Check(void *Arg)
{
	BIT_t *BIT_p = Arg;
	int FD;
	DIMM_t *DIMM;
	char In_Buffer[STRLEN_MAX];
	char Out_Buffer[STRLEN_MAX];
	int Ret = 0;

	DIMM = Plat_Devs->DIMM;
	if (DIMM == NULL) {
		SC_ERR("ddr operation is not supported");
		return -1;
	}

	FD = open(DIMM->I2C_Bus, O_RDWR);
	if (FD < 0) {
		SC_ERR("unable to open I2C bus %s: %m", DIMM->I2C_Bus);
		SC_PRINT("%s: FAIL", BIT_p->Name);
		return -1;
	}

	if (ioctl(FD, I2C_SLAVE_FORCE, DIMM->Spd.Bus_addr) < 0) {
		SC_ERR("unable to access EEPROM device %#x: %m",
		       DIMM->Spd.Bus_addr);
		SC_PRINT("%s: FAIL", BIT_p->Name);
		(void) close(FD);
		return -1;
	}

	(void) memset(Out_Buffer, 0, STRLEN_MAX);
	(void) memset(In_Buffer, 0, STRLEN_MAX);
	Out_Buffer[0] = 0x2;	// Byte 2: DRAM Device Type
	I2C_READ(FD, DIMM->Spd.Bus_addr, 1, Out_Buffer, In_Buffer, Ret);
	if (Ret != 0) {
		SC_PRINT("%s: FAIL", BIT_p->Name);
		(void) close(FD);
		return Ret;
	}

	if (In_Buffer[0] != 0xc) {
		SC_ERR("DIMM is not DDR4");
		SC_PRINT("%s: FAIL", BIT_p->Name);
		(void) close(FD);
		return -1;
	}

	(void) close(FD);
	SC_PRINT("%s: PASS", BIT_p->Name);
	return 0;
}

/*
 * This routine gets the voltage from a regulator and compares it against
 * expected minimum and maximum values.
 */
int
Voltages_Check(void *Arg)
{
	BIT_t *BIT_p = Arg;
	Voltages_t *Voltages;
	Voltage_t *Regulator;
	float Voltage;

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

		if ((Voltage < Regulator->Minimum_Volt) ||
		    (Voltage > Regulator->Maximum_Volt)) {
			SC_ERR("voltage for %s is out-of-range",
			   Regulator->Name);
			SC_PRINT("%s: FAIL", BIT_p->Name);
			return -1;
		}
	}

	SC_PRINT("%s: PASS", BIT_p->Name);
	return 0;
}
