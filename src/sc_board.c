/*
 * Copyright (c) 2020 - 2022 Xilinx, Inc.  All rights reserved.
 * Copyright (c) 2022 - 2025 Advanced Micro Devices, Inc.  All rights reserved.
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
extern char Silicon_Revision[];

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
	Default_PDI_t *Default_PDI;
	char *ImageID, *UniqueID;

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

	Default_PDI = Plat_Devs->Default_PDI;
	if (Default_PDI == NULL) {
		SC_ERR("no default PDI is defined");
		return -1;
	}

	ImageID = Default_PDI->ImageID;
	if (Get_Silicon_Revision(Silicon_Revision) != 0) {
		return -1;
	}

	if (strcmp(Silicon_Revision, "ES1") == 0) {
		UniqueID = Default_PDI->UniqueID_Rev0;
	} else {
		UniqueID = Default_PDI->UniqueID_Rev1;
	}

	/* State == 1 */
	(void) sprintf(TCL_File, "%s%s", SCRIPT_PATH, TCL_CMD_TCL);
	(void) sprintf(TCL_Args, "%s %s %s", ImageID, UniqueID, LOAD_DEFAULT_PDI_CMD);
	return XSDB_Op(TCL_File, TCL_Args, Output, sizeof(Output));
}
