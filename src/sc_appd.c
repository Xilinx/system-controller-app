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
#include <sys/stat.h>
#include "sc_app.h"

#define CONFIGFILE	APPDIR"/config"
#ifdef DEBUG
#define LOGFILE		APPDIR"/gpio.log"
#endif /* DEBUG */

#define GPIOLINE	"ZU4_TRIGGER"

/*
 * Default configurable variables.  They can be modified in 'config' file.
 */
int WDT_Timeout = 200;		// Milliseconds
int WDT_Edge = 1;		// 0 - Falling Edge
				// 1 - Rising Edge	
				// 2 - Either Edge
int Training_Interval = 1000;	// Milliseconds

struct itimerspec Timer_Start;
struct itimerspec Timer_Off = { 0 };
timer_t Timer_Id;

typedef enum {
	Unknown,
	Workaround,
	Watchdog,
} GPIO_Behavior_t;

GPIO_Behavior_t GPIO_Behavior = Unknown;
time_t GPIO_First_Time = 0;
int GPIO_State;

extern Clocks_t Clocks;
extern Voltages_t Voltages;
extern Workarounds_t Workarounds;
int (*Workaround_Op)(void *);
extern int Plat_Board_Name(char *);
extern int Plat_Reset_Ops(void);
extern int Plat_FMCAutoAdjust(void);
extern int Access_Regulator(Voltage_t *, float *, int);
extern int Set_IDT_8A34001(Clock_t *, char *, int);

static int
Is_Silicon_ES1(void)
{
	FILE *FP;
	char Buffer[STRLEN_MAX];

	if (access(SILICONFILE, F_OK) == 0) {
		FP = fopen(SILICONFILE, "r");
		if (FP == NULL) {
			SC_ERR("failed to open file %s: %m", SILICONFILE);
			return -1;
		}

		(void) fgets(Buffer, sizeof (Buffer), FP);
		(void) fclose(FP);
		if (strcmp("ES1\n", Buffer) == 0) {
			return 1;
		}
	}

	return 0;
}

/*
 * The real timer signal handler.
 */
static void
Timer_Handler(int Signal, siginfo_t *Signal_Info, void *Arg)
{
	time_t Now;
	double MSeconds;

	if (GPIO_Behavior == Unknown) {
		/*
		 * Regardless of WDT_Timeout, wait until Training_Interval
		 * time passes before setting GPIO behavior to Workaround.
		 */
		MSeconds = (difftime(time(&Now), GPIO_First_Time) * 1000);
		if (MSeconds < Training_Interval) {
			return;
		}

		GPIO_Behavior = Workaround;

		/* Cancel the timer */
		if (timer_settime(Timer_Id, 0, &Timer_Off, NULL) == -1) {
			SC_ERR("failed to set the timer: %m");
		}

		if (Is_Silicon_ES1() == 1) {
			(void)(*Workaround_Op)(&GPIO_State);
		}

		return;
	}

	if (GPIO_Behavior != Watchdog) {
		SC_ERR("invalid GPIO behavior");
	}

	/* Cancel the timer */
	if (timer_settime(Timer_Id, 0, &Timer_Off, NULL) == -1) {
		SC_ERR("failed to set the timer: %m");
	}

	(void) (*Plat_Reset_Ops)();
}

int
Monitor_GPIO(struct gpiod_line *GPIO_Line)
{
	struct timespec Timeout = { 1, 0 };
	struct gpiod_line_event GPIO_Event;
	int State;

#ifdef DEBUG
	FILE *FP = NULL;

	FP = fopen(LOGFILE, "a");
	if (FP == NULL) {
		SC_ERR("failed to create %s: %m", LOGFILE);
		return -1;
	}
#endif /* DEBUG */

	/* Wait here until we receive an event */
	while (gpiod_line_event_wait(GPIO_Line, &Timeout) != 1);

	State = gpiod_line_event_read(GPIO_Line, &GPIO_Event);
	if (State == -1) {
		SC_ERR("failed to read the last event notification");
		return -1;
	}

	State = (GPIO_Event.event_type == 1) ? 1 : 0;

#ifdef DEBUG
	fprintf(FP, "%d:\t%lld(s).%.3ld(ms)\n", State,
	    (long long)GPIO_Event.ts.tv_sec, (GPIO_Event.ts.tv_nsec/1000000));
	fclose(FP);
#endif /* DEBUG */

	return State;
}

/*
 * On VCK190, GPIO line 11 is used to apply vccaux workaround for ES1 Versal,
 * and to use as watchdog timer kicker for Production version of Versal.  Which
 * version of Versal used in VCK190 is determined by how this GPIO line behaves.
 */ 
