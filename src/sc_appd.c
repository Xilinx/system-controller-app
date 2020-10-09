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

#define GPIOCHIP	"gpiochip0"
#define GPIOLINE	11

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
extern Workarounds_t Workarounds;
int (*Workaround_Op)(void *);
extern int Plat_Board_Name(char *);
extern int Plat_Reset_Ops(void);

#define SILICON_ES1 \
{ \
	if (access(SILICONFILE, F_OK) == -1) { \
		FILE *FP; \
		FP = fopen(SILICONFILE, "w"); \
		if (FP == NULL) { \
			printf("ERROR: failed to write silicon file\n"); \
		} else { \
			(void) fputs("ES1\n", FP); \
		} \
		fclose(FP); \
	} \
}

#define IS_SILICON_ES1(Yes) \
{ \
	(Yes) = 0; \
	if (access(SILICONFILE, F_OK) == 0) { \
		FILE *FP; \
		char Buffer[STRLEN_MAX]; \
		FP = fopen(SILICONFILE, "r"); \
		if (FP == NULL) { \
			printf("ERROR: failed to read silicon file\n"); \
		} else { \
			(void) fgets(Buffer, sizeof (Buffer), FP); \
			fclose(FP); \
			if (strcmp("ES1\n", Buffer) == 0) { \
				(Yes) = 1; \
			} \
		} \
	} \
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
			printf("ERROR: failed to set the timer\n");
		}

		SILICON_ES1;
		(void)(*Workaround_Op)(&GPIO_State);
		return;
	}

	if (GPIO_Behavior != Watchdog) {
		printf("ASSERT: invalid GPIO behavior!\n");
	}

	/* Cancel the timer */
	if (timer_settime(Timer_Id, 0, &Timer_Off, NULL) == -1) {
		printf("ERROR: failed to set the timer\n");
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
		printf("ERROR: failed to create logfile\n");
		return -1;
	}
