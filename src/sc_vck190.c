/*
 * Copyright (c) 2020 - 2021 Xilinx, Inc.  All rights reserved.
 *
 * SPDX-License-Identifier: MIT
 */

#include <stdio.h>
#include <stdlib.h>
#include <fcntl.h>
#include <unistd.h>
#include <time.h>
#include <string.h>
#include <sys/ioctl.h>
#include <linux/i2c-dev.h>
#include "sc_app.h"

#define BOOTMODE_TCL	"boot_mode/alt_boot_mode.tcl"
#define QSFP_MODSEL_TCL	"qsfp_set_modsel/qsfp_download.tcl"

extern Plat_Devs_t *Plat_Devs;
extern Plat_Ops_t *Plat_Ops;

int VCK190_JTAG_Op(int);
int VCK190_XSDB_Op(const char *, char *, int);
int VCK190_IDT_8A34001_Reset(void);
int Workaround_Vccaux(void *Arg);
extern char *Appfile(char *);
extern int Access_Regulator(Voltage_t *, float *, int);
extern int Access_IO_Exp(IO_Exp_t *, int, int, unsigned int *);
extern int FMC_Vadj_Range(FMC_t *, float *, float *);
extern int GPIO_Get(char *, int *);
extern int GPIO_Set(char *, int);

IDT_8A34001_Data_t VCK190_IDT_8A34001_Data = {
	.Number_Label = 14,
	.Display_Label = { "CIN 1 From Bank 200 GTY_REF", \
			   "CIN 2 From Bank 706", \
			   "Q0 to CIN 0", \
			   "Q1 to Bank 200 GTY_REF", \
			   "Q2 to Bank 201, 204-206 GTY_REF", \
			   "Q3 to FMC2 REFCLK", \
			   "Q4 to FMC2 SYNC", \
			   "Q5 to J328", \
			   "Q6 to NC", \
			   "Q7 to NC", \
			   "Q8 to NC", \
			   "Q9 to NC", \
			   "Q10 to NC", \
			   "Q11 to NC", \
	},
	.Internal_Label = { "IN1_Frequency", \
			    "IN2_Frequency", \
			    "OUT0DesiredFrequency", \
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
	.Chip_Reset = VCK190_IDT_8A34001_Reset,
};

/*
 * On-board EEPROM
 */
OnBoard_EEPROM_t VCK190_OnBoard_EEPROM = {
	.Name = "onboard",
	.I2C_Bus = "/dev/i2c-11",
	.I2C_Address = 0x54,
};

/*
 * VCK190-sepcific Devices
 */
Plat_Devs_t VCK190_Devs = {
	.OnBoard_EEPROM = &VCK190_OnBoard_EEPROM,
};

int
Workaround_Vccaux(void *Arg)
{
	int fd;
	int *State;
	char WriteBuffer[10];

	State = (int *)Arg;

	fd = open("/dev/i2c-3", O_RDWR);
	if (fd < 0) {
		SC_ERR("cannot open the I2C device %s: %m", "/dev/i2c-3");
		return -1;
	}

	// Select IRPS5401
	if (ioctl(fd, I2C_SLAVE_FORCE, 0x47) < 0) {
		SC_ERR("unable to access IRPS5401 address");
		(void) close(fd);
		return -1;
	}

	// Set the page for the IRPS5401
	WriteBuffer[0] = 0x00;
	WriteBuffer[1] = 0x03;
	if (write(fd, WriteBuffer, 2) != 2) {
		SC_ERR("unable to select page for IRPS5401");
		(void) close(fd);
		return -1;
	}

	// Change the state of VOUT (ON: State = 1, OFF: State = 0)
	WriteBuffer[0] = 0x01;
	WriteBuffer[1] = (1 == *State) ? 0x80 : 0x00;
	if (write(fd, WriteBuffer, 2) != 2) {
		SC_ERR("unable to change VOUT for IRPS5401");
		(void) close(fd);
		return -1;
	}

	(void) close(fd);
	return 0;
}

int
VCK190_BootMode_Op(BootMode_t *BootMode, int Op)
{
	FILE *FP;
	char Buffer[SYSCMD_MAX];

	/* Only set boot mode is supported */
	if (Op != 1) {
		SC_ERR("invalid boot mode operation");
		return -1;
	}

	/* Record the boot mode */
	FP = fopen(BOOTMODEFILE, "w");
	if (FP == NULL) {
		SC_ERR("failed to open boot_mode file %s: %m", BOOTMODEFILE);
		return -1;
	}

	(void) sprintf(Buffer, "%s\n", BootMode->Name);
	SC_INFO("Boot Mode: %s", Buffer);
	(void) fputs(Buffer, FP);
	(void) fclose(FP);

	return 0;
}

int
VCK190_SetBootMode(int Value)
{
	FILE *FP;
	char Output[STRLEN_MAX] = { 0 };
	char System_Cmd[SYSCMD_MAX];

	(void) VCK190_JTAG_Op(1);
	sprintf(System_Cmd, "%s; %s %s%s %x 2>&1", XSDB_ENV, XSDB_CMD, BIT_PATH,
	    BOOTMODE_TCL, Value);
	SC_INFO("Command: %s", System_Cmd);
	FP = popen(System_Cmd, "r");
	if (FP == NULL) {
		SC_ERR("failed to invoke xsdb: %m");
		return -1;
	}

	(void) fgets(Output, sizeof(Output), FP);
	(void) pclose(FP);
	SC_INFO("XSDB Output: %s", Output);
	(void) VCK190_JTAG_Op(0);
	if (strstr(Output, "no targets found") != NULL) {
		SC_ERR("could not connect to Versal through jtag");
		return -1;
	}

	return 0;
}

int
VCK190_Reset_Op(void)
{
	FILE *FP;
	int State;
	char Buffer[SYSCMD_MAX] = { 0 };
	BootModes_t *BootModes;
	BootMode_t *BootMode;

	if (access(SILICONFILE, F_OK) == 0) {
		FP = fopen(SILICONFILE, "r");
		if (FP == NULL) {
			SC_ERR("failed to read silicon file %s: %m", SILICONFILE);
			return -1;
		}

		(void) fgets(Buffer, SYSCMD_MAX, FP);
		(void) fclose(FP);
		SC_INFO("%s: %s", SILICONFILE, Buffer);
		if (strcmp(Buffer, "ES1\n") == 0) {
			// Turn VCCINT_RAM off
			State = 0;
			if (Workaround_Vccaux(&State) != 0) {
				SC_ERR("failed to turn VCCINT_RAM off");
				return -1;
			}
		}
	}

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

	/* If a boot mode is defined, set it after POR */
	if (access(BOOTMODEFILE, F_OK) == 0) {
		FP = fopen(BOOTMODEFILE, "r");
		if (FP == NULL) {
			SC_ERR("failed to read boot_mode file %s: %m", BOOTMODEFILE);
			return -1;
		}

		(void) fscanf(FP, "%s", Buffer);
		(void) fclose(FP);
		SC_INFO("%s: %s", BOOTMODEFILE, Buffer);
		BootModes = Plat_Devs->BootModes;
		for (int i = 0; i < BootModes->Numbers; i++) {
			BootMode = &BootModes->BootMode[i];
			if (strcmp(Buffer, (char *)BootMode->Name) == 0) {
				if (VCK190_SetBootMode(BootMode->Value) != 0) {
					SC_ERR("failed to set the boot mode");
					return -1;
				}

				break;
			}
		}
	}

	return 0;
}

int
VCK190_JTAG_Op(int Select)
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
VCK190_XSDB_Op(const char *TCL_File, char *Output, int Length)
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

	(void) VCK190_JTAG_Op(1);
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
	(void) VCK190_JTAG_Op(0);
	return Ret;
}

