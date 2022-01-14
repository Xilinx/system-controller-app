/*
 * Copyright (c) 2020 - 2021 Xilinx, Inc.  All rights reserved.
 *
 * SPDX-License-Identifier: MIT
 */

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <signal.h>
#include <string.h>
#include <fcntl.h>
#include <time.h>
#include <gpiod.h>
#include "sc_app.h"

#define GPIOLINE	"ZU4_TRIGGER"

extern Plat_Devs_t *Plat_Devs;
extern Plat_Ops_t *Plat_Ops;

int (*Workaround_Op)(void *);
extern char *Appfile(char *);
extern int Board_Identification(char *);
extern int Access_Regulator(Voltage_t *, float *, int);
extern int Set_IDT_8A34001(Clock_t *, char *, int);

/*
 * On VCK190/VMK180 boards, the GPIO line 11 is used to determine when to apply
 * the vccaux workaround for ES1 part.
 */ 
int
VCK190_GPIO(void)
{
	Workarounds_t *Workarounds;
	struct gpiod_chip *GPIO_Chip;
	struct gpiod_line *GPIO_Line;
	char GPIO_ChipName[STRLEN_MAX];
	int GPIO_State;
	unsigned int GPIO_Offset;
	struct timespec Timeout = { 1, 0 };
	struct gpiod_line_event GPIO_Event;
	
	/* Find the vccaux workaround function */
	Workarounds = Plat_Devs->Workarounds;
	for (int i = 0; i < Workarounds->Numbers; i++) {
		if (strcmp("vccaux", (char *)Workarounds->Workaround[i].Name) == 0) {
			Workaround_Op = Workarounds->Workaround[i].Plat_Workaround_Op;
			break;
		}
	}

	if (Workaround_Op == NULL) {
		SC_ERR("failed to locate workaround function!");
		return -1;
	}

	/* Find the GPIO chip name that handles GPIO line */
	if (gpiod_ctxless_find_line(GPIOLINE, GPIO_ChipName, STRLEN_MAX,
	    &GPIO_Offset) != 1) {
		SC_ERR("failed to find gpio line");
		return -1;
	}

	/* Open the GPIO line for monitoring */
	GPIO_Chip = gpiod_chip_open_by_name(GPIO_ChipName);
	if (GPIO_Chip == NULL) {
		SC_ERR("failed to open gpio chip");
		return -1;
	}

	GPIO_Line = gpiod_chip_get_line(GPIO_Chip, GPIO_Offset);
	if (GPIO_Line == NULL) {
		SC_ERR("failed to get gpio line");
		return -1;
	}

	if (gpiod_line_request_both_edges_events(GPIO_Line, "sc_appd") == -1) {
		SC_ERR("failed to request event notification");
		return -1;
	}

	/*
	 * If we are late to the party and Versal has already asserted
	 * GPIO line high and it is waiting for the workaround, apply it.
	 */
	GPIO_State = gpiod_line_get_value(GPIO_Line);
	if (GPIO_State == -1) {
		SC_ERR("failed to get current state of gpio line");
		return -1;
	}

	SC_INFO("GPIO line state at boot time: %d", GPIO_State);
	if (GPIO_State == 1) {
		(void)(*Workaround_Op)(&GPIO_State);
	}

	while (1) {
		/* Wait here until we receive an event */
		while (gpiod_line_event_wait(GPIO_Line, &Timeout) != 1);

		if (gpiod_line_event_read(GPIO_Line, &GPIO_Event) < 0) {
			SC_ERR("failed to read the last event notification");
			return -1;
		}

		GPIO_State = (GPIO_Event.event_type == 1) ? 1 : 0;
		SC_INFO("GPIO line state: %d", GPIO_State);

		if (GPIO_State == 1) {
			(void)(*Workaround_Op)(&GPIO_State);
		}
	}

	/* It should never get here! */
	return 0;
}

