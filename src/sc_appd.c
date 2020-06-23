#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "sc_app.h"

extern Workarounds_t Workarounds;

int
Monitor_GPIO(int Line)
{
	FILE *FP;
	int State;
	char Command[SYSCMD_MAX];
	char Output[STRLEN_MAX];
	char Cmd_Str[] = "gpiomon --format=\"%e\" --num-events=1 gpiochip0";

	(void) sprintf(Command, "%s %d", Cmd_Str, Line);

	FP = popen(Command, "r");
	if (FP == NULL) {
		printf("ERROR: failed to execute gpiomon command\n");
		return -1;
	}

	(void) fgets(Output, sizeof(Output), FP);
	pclose(FP);
	State = atoi(Output);
	if (State == 0 || State == 1) {
		return State;
	} else {
		return -1;
	}
}

int
Monitor_Workaround(void *Op)
{
	int State;
	void (*Workaround_Op)(void *) = Op;

	while (1) {
		/* GPIO line 11 is connected to PMC MIO37 */
		State = Monitor_GPIO(11);
		if (State == -1) {
			printf("ERROR: failed to monitor gpio 11\n");
			return -1;
		}
		(void)(*Workaround_Op)(&State);
	}

	/* Never gets here */
	return 0;
}

int
main()
{
	int i;

	/* Currently we support only VCK190 with ES1 silicon */
	for (int i = 0; i < Workarounds.Numbers; i++) {
		if (strcmp("vccaux", (char *)Workarounds.Workaround[i].Name) == 0) {
			(void) Monitor_Workaround(Workarounds.Workaround[i].Plat_Workaround_Op);
			printf("ERROR: failed to monitor for vccaux workaround!\n");
			return -1;
		}
	}

	printf("ERROR: not a VCK190 board!\n");
	return -1;
}