int
VCK190_IDCODE_Op(char *Output, int Length)
{
	return VCK190_XSDB_Op(IDCODE_TCL, Output, Length);
}

/*
 * The QSFP must be selected and not held in Reset (High) in order to be
 * accessed.  These lines are driven by Vesal.  The QSFP1_RESETL_LS high
 * has a pull up, but QSFP1_MODSKLL_LS has no pull down.  If Versal is not
 * programmed to drive QSFP1_MODSKLL_LS low, the QSFP will not respond.
 */
int
VCK190_QSFP_ModuleSelect_Op(QSFP_t *Arg, int State)
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
	 * to download a PDI.  Calling VCK190_Reset_Op() restores the current
	 * boot mode.
	 */
	if (State == 0) {
		(void) VCK190_Reset_Op();
		return 0;
	}

	/* State == 1 */
	(void) VCK190_JTAG_Op(1);
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
	(void) VCK190_JTAG_Op(0);
	if (strstr(Output, "no targets found") != NULL) {
		SC_ERR("could not connect to Versal through jtag");
		return -1;
	}

	return 0;
}

/*
 * FMC Cards
 */
typedef enum {
	FMC_1,
	FMC_2,
	FMC_MAX,
} FMC_Index;

int
VCK190_FMCAutoVadj_Op(void)
{
	int Target_Index = -1;
	IO_Exp_t *IO_Exp;
	unsigned int Value;
	Voltages_t *Voltages;
	Voltage_t *Regulator;
	FMCs_t *FMCs;
	FMC_t *FMC;
	int FMC1 = 0;
	int FMC2 = 0;
	float Voltage = 0;
	float Min_Voltage_1 = 0;
	float Max_Voltage_1 = 0;
	float Min_Voltage_2 = 0;
	float Max_Voltage_2 = 0;
	float Min_Combined, Max_Combined;

	IO_Exp = Plat_Devs->IO_Exp;
	if (Access_IO_Exp(IO_Exp, 0, 0x0, &Value) != 0) {
		SC_ERR("failed to read input of IO Expander");
		return -1;
	}

	SC_INFO("IO Expander input: %#x", Value);

	/* If bit[0] is 0, then FMC1 is connected */
	if ((~Value & 0x1) == 0x1) {
		FMC1 = 1;
	}

	/* If bit[1] is 0, then FMC2 is connected */
	if ((~Value & 0x2) == 0x2) {
		FMC2 = 1;
	}

	Voltages = Plat_Devs->Voltages;
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

	FMCs = Plat_Devs->FMCs;
	if (FMC1 == 1) {
		FMC = &FMCs->FMC[FMC_1];
		if (FMC_Vadj_Range(FMC, &Min_Voltage_1, &Max_Voltage_1) != 0) {
			SC_ERR("failed to get Voltage Adjust range for FMC1");
			return -1;
		}
	}

	if (FMC2 == 1) {
		FMC = &FMCs->FMC[FMC_2];
		if (FMC_Vadj_Range(FMC, &Min_Voltage_2, &Max_Voltage_2) != 0) {
			SC_ERR("failed to get Voltage Adjust range for FMC2");
			return -1;
		}
	}

	if (FMC1 == 1 && FMC2 == 0) {
		Min_Combined = Min_Voltage_1;
		Max_Combined = Max_Voltage_1;
	} else if (FMC1 == 0 && FMC2 == 1) {
		Min_Combined = Min_Voltage_2;
		Max_Combined = Max_Voltage_2;
	} else if (FMC1 == 1 && FMC2 == 1) {
		Min_Combined = MAX(Min_Voltage_1, Min_Voltage_2);
		Max_Combined = MIN(Max_Voltage_1, Max_Voltage_2);
	} else {
		Min_Combined = 0;
		Max_Combined = 1.5;
	}

	SC_INFO("Combined Min: %.2f, Combined Max: %.2f",
		Min_Combined, Max_Combined);

	/*
	 * Start with satisfying the lower target voltage and then see
	 * if it does also satisfy the higher voltage.
	 */
	if (Min_Combined <= 1.2 && Max_Combined >= 1.2) {
		Voltage = 1.2;
	}

	if (Min_Combined <= 1.5 && Max_Combined >= 1.5) {
		Voltage = 1.5;
	}

	if (Access_Regulator(Regulator, &Voltage, 1) != 0) {
		SC_ERR("failed to set voltage of VADJ_FMC regulator");
		return -1;
	}

	return 0;
}

