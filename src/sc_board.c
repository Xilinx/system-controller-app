/*
 * Copyright (c) 2020 - 2022 Xilinx, Inc.  All rights reserved.
 * Copyright (c) 2022 - 2024 Advanced Micro Devices, Inc.  All rights reserved.
 *
 * SPDX-License-Identifier: MIT
 */

#include <stdio.h>
#include <stdlib.h>
#include <fcntl.h>
#include <ctype.h>
#include <unistd.h>
#include <libgen.h>
#include "sc_app.h"

extern Plat_Devs_t *Plat_Devs;
extern char Board_Name[];

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
VCK190_QSFP_ModuleSelect(__attribute__((unused)) SFP_t *Arg, int State)
{
	char TCL_File[STRLEN_MAX];
	char TCL_Args[STRLEN_MAX];
	char Output[STRLEN_MAX] = { 0 };

	if (State != 0 && State != 1) {
		SC_ERR("invalid SFP module select state");
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
	(void) sprintf(TCL_File, "%s%s", BIT_PATH, QSFP_MODSEL_TCL);
	(void) sprintf(TCL_Args, "%s", Board_Name);
	return XSDB_Op(TCL_File, TCL_Args, Output, sizeof(Output));
}