int
VCK190_GPIO(void)
{
	FILE *FP;
	char Buffer[SYSCMD_MAX];
	char Config_Token[STRLEN_MAX];
	struct sigaction Sig_Action;
	struct sigevent Sig_Event;
	sigset_t Sig_Mask;
	struct gpiod_chip *GPIO_Chip;
	struct gpiod_line *GPIO_Line;
	char GPIO_ChipName[STRLEN_MAX];
	unsigned int GPIO_Offset;
	time_t Now;
	double MSeconds;
	
	/* Find the vccaux workaround function */
	for (int i = 0; i < Workarounds.Numbers; i++) {
		if (strcmp("vccaux", (char *)Workarounds.Workaround[i].Name) == 0) {
			Workaround_Op = Workarounds.Workaround[i].Plat_Workaround_Op;
			break;
		}
	}

	if (Workaround_Op == NULL) {
		SC_ERR("failed to locate workaround function!");
		return -1;
	}

	/* Check for any custom configurable variables */
	if (access(CONFIGFILE, F_OK) == 0) {
		FP = fopen(CONFIGFILE, "r");
		if (FP == NULL) {
			SC_ERR("failed to read file %s: %m", CONFIGFILE);
			return -1;
		}

		while (fgets(Buffer, SYSCMD_MAX, FP)) {
			if (strstr(Buffer, "WDT_Timeout:") != NULL) {
				(void) strtok(Buffer, ":");
				if (strcmp(Buffer, "WDT_Timeout") != 0) {
					continue;
				}

				(void) strcpy(Config_Token, strtok(NULL, "\n"));
				WDT_Timeout = atoi(Config_Token);
			}

			if (strstr(Buffer, "WDT_Edge:") != NULL) {
				(void) strtok(Buffer, ":");
				if (strcmp(Buffer, "WDT_Edge") != 0) {
					continue;
				}

				(void) strcpy(Config_Token, strtok(NULL, "\n"));
				WDT_Edge = atoi(Config_Token);
			}

			if (strstr(Buffer, "Training_Interval:") != NULL) {
				(void) strtok(Buffer, ":");
				if (strcmp(Buffer, "Training_Interval") != 0) {
					continue;
				}

				(void) strcpy(Config_Token, strtok(NULL, "\n"));
				Training_Interval = atoi(Config_Token);
			}
		}

		fclose(FP);
	}

	/* Initialize the real timer signal */
	Sig_Action.sa_flags = SA_SIGINFO;
	Sig_Action.sa_sigaction = Timer_Handler;
	sigemptyset(&Sig_Action.sa_mask);
	if (sigaction(SIGRTMIN, &Sig_Action, NULL) == -1) {
		SC_ERR("failed to set sigaction: %m");
		return -1;
	}

	/* Create the real timer */
	Sig_Event.sigev_notify = SIGEV_SIGNAL;
	Sig_Event.sigev_signo = SIGRTMIN;
	Sig_Event.sigev_value.sival_ptr = &Timer_Id;
	if (timer_create(CLOCK_REALTIME, &Sig_Event, &Timer_Id) == -1) {
		SC_ERR("failed to create timer: %m");
		return -1;
	}

	Timer_Start.it_value.tv_sec = (WDT_Timeout / 1000);
	Timer_Start.it_value.tv_nsec =
	    ((WDT_Timeout - (Timer_Start.it_value.tv_sec * 1000)) * 1000000);
	Timer_Start.it_interval.tv_sec = Timer_Start.it_value.tv_sec;
	Timer_Start.it_interval.tv_nsec = Timer_Start.it_value.tv_nsec;

	/* Find the GPIO chip name that handles WDT line */
	if (gpiod_ctxless_find_line(GPIOLINE, GPIO_ChipName, STRLEN_MAX,
	    &GPIO_Offset) != 1) {
		SC_ERR("failed to find GPIO line");
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
	 * If we are late for the party and Versal has already asserted
	 * GPIO line high and it is waiting for the workaround, apply it.
	 */
	if (Is_Silicon_ES1() == 1) {
		GPIO_State = gpiod_line_get_value(GPIO_Line);
		if (GPIO_State == -1) {
			SC_ERR("failed to get current state of gpio line");
			return -1;
		}

		if (GPIO_State == 1) {
			(void)(*Workaround_Op)(&GPIO_State);
		}
	}

	while (1) {
		/* GPIO line 11 is connected to PMC MIO37 */
		GPIO_State = Monitor_GPIO(GPIO_Line);
		if (GPIO_State == -1) {
			SC_ERR("failed to monitor gpio line");
			return -1;
		}
#ifdef DEBUG
		printf("%d", GPIO_State); fflush(NULL);
#endif /* DEBUG */

		switch (GPIO_Behavior) {
		case Unknown:
			/* Training starts with detection of first rising edge */
			if (GPIO_State == 0 && GPIO_First_Time == 0) {
				break;
			}

			if (GPIO_First_Time == 0) {
				time(&GPIO_First_Time);
			} else {
				/*
				 * If transitions arrive earlier than WDT_Timeout,
				 * the GPIO behavior is Watchdog.
				 */
				MSeconds = (difftime(time(&Now), GPIO_First_Time) * 1000);
				if (MSeconds < Training_Interval) {
					GPIO_Behavior = Watchdog;
				}
			}

			/* Workaround behavior is determined in timer handler */
			if (timer_settime(Timer_Id, 0, &Timer_Start, NULL) == -1) {
				SC_ERR("failed to set the timer: %m");
			}

			break;

		case Watchdog:
			/* Reset the timer */
			if ((WDT_Edge == 2) ||
			    (WDT_Edge == 1 && GPIO_State == 1) ||
			    (WDT_Edge == 0 && GPIO_State == 0)) {
				if (timer_settime(Timer_Id, 0, &Timer_Start, NULL) == -1) {
					SC_ERR("failed to set the timer: %m");
				}
			}

			break;

		case Workaround:
			if (Is_Silicon_ES1() == 1) {
				(void)(*Workaround_Op)(&GPIO_State);
			}
			break;

		default:
			SC_ERR("invalid GPIO behavior!");
		}
	}

	/* It should never get here! */
	return 0;
}

int
VCK190_Version(void)
{
	FILE *FP;
	float Voltage;
	char Output[STRLEN_MAX];
	char Command[] = "/usr/bin/sc_app -c getvoltage -t VCC_RAM";

	/* Remove previous 'silicon' file, if any */
	(void) remove(SILICONFILE);

	SC_INFO("Command: %s", Command);
	FP = popen(Command, "r");
	if (FP == NULL) {
		SC_ERR("failed to run %s: %m", Command);
		return -1;
	}

	(void) fgets(Output, sizeof(Output), FP);
	(void) pclose(FP);
	SC_INFO("Output: %s", Output);
	(void) strtok(Output, ":");
	(void) strcpy(Output, strtok(NULL, "\n"));
	Voltage = atof(Output);

	/*
	 * The VCC_RAM regulator is programmed to be off at board power on
	 * for ES1 part.  Boards for production silicon will power on with
	 * VCC_RAM at 0.78V in low range.  Since on production boards, we
	 * may not read exactly 0.78V, compare the read value with 5% off
	 * margin to be safe.
	 */
	if (Voltage < (0.78f - 0.04f)) {
		FP = fopen(SILICONFILE, "w");
		if (FP == NULL) {
			SC_ERR("failed to write to %s: %m", SILICONFILE);
			return -1;
		}

		(void) fputs("ES1\n", FP);
		(void) fclose(FP);
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

	while (fgets(Buffer, SYSCMD_MAX, FP)) {
		SC_INFO("%s: %s", CLOCKFILE, Buffer);
		(void) strtok(Buffer, ":");
		(void) strcpy(Value, strtok(NULL, "\n"));
		for (int i = 0; i < Clocks.Numbers; i++) {
			if (strcmp(Buffer, (char *)Clocks.Clock[i].Name) == 0) {
				Clock = &Clocks.Clock[i];
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

	while (fgets(Buffer, SYSCMD_MAX, FP)) {
		SC_INFO("%s: %s", VOLTAGEFILE, Buffer);
		(void) strtok(Buffer, ":");
		(void) strcpy(Value, strtok(NULL, "\n"));
		for (int i = 0; i < Voltages.Numbers; i++) {
			if (strcmp(Buffer, (char *)Voltages.Voltage[i].Name) == 0) {
				Regulator = &Voltages.Voltage[i];
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
main()
{
	char Board[STRLEN_MAX];
	SC_OPENLOG("sc_appd");
	SC_INFO(">>> Begin");

	/* If '.sc_app' directory doesn't exist, create it */
	if (access(APPDIR, F_OK) == -1) {
		if (mkdir(APPDIR, 0755) == -1) {
			SC_ERR("mkdir %s failed: %m", APPDIR);
			return -1;
		}
	}

	/* Detect FMC cards and auto adjust voltage */
	if (Plat_FMCAutoAdjust() != 0) {
		SC_ERR("failed to auto adjust FMC cards");
		return -1;
	}

	/* Set custom clock frequency */
	if (Set_Clocks() != 0) {
		SC_ERR("failed to set clock frequency");
		return -1;
	}

	/* Set custom regulator voltage */
	if (Set_Voltages() != 0) {
		SC_ERR("failed to set regulator voltage");
		return -1;
	}

	/* No pre-set boot mode is supported */
	(void) remove(BOOTMODEFILE);

	if (Plat_Board_Name(Board) == -1) {
		SC_ERR("failed to get board name");
		return -1;
	}

	if (strcmp(Board, "vck190") == 0) {
		if (VCK190_Version() == -1) {
			SC_ERR("failed to determine silicon version");
			return -1;
		}
		(void) VCK190_GPIO();
	}

	SC_INFO("<<< End(-1)");
	return -1;
}