#endif /* DEBUG */

	/* Wait here until we receive an event */
	while (gpiod_line_event_wait(GPIO_Line, &Timeout) != 1);

	State = gpiod_line_event_read(GPIO_Line, &GPIO_Event);
	if (State == -1) {
		printf("ERROR: failed to read the last event notification\n");
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
	int ES1;
	char Buffer[SYSCMD_MAX];
	char Config_Token[STRLEN_MAX];
	struct sigaction Sig_Action;
	struct sigevent Sig_Event;
	sigset_t Sig_Mask;
	struct gpiod_chip *GPIO_Chip;
	struct gpiod_line *GPIO_Line;
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
		printf("ERROR: failed to locate workaround function!\n");
		return -1;
	}

	/* Check for any custom configurable variables */
	if (access(CONFIGFILE, F_OK) == 0) {
		FP = fopen(CONFIGFILE, "r");
		if (FP == NULL) {
			printf("ERROR: failed to read config file\n");
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
		printf("ERROR: failed to set sigaction");
		return -1;
	}

	/* Create the real timer */
	Sig_Event.sigev_notify = SIGEV_SIGNAL;
	Sig_Event.sigev_signo = SIGRTMIN;
	Sig_Event.sigev_value.sival_ptr = &Timer_Id;
	if (timer_create(CLOCK_REALTIME, &Sig_Event, &Timer_Id) == -1) {
		printf("ERROR: failed to create timer");
		return -1;
	}

	Timer_Start.it_value.tv_sec = (WDT_Timeout / 1000);
	Timer_Start.it_value.tv_nsec =
	    ((WDT_Timeout - (Timer_Start.it_value.tv_sec * 1000)) * 1000000);
	Timer_Start.it_interval.tv_sec = Timer_Start.it_value.tv_sec;
	Timer_Start.it_interval.tv_nsec = Timer_Start.it_value.tv_nsec;

	/* Open the GPIO line for monitoring */
	GPIO_Chip = gpiod_chip_open_by_name(GPIOCHIP);
	if (GPIO_Chip == NULL) {
		printf("ERROR: failed to open gpio chip\n");
		return -1;
	}

	GPIO_Line = gpiod_chip_get_line(GPIO_Chip, GPIOLINE);
	if (GPIO_Line == NULL) {
		printf("ERROR: failed to get gpio line\n");
		return -1;
	}

	if (gpiod_line_request_both_edges_events(GPIO_Line, "sc_appd") == -1) {
		printf("ERROR: failed to request event notification\n");
		return -1;
	}

	/*
	 * If we are late for the party and Versal has already asserted
	 * GPIO line high and it is waiting for the workaround, apply it.
	 */
	IS_SILICON_ES1(ES1);
	if (ES1 == 1) {
		GPIO_State = gpiod_line_get_value(GPIO_Line);
		if (GPIO_State == -1) {
			printf("ERROR: failed to get current state of gpio line\n");
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
			printf("ERROR: failed to monitor gpio line\n");
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
				printf("ERROR: failed to set the timer\n");
			}

			break;

		case Watchdog:
			/* Reset the timer */
			if ((WDT_Edge == 2) ||
			    (WDT_Edge == 1 && GPIO_State == 1) ||
			    (WDT_Edge == 0 && GPIO_State == 0)) {
				if (timer_settime(Timer_Id, 0, &Timer_Start, NULL) == -1) {
					printf("ERROR: failed to set the timer\n");
				}
			}

			break;

		case Workaround:
			SILICON_ES1;
			(void)(*Workaround_Op)(&GPIO_State);
			break;

		default:
			printf("ERROR: invalid GPIO behavior!\n");
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

	FP = popen(Command, "r");
	if (FP == NULL) {
		printf("ERROR: failed to execute sc_app command\n");
		return -1;
	}

	(void) fgets(Output, sizeof(Output), FP);
	pclose(FP);
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
		SILICON_ES1;
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
	char Value[STRLEN_MAX];

	/* If there is no clock file, there is nothing to do */
	if (access(CLOCKFILE, F_OK) != 0) {
		return 0;
	}

	FP = fopen(CLOCKFILE, "r");
	if (FP == NULL) {
		printf("ERROR: failed to read clock file\n");
		return -1;
	}

	while (fgets(Buffer, SYSCMD_MAX, FP)) {
		(void) strtok(Buffer, ":");
		(void) strcpy(Value, strtok(NULL, "\n"));
		for (int i = 0; i < Clocks.Numbers; i++) {
			if (strcmp(Buffer, (char *)Clocks.Clock[i].Name) == 0) {
				Clock = &Clocks.Clock[i];
				break;
			}
		}

		FD = open(Clock->Sysfs_Path, O_WRONLY);
		if (FD < 0) {
			printf("ERROR: failed to open clock device\n");
			(void) fclose(FP);
			return -1;
		}

		/* Remove any white spaces in Value string */
		sprintf(Value, "%d\n", atoi(Value));
		if (write(FD, Value, strlen(Value)) != strlen(Value)) {
			printf("ERROR: failed to set clock frequency\n");
			(void) close(FD);
			(void) fclose(FP);
			return -1;
		}

		(void) close(FD);
	}

	(void) fclose(FP);
	return 0;
}

int
main()
{
	char Board[STRLEN_MAX];

	/* If '.sc_app' directory doesn't exist, create it */
	if (access(APPDIR, F_OK) == -1) {
		if (mkdir(APPDIR, 0755) == -1) {
			printf("ERROR: failed to create \'.sc_app\' directory\n");
			return -1;
		}
	}

	/* Set custom clock frequency */
	if (Set_Clocks() != 0) {
		printf("ERROR: failed to set clock frequency\n");
		return -1;
	}

	if (Plat_Board_Name(Board) == -1) {
		printf("ERROR: failed to get board name\n");
		return -1;
	}

	if (strcmp(Board, "vck190") == 0) {
		if (VCK190_Version() == -1) {
			printf("ERROR: failed to determine silicon version\n");
			return -1;
		}
		(void) VCK190_GPIO();
	}

	return -1;
}