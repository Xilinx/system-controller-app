/*
 * Copyright (c) 2021 Xilinx, Inc.  All rights reserved.
 *
 * SPDX-License-Identifier: MIT
 */

#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>
#include <string.h>
#include "sc_app.h"

extern Plat_Devs_t *Plat_Devs;
extern Plat_Ops_t *Plat_Ops;

int VPK120_IDT_8A34001_Reset(void);
extern int Access_Regulator(Voltage_t *, float *, int);
extern int Access_IO_Exp(IO_Exp_t *, int, int, unsigned int *);
extern int FMC_Vadj_Range(FMC_t *, float *, float *);
extern int GPIO_Get(char *, int *);
extern int GPIO_Set(char *, int);

IDT_8A34001_Data_t VPK120_IDT_8A34001_Data = {
	.Number_Label = 12,
	.Display_Label = { "Q0 - From 8A34001 Q0 to 8A34001 CLK0", \
			   "Q1 - From Bank 703 to Bank 206 GTM RX2", \
			   "Q2 - From Bank 206 GTM TX2 to Bank 703", \
			   "Q3 - From 8A34001 Q4 to SMA J339", \
			   "Q4 - From SMA J330-331 to 8A34001 CLK3", \
			   "Q5 - From Bank 202/204 GTM REFCLK1 to J328", \
			   "Q6 - From Bank 206 GTM REFCLK1 to Bank 711", \
			   "Q7 - From FMC REFCLK M2C to Bank 206 GTM REFCLK0", \
			   "Q8 - To Bank 204/205 GTM REFCLKP0", \
			   "Q9 - To Bank 202/203 GTM REFCLKP0", \
			   "Q10 - To SMA J328", \
			   "Q11 - To N.C.", \
	},
	.Internal_Label = { "OUT0DesiredFrequency", \
			    "OUT1DesiredFrequency", \
			    "OUT2DesiredFrequency", \
			    "OUT3DesiredFrequency", \
			    "OUT4DesiredFrequency", \
			    "OUT5DesiredFrequency", \
			    "OUT6DesiredFrequency", \
			    "OUT7DesiredFrequency", \
			    "OUT8DesiredFrequency", \
			    "OUT9DesiredFrequency", \
			    "OUT10DesiredFrequency", \
			    "OUT11DesiredFrequency", \
	},
	.Chip_Reset = VPK120_IDT_8A34001_Reset,
};

OnBoard_EEPROM_t VPK120_OnBoard_EEPROM = {
	.Name = "onboard",
	.I2C_Bus = "/dev/i2c-10",
	.I2C_Address = 0x54,
};

/*
 * Board-specific Devices
 */
Plat_Devs_t VPK120_Devs = {
	.OnBoard_EEPROM = &VPK120_OnBoard_EEPROM,
};

void
VPK120_Version_Op(int *Major, int *Minor)
{
	*Major = 1;
	*Minor = 0;
}

int
VPK120_BootMode_Op(BootMode_t *BootMode, int Op)
{
	for (int i = 0; i < 4; i++) {
		if (GPIO_Set(Plat_Devs->BootModes->Mode_Lines[i],
		    ((BootMode->Value >> i) & 0x1)) != 0) {
			SC_ERR("failed to set GPIO line %s",
			       Plat_Devs->BootModes->Mode_Lines[i]);
			return -1;
		}
	}

	return 0;
}

int
VPK120_Reset_Op(void)
{
	/* Assert POR */
	if (GPIO_Set("SYSCTLR_POR_B_LS", 0) != 0) {
		SC_ERR("failed to assert power-on-reset");
		return -1;
	}

	sleep(1);

	/* De-assert POR */
	if (GPIO_Set("SYSCTLR_POR_B_LS", 1) != 0) {
		SC_ERR("failed to de-assert power-on-reset");
		return -1;
	}

	return 0;
}