int
VCK190_IDT_8A34001_Reset(void)
{
	IO_Exp_t *IO_Exp;
	unsigned int Value;

	IO_Exp = Plat_Devs->IO_Exp;

	/*
	 * The '8A34001_EXP_RST_B' line is controlled by bit 5 of register
	 * offset 3.  The output register pair (offsets 2 & 3) are written
	 * at once.
	 */
	Value = 0x0;	// Assert reset - active low
	if (Access_IO_Exp(IO_Exp, 1, 0x2, &Value) != 0) {
		SC_ERR("failed to assert reset of 8A34001 chip");
		return -1;
	}

	sleep (1);

	Value = 0x20;	// De-assert reset
	if (Access_IO_Exp(IO_Exp, 1, 0x2, &Value) != 0) {
		SC_ERR("failed to de-assert reset of 8A34001 chip");
		return -1;
	}

	return 0;
}

/*
 * VCK190-specific Operations
*/
Plat_Ops_t VCK190_Ops = {
	.BootMode_Op = VCK190_BootMode_Op,
	.Reset_Op = VCK190_Reset_Op,
	.IDCODE_Op = VCK190_IDCODE_Op,
	.XSDB_Op = VCK190_XSDB_Op,
	.QSFP_ModuleSelect_Op = VCK190_QSFP_ModuleSelect_Op,
	.FMCAutoVadj_Op = VCK190_FMCAutoVadj_Op,
};
