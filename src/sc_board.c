/*
 * Copyright (c) 2020 - 2022 Xilinx, Inc.  All rights reserved.
 *
 * SPDX-License-Identifier: MIT
 */

#include <stdio.h>
#include <fcntl.h>
#include <unistd.h>
#include "sc_app.h"

#define QSFP_MODSEL_TCL	"qsfp_set_modsel/qsfp_download.tcl"

extern Plat_Devs_t *Plat_Devs;

extern int Access_Regulator(Voltage_t *, float *, int);
extern int Reset_Op(void);
extern int JTAG_Op(int);
extern int Reset_IDT_8A34001(void);

/*
 * On-board EEPROM
 */
OnBoard_EEPROM_t Legacy_OnBoard_EEPROM = {
	.Name = "onboard",
	.I2C_Bus = "/dev/i2c-11",
	.I2C_Address = 0x54,
};

OnBoard_EEPROM_t Common_OnBoard_EEPROM = {
	.Name = "onboard",
	.I2C_Bus = "/dev/i2c-1",
	.I2C_Address = 0x54,
};

int
VCK190_ES1_Vccaux_Workaround(void *Arg)
{
	int *State;
	int Target_Index = -1;
	Voltages_t *Voltages;
	Voltage_t *Regulator;
	float Voltage;

	State = (int *)Arg;
	Voltages = Plat_Devs->Voltages;
	if (Voltages == NULL) {
		SC_ERR("voltage operation is not suppoprted");
		return -1;
	}

	for (int i = 0; i < Voltages->Numbers; i++) {
		if (strcmp("VCC_RAM", (char *)Voltages->Voltage[i].Name) == 0) {
			Target_Index = i;
			Regulator = &Voltages->Voltage[Target_Index];
			break;
		}
	}

	if (Target_Index == -1) {
		SC_ERR("failed to locate VCC_RAM regulator");
		return -1;
	}

	Voltage = (*State == 1) ? Regulator->Typical_Volt : 0;
	if (Access_Regulator(Regulator, &Voltage, 1) != 0) {
		SC_ERR("failed to set VCC_RAM regulator to %0.3f v", Voltage);
		return -1;
	}

	return 0;
}

/*
 * The QSFP must be selected and not held in Reset (High) in order to be
 * accessed.  These lines are driven by Vesal.  The QSFP1_RESETL_LS high
 * has a pull up, but QSFP1_MODSKLL_LS has no pull down.  If Versal is not
 * programmed to drive QSFP1_MODSKLL_LS low, the QSFP will not respond.
 */
int
VCK190_QSFP_ModuleSelect(QSFP_t *Arg, int State)
{
	FILE *FP;
	char Output[STRLEN_MAX] = { 0 };
	char System_Cmd[SYSCMD_MAX];

	if (State != 0 && State != 1) {
		SC_ERR("invalid QSFP module select state");
		return -1;
	}

	/*
	 * Call with 'State == 1' may change the current boot mode to JTAG
	 * to download a PDI.  Calling Reset_Op() restores the current
	 * boot mode.
	 */
	if (State == 0) {
		(void) Reset_Op();
		return 0;
	}

	/* State == 1 */
	(void) JTAG_Op(1);
	sprintf(System_Cmd, "%s; %s %s%s 2>&1", XSDB_ENV, XSDB_CMD, BIT_PATH,
	    QSFP_MODSEL_TCL);
	SC_INFO("Command: %s", System_Cmd);
	FP = popen(System_Cmd, "r");
	if (FP == NULL) {
		SC_ERR("failed to invoke xsdb: %m");
		return -1;
	}

	(void) fgets(Output, sizeof(Output), FP);
	(void) pclose(FP);
	SC_INFO("XSDB Output: %s", Output);
	(void) JTAG_Op(0);
	if (strstr(Output, "no targets found") != NULL) {
		SC_ERR("could not connect to Versal through jtag");
		return -1;
	}

	return 0;
}