int
VPK120_JTAG_Op(int Select)
{
	int State;

	switch (Select) {
	case 1:
		/* Overwrite the default JTAG switch */
		if (GPIO_Set("SYSCTLR_JTAG_S0", 0) != 0) {
			SC_ERR("failed to set JTAG 0");
			return -1;
		}

		if (GPIO_Set("SYSCTLR_JTAG_S1", 0) != 0) {
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
		if (GPIO_Get("SYSCTLR_JTAG_S0", &State) != 0) {
			SC_ERR("failed to release JTAG 0");
			return -1;
		}

		if (GPIO_Get("SYSCTLR_JTAG_S1", &State) != 0) {
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
VPK120_XSDB_Op(const char *TCL_File, char *Output, int Length)
{
	FILE *FP;
	char System_Cmd[SYSCMD_MAX];
	int Ret = 0;

	(void) sprintf(System_Cmd, "%s%s", BIT_PATH, TCL_File);
	if (access(System_Cmd, F_OK) != 0) {
		SC_ERR("failed to access file %s: %m", System_Cmd);
		return -1;
	}

	if (Output == NULL) {
		SC_ERR("unallocated output buffer");
		return -1;
	}

	(void) VPK120_JTAG_Op(1);
	(void) sprintf(System_Cmd, "%s; %s %s%s 2>&1", XSDB_ENV, XSDB_CMD,
		       BIT_PATH, TCL_File);
	SC_INFO("Command: %s", System_Cmd);
	FP = popen(System_Cmd, "r");
	if (FP == NULL) {
		SC_ERR("failed to invoke xsdb");
		Ret = -1;
		goto Out;
	}

	(void) fgets(Output, Length, FP);
	(void) pclose(FP);
	SC_INFO("XSDB Output: %s", Output);
	if (strstr(Output, "no targets found") != NULL) {
		SC_ERR("could not connect to Versal through jtag");
		Ret = -1;
	}

Out:
	(void) VPK120_JTAG_Op(0);
	return Ret;
}

int
VPK120_IDCODE_Op(char *Output, int Length)
{
	return VPK120_XSDB_Op(IDCODE_TCL, Output, Length);
}

int
VPK120_QSFP_ModuleSelect_Op(QSFP_t *QSFP, int State)
{
	IO_Exp_t *IO_Exp;
	unsigned int Value;

	if (State != 0 && State != 1) {
		SC_ERR("invalid QSFP module select state");
		return -1;
	}

	/* Nothing to do for 'State == 0' */
	if (State == 0) {
		return 0;
	}

	/* State == 1 */
	IO_Exp = Plat_Devs->IO_Exp;

	/* Set direction */
	Value = 0x73DF;
	if (Access_IO_Exp(IO_Exp, 1, 0x6, &Value) != 0) {
		SC_ERR("failed to set IO expander direction");
		return -1;
	}

	/*
	 * Only one QSFP-DD can be referenced at a time since both
	 * have address 0x50 and are on the same I2C bus, so make
	 * sure the other QSFP-DD is not selected.
	 */
	if (strcmp(QSFP->Name, "QSFPDD1") == 0) {
		Value = 0x820;
	} else {
		Value = 0x420;
	}

	if (Access_IO_Exp(IO_Exp, 1, 0x2, &Value) != 0) {
		SC_ERR("failed to set IO expander output");
		return -1;
	}

	return 0;
}

int
VPK120_FMCAutoVadj_Op(void)
{
	int Target_Index = -1;
	IO_Exp_t *IO_Exp;
	unsigned int Value;
	Voltages_t *Voltages;
	Voltage_t *Regulator;
	FMCs_t *FMCs;
	FMC_t *FMC;
	int Plugged = 0;
	float Voltage = 0;
	float Min_Voltage = 0;
	float Max_Voltage = 0;

	IO_Exp = Plat_Devs->IO_Exp;
	if (Access_IO_Exp(IO_Exp, 0, 0x0, &Value) != 0) {
		SC_ERR("failed to read input of IO Expander");
		return -1;
	}

	SC_INFO("IO Expander input: %#x", Value);

	/* If bit[0] is 0, then a FMC module is connected */
	if ((~Value & 0x1) == 0x1) {
		Plugged = 1;
	}

	Voltages  = Plat_Devs->Voltages;
	for (int i = 0; i < Voltages->Numbers; i++) {
		if (strcmp("VADJ_FMC", (char *)Voltages->Voltage[i].Name) == 0) {
			Target_Index = i;
			Regulator = &Voltages->Voltage[Target_Index];
			break;
		}
	}

	if (Target_Index == -1) {
		SC_ERR("no regulator exists for VADJ_FMC");
		return -1;
	}

	if (Plugged == 1) {
		FMCs = Plat_Devs->FMCs;
		FMC = &FMCs->FMC[0];
		if (FMC_Vadj_Range(FMC, &Min_Voltage, &Max_Voltage) != 0) {
			SC_ERR("failed to get Voltage Adjust range for FMC");
			return -1;
		}
	} else {
		Min_Voltage = 0;
		Max_Voltage = 1.5;
	}

	SC_INFO("Voltage Min: %.2f, Voltage Max: %.2f",
		Min_Voltage, Max_Voltage);

	/*
	 * Start with satisfying the lower target voltage and then see
	 * if it does also satisfy the higher voltage.
	 */
	if (Min_Voltage <= 1.2 && Max_Voltage >= 1.2) {
		Voltage = 1.2;
	}

	if (Min_Voltage <= 1.5 && Max_Voltage >= 1.5) {
		Voltage = 1.5;
	}

	if (Access_Regulator(Regulator, &Voltage, 1) != 0) {
		SC_ERR("failed to set voltage of VADJ_FMC regulator");
		return -1;
	}

	return 0;
}

int
VPK120_IDT_8A34001_Reset(void)
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

	sleep (1);

	Value = 0x20;   // De-assert reset
	if (Access_IO_Exp(IO_Exp, 1, 0x2, &Value) != 0) {
		SC_ERR("failed to de-assert reset of 8A34001 chip");
		return -1;
	}

	return 0;
}

/*
 * Board-specific Operations
 */
Plat_Ops_t VPK120_Ops = {
	.Version_Op = VPK120_Version_Op,
	.BootMode_Op = VPK120_BootMode_Op,
	.Reset_Op = VPK120_Reset_Op,
	.IDCODE_Op = VPK120_IDCODE_Op,
	.XSDB_Op = VPK120_XSDB_Op,
	.QSFP_ModuleSelect_Op = VPK120_QSFP_ModuleSelect_Op,
	.FMCAutoVadj_Op = VPK120_FMCAutoVadj_Op,
};