int
Vccaux_Workaround(void)
{
	FILE *FP;
	Voltages_t *Voltages;
	Voltage_t *Regulator;
	float Voltage;
	char Buffer[SYSCMD_MAX];
	char Value[STRLEN_MAX];
	int Workaround;
	int Found = 0;

	/* Remove previous 'silicon' file, if any */
	(void) remove(SILICONFILE);

	Voltages = Plat_Devs->Voltages;
	for (int i = 0; i < Voltages->Numbers; i++) {
		if (strcmp((char *)Voltages->Voltage[i].Name, "VCC_RAM") == 0) {
			Regulator = &Voltages->Voltage[i];
			break;
		}
	}

	if (Access_Regulator(Regulator, &Voltage, 0) != 0) {
		SC_ERR("failed to get VCC_RAM voltage");
		return -1;
	}

	SC_INFO("VCC_RAM is %.2f", Voltage);

	/*
	 * The VCC_RAM regulator is programmed to be off when the board powers
	 * on with ES1 part.  A board for production silicon powers on with
	 * VCC_RAM at 0.78V or higher.  Since on a production board, we may not
	 * read exactly 0.78V, compare the read value with 5% off of that value.
	 */
	if (Voltage < (0.78f - 0.04f)) {
		FP = fopen(SILICONFILE, "w");
		if (FP == NULL) {
			SC_ERR("failed to write to %s: %m", SILICONFILE);
			return -1;
		}

		(void) fputs("ES1\n", FP);
		(void) fclose(FP);
	} else {
		SC_INFO("Running on a board with production silicon. "
			"No workaround is required.");
		return 0;
	}

	/*
	 * XXX - The task of applying the vccaux workaround is currently
	 * performed by the Board Framework.  At this time, this task is
	 * not performed by sc_appd by default.  If there is no 'Vccaux_Workaround'
	 * variable defined in CONFIGFILE, add one and set it to 0.  If this
	 * variable already exists in CONFIGFILE and it is set to 1, it overrides
	 * the current default behavior.
	*/

	/* If there is no config file, create one and add 'Vccaux_Workaround: 0' */
	if (access(CONFIGFILE, F_OK) != 0) {
		(void) sprintf(Buffer, "echo \"Vccaux_Workaround: 0\" > %s; "
			       "sync", CONFIGFILE);
		SC_INFO("Command: %s", Buffer);
		system(Buffer);
		return 0;
	}

	FP = fopen(CONFIGFILE, "r");
	if (FP == NULL) {
		SC_ERR("failed to read file %s: %m", CONFIGFILE);
		return -1;
	}

	while (fgets(Buffer, SYSCMD_MAX, FP)) {
		if (strstr(Buffer, "Vccaux_Workaround:") != NULL) {
			SC_INFO("%s: %s", CONFIGFILE, Buffer);
			(void) strtok(Buffer, ":");
			if (strcmp(Buffer, "Vccaux_Workaround") != 0) {
				continue;
			}

			(void) strcpy(Value, strtok(NULL, "\n"));
			Workaround = atoi(Value);
			Found = 1;
			break;
		}
	}

	(void) fclose(FP);

	if (!Found) {
		(void) sprintf(Buffer, "echo \"Vccaux_Workaround: 0\" >> %s; "
			       "sync", CONFIGFILE);
		SC_INFO("Command: %s", Buffer);
		system(Buffer);
		return 0;
	}

	if (Workaround == 1) {
		return VCK190_GPIO();
	}

	return 0;
}

/*
 * Apply any applicable workaround
 */
int
Apply_Workarounds(void)
{
	FILE *FP;
	char Buffer[SYSCMD_MAX] = { 0 };

	if (access(BOARDFILE, F_OK) != 0) {
		SC_ERR("failed to access board file");
		return -1;
	}

	FP = fopen(BOARDFILE, "r");
	if (FP == NULL) {
		SC_ERR("failed to read file %s: %m", BOARDFILE);
		return -1;
	}

	(void) fgets(Buffer, SYSCMD_MAX, FP);
	(void) fclose(FP);

	/*
	 * Current workarounds:
	 *	vccaux: VCK190/VMK180 boards with ES1 part
	 */
	if ((strcmp(Buffer, "VCK190\n") == 0) || (strcmp(Buffer, "VMK180\n") == 0)) {
		if (Vccaux_Workaround() != 0) {
			SC_ERR("failed to determine the need for vccaux workaround");
			return -1;
		}
	}

	return 0;
}

/*
 * This routine sets any custom clock frequency defined by the user.
 */
int
Set_Clocks(void)
{
	FILE *FP;
	int FD;
	Clocks_t *Clocks;
	Clock_t *Clock;
	char Buffer[SYSCMD_MAX];
	char Value[SYSCMD_MAX];

	/* Remove '8A34001' file, if there is one */
	(void) remove(IDT8A34001FILE);

	/* If there is no clock file, there is nothing to do */
	if (access(CLOCKFILE, F_OK) != 0) {
		return 0;
	}

	FP = fopen(CLOCKFILE, "r");
	if (FP == NULL) {
		SC_ERR("failed to read clock file: %m");
		return -1;
	}

	Clocks = Plat_Devs->Clocks;
	while (fgets(Buffer, SYSCMD_MAX, FP)) {
		SC_INFO("%s: %s", CLOCKFILE, Buffer);
		(void) strtok(Buffer, ":");
		(void) strcpy(Value, strtok(NULL, "\n"));
		for (int i = 0; i < Clocks->Numbers; i++) {
			if (strcmp(Buffer, (char *)Clocks->Clock[i].Name) == 0) {
				Clock = &Clocks->Clock[i];
				break;
			}
		}

		if (Clock->Type == IDT_8A34001) {
			if (Set_IDT_8A34001(Clock, Value, 0) != 0) {
				return -1;
			}

			continue;
		}

		FD = open(Clock->Sysfs_Path, O_WRONLY);
		if (FD < 0) {
			SC_ERR("failed to open %s: %m", Clock->Sysfs_Path);
			(void) fclose(FP);
			return -1;
		}

		/* Remove any white spaces in Value string */
		(void) sprintf(Value, "%u\n",
		    (unsigned int)(strtod(Value, NULL) * 1000000));
		if (write(FD, Value, strlen(Value)) != strlen(Value)) {
			SC_ERR("failed to set clock frequency %s: %m", Value);
			(void) close(FD);
			(void) fclose(FP);
			return -1;
		}

		(void) close(FD);
	}

	(void) fclose(FP);
	return 0;
}

/*
 * This routine sets any custom regulator voltage defined by the user.
 */
int
Set_Voltages(void)
{
	FILE *FP;
	Voltages_t *Voltages;
	Voltage_t *Regulator;
	char Buffer[SYSCMD_MAX];
	char Value[STRLEN_MAX];
	float Voltage;

	/* If there is no voltage file, there is nothing to do */
	if (access(VOLTAGEFILE, F_OK) != 0) {
		return 0;
	}

	FP = fopen(VOLTAGEFILE, "r");
	if (FP == NULL) {
		SC_ERR("failed to read file %s: %m", VOLTAGEFILE);
		return -1;
	}

	Voltages = Plat_Devs->Voltages;
	while (fgets(Buffer, SYSCMD_MAX, FP)) {
		SC_INFO("%s: %s", VOLTAGEFILE, Buffer);
		(void) strtok(Buffer, ":");
		(void) strcpy(Value, strtok(NULL, "\n"));
		for (int i = 0; i < Voltages->Numbers; i++) {
			if (strcmp(Buffer, (char *)Voltages->Voltage[i].Name) == 0) {
				Regulator = &Voltages->Voltage[i];
				break;
			}
		}

		Voltage = strtof(Value, NULL);
		if (Access_Regulator(Regulator, &Voltage, 1) != 0) {
			SC_ERR("failed to set voltage on regulator");
			(void) fclose(FP);
			return -1;
		}
	}

	(void) fclose(FP);
	return 0;
}

int
FMC_Autodetect_Vadj()
{
	FILE *FP;
	char Buffer[SYSCMD_MAX];
	char Value[STRLEN_MAX];
	int Autodetect;
	int Found = 0;

	/*
	 * XXX - The FMC Autodetect Vadj task is currently performed by
	 * the Board Framework.  At this time, this feature is not performed
	 * by sc_appd by default.  If there is no 'FMC_Autodetect' variable
	 * defined in CONFIGFILE, add one and set it to 0.  If this variable
	 * already exists in CONFIGFILE and it is set to 1, it overrides
	 * the current default behavior.
	 */

	/* If there is no config file, create one and add 'FMC_Autodetect: 0' */
	if (access(CONFIGFILE, F_OK) != 0) {
		(void) sprintf(Buffer, "echo \"FMC_Autodetect: 0\" > %s; "
			       "sync", CONFIGFILE);
		SC_INFO("Command: %s", Buffer);
		system(Buffer);
		return 0;
	}

	FP = fopen(CONFIGFILE, "r");
	if (FP == NULL) {
		SC_ERR("failed to read file %s: %m", CONFIGFILE);
		return -1;
	}

	while (fgets(Buffer, SYSCMD_MAX, FP)) {
		if (strstr(Buffer, "FMC_Autodetect:") != NULL) {
			SC_INFO("%s: %s", CONFIGFILE, Buffer);
			(void) strtok(Buffer, ":");
			if (strcmp(Buffer, "FMC_Autodetect") != 0) {
				continue;
			}

			(void) strcpy(Value, strtok(NULL, "\n"));
			Autodetect = atoi(Value);
			Found = 1;
			break;
		}
	}

	(void) fclose(FP);

	if (!Found) {
		(void) sprintf(Buffer, "echo \"FMC_Autodetect: 0\" >> %s; "
			       "sync", CONFIGFILE);
		SC_INFO("Command: %s", Buffer);
		system(Buffer);
		return 0;
	}

	if (Autodetect == 1) {
		if (Plat_Ops->FMCAutoVadj_Op == NULL) {
			SC_ERR("FMC autodetect vadj operation is not supported");
			return -1;
		}

		if (Plat_Ops->FMCAutoVadj_Op() != 0) {
			SC_ERR("failed to autodetect FMC vadj");
			return -1;
		}
	}

	return 0;
}

int
main()
{
	char Board_Name[STRLEN_MAX];
	int Ret = -1;

	SC_OPENLOG("sc_appd");
	SC_INFO(">>> Begin");

	/* Identify the board */
	if (Board_Identification(Board_Name) != 0) {
		goto Out;
	}

	/* Detect FMC modules and auto adjust voltage */
	if (FMC_Autodetect_Vadj() != 0) {
		SC_ERR("failed to FMC autodetect vadj");
		goto Out;
	}

	/* Set custom clock frequency */
	if (Set_Clocks() != 0) {
		SC_ERR("failed to set clock frequency");
		goto Out;
	}

	/* Set custom regulator voltage */
	if (Set_Voltages() != 0) {
		SC_ERR("failed to set regulator voltage");
		goto Out;
	}

	/* No pre-set boot mode is supported */
	(void) remove(BOOTMODEFILE);

	/* Applying any workaround */
	if (Apply_Workarounds() != 0) {
		SC_ERR("failed to apply workarounds");
		goto Out;
	}

	Ret = 0;
Out:
	SC_INFO("<<< End(%d)", Ret);
	return Ret;
}
