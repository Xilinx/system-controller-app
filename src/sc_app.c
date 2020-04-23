#include <stdio.h>
#include <stdlib.h>
#include <fcntl.h>
#include <unistd.h>
#include <string.h>
#include "sc_app.h"


extern I2C_Buses_t I2C_Buses;
extern BootModes_t BootModes;
extern Clocks_t Clocks;
extern Ina226s_t Ina226s;
extern Voltages_t Voltages;
extern Workarounds_t Workarounds;
extern BITs_t BITs;

int Parse_Options(int argc, char **argv);
int BootMode_Ops(void);
int Clock_Ops(void);
int Power_Ops(void);
int Workaround_Ops(void);
int BIT_Ops(void);
extern int Plat_Reset_Ops(void);
extern int Plat_EEPROM_Ops(void);

static char Usage[] = "\n\
sc_app -c <command> [-t <target> [-v <value>]]\n\n\
<command>:\n\
	listbootmode - lists the supported boot mode targets\n\
	setbootmode - set boot mode to <target>\n\
	reset - apply power-on-reset\n\
	eeprom - lists the content of EEPROM\n\
	listclock - lists the supported clock targets\n\
	getclock - get the frequency of <target>\n\
	setclock - set <target> to <value> frequency\n\
	restoreclock - restore <target> to default value\n\
	listpower - lists the supported power targets\n\
	getpower - get the voltage, current, and power of <target>\n\
	listworkaround - lists the applicable workaround targets\n\
	workaround - apply <target> workaround (may requires <value>)\n\
	listBIT - lists the supported Board Interface Test targets\n\
	BIT - run BIT target\n\
";

typedef enum {
	LISTBOOTMODE,
	SETBOOTMODE,
	RESET,
	EEPROM,
	LISTCLOCK,
	GETCLOCK,
	SETCLOCK,
	RESTORECLOCK,
	LISTPOWER,
	GETPOWER,
	LISTWORKAROUND,
	WORKAROUND,
	LISTBIT,
	BIT,
	COMMAND_MAX,
} CmdId_t;

typedef struct {
	CmdId_t	CmdId;	
	char CmdStr[STRLEN_MAX];
	int (*CmdOps)(void);
} Command_t;

static Command_t Commands[] = {
	{ .CmdId = LISTBOOTMODE, .CmdStr = "listbootmode", .CmdOps = BootMode_Ops, },
	{ .CmdId = SETBOOTMODE, .CmdStr = "setbootmode", .CmdOps = BootMode_Ops, },
	{ .CmdId = RESET, .CmdStr = "reset", .CmdOps = Plat_Reset_Ops, },
	{ .CmdId = EEPROM, .CmdStr = "eeprom", .CmdOps = Plat_EEPROM_Ops, },
	{ .CmdId = LISTCLOCK, .CmdStr = "listclock", .CmdOps = Clock_Ops, },
	{ .CmdId = GETCLOCK, .CmdStr = "getclock", .CmdOps = Clock_Ops, },
	{ .CmdId = SETCLOCK, .CmdStr = "setclock", .CmdOps = Clock_Ops, },
	{ .CmdId = RESTORECLOCK, .CmdStr = "restoreclock", .CmdOps = Clock_Ops, },
	{ .CmdId = LISTPOWER, .CmdStr = "listpower", .CmdOps = Power_Ops, },
	{ .CmdId = GETPOWER, .CmdStr = "getpower", .CmdOps = Power_Ops, },
	{ .CmdId = LISTWORKAROUND, .CmdStr = "listworkaround", .CmdOps = Workaround_Ops, },
	{ .CmdId = WORKAROUND, .CmdStr = "workaround", .CmdOps = Workaround_Ops, },
	{ .CmdId = LISTBIT, .CmdStr = "listBIT", .CmdOps = BIT_Ops, },
	{ .CmdId = BIT, .CmdStr = "BIT", .CmdOps = BIT_Ops, },
};

char Command_Arg[STRLEN_MAX];
char Target_Arg[STRLEN_MAX];
char Value_Arg[STRLEN_MAX];
int C_Flag, T_Flag, V_Flag;
static Command_t Command;

/*
 * Main routine
 */
int
main(int argc, char **argv)
{
	int Ret;

	Ret = Parse_Options(argc, argv);
	if (Ret != 0) {
		return Ret;
	}

	Ret = (*Command.CmdOps)();
	return Ret;
}

/*
 * Parse the command line.
 */
int
Parse_Options(int argc, char **argv)
{
	int Options = 0;
	int c;

	opterr = 0;
	while ((c = getopt(argc, argv, "hc:t:v:")) != -1) {
		Options++;
		switch (c) {
		case 'h':
			printf("%s\n", Usage);
			return 1;
			break;
		case 'c':
			C_Flag = 1;
			strcpy(Command_Arg, optarg);
			break;
		case 't':
			T_Flag = 1;
			strcpy(Target_Arg, optarg);
			break;
		case 'v':
			V_Flag = 1;
			strcpy(Value_Arg, optarg);
			break;
		case '?':
			printf("ERROR: invalid argument\n");
			printf("%s\n", Usage);
			return -1;
		default:
			printf("%s\n", Usage);
			return -1;
		}
	}

	if (Options == 0) {
		printf("%s\n", Usage);
		return -1;
	}

	for (int i = 0; i < COMMAND_MAX; i++) {
		if (strcmp(Command_Arg, (char *)Commands[i].CmdStr) == 0) {
			Command = Commands[i];
			return 0;
		}
	}

	return -1;
}

/*
 * Boot Mode Operations
 */
int
BootMode_Ops(void)
{
	int Target_Index = -1;
	char System_Cmd[SYSCMD_MAX];

	if (Command.CmdId == LISTBOOTMODE) {
		for (int i = 0; i < BootModes.Numbers; i++) {
			printf("%s\t0x%x\n", BootModes.BootMode[i].Name,
			    BootModes.BootMode[i].Value);
		}
		return 0;
	}

	/* Validate the bootmode target */
	if (T_Flag == 0) {
		printf("ERROR: no bootmode target\n");
		return -1;
	}

	for (int i = 0; i < BootModes.Numbers; i++) {
		if (strcmp(Target_Arg, (char *)BootModes.BootMode[i].Name) == 0) {
			Target_Index = i;
			break;
		}
	}

	if (Target_Index == -1) {
		printf("ERROR: invalid bootmode target\n");
		return -1;
	}

	switch (Command.CmdId) {
	case SETBOOTMODE:
		/* Boot Mode */
		for (int i = 0; i < 4; i++) {
			sprintf(System_Cmd, "gpioset %s=%d",
			    (char *)&BootModes.Mode_Lines[i],
			    ((BootModes.BootMode[Target_Index].Value >> i) & 0x1));
			system(System_Cmd);
		}
		break;
	default:
		printf("ERROR: invalid bootmode command\n");
		break;
	}

	return 0;
}

/*
 * Clock Operations
 */
int
Clock_Ops(void)
{
	int Target_Index = -1;
	char System_Cmd[SYSCMD_MAX];
	int Upper, Lower;
	unsigned long int Value;

	if (Command.CmdId == LISTCLOCK) {
		for (int i = 0; i < Clocks.Numbers; i++) {
			printf("%s\n", Clocks.Clock[i].Name);
		}
		return 0;
	}

	/* Validate the clock target */
	if (T_Flag == 0) {
		printf("ERROR: no clock target\n");
		return -1;
	}

	for (int i = 0; i < Clocks.Numbers; i++) {
		if (strcmp(Target_Arg, (char *)Clocks.Clock[i].Name) == 0) {
			Target_Index = i;
			break;
		}
	}

	if (Target_Index == -1) {
		printf("ERROR: invalid clock target\n");
		return -1;
	}

	switch (Command.CmdId) {
	case GETCLOCK:
		sprintf(System_Cmd, "echo -n \'Frequency(Hz):\t\'; cat %s",
		    Clocks.Clock[Target_Index].Sysfs_Path);
		system(System_Cmd);
		break;
	case SETCLOCK:
		/* Validate the frequency */
		if (V_Flag == 0) {
			printf("ERROR: no clock frequency\n");
			return -1;
		}

		Value = atol(Value_Arg);
		Upper = Clocks.Clock[Target_Index].Upper_Freq;
		Lower = Clocks.Clock[Target_Index].Lower_Freq;
		if (Value > Upper || Value < Lower) {
			printf("ERROR: valid frequency range is %d-%d\n",
			    Lower, Upper);
			return -1;
		}
		sprintf(System_Cmd, "echo %d > %s", (int)Value,
		    Clocks.Clock[Target_Index].Sysfs_Path);
		system(System_Cmd);
		break;
	case RESTORECLOCK:
		Value = Clocks.Clock[Target_Index].Default_Freq;
		sprintf(System_Cmd, "echo %d > %s", (int)Value,
		    Clocks.Clock[Target_Index].Sysfs_Path);
		system(System_Cmd);
		break;
	default:
		printf("ERROR: invalid clock command\n");
		break;
	}

	return 0;
}

/*
 * Power Operations
 */
int Power_Ops(void)
{
	int Target_Index = -1;
	char System_Cmd[SYSCMD_MAX];

	if (Command.CmdId == LISTPOWER) {
		for (int i = 0; i < Ina226s.Numbers; i++) {
			printf("%s\n", Ina226s.Ina226[i].Name);
		}
		return 0;
	}

	/* Validate the power target */
	if (T_Flag == 0) {
		printf("ERROR: no power target\n");
		return -1;
	}

	for (int i = 0; i < Ina226s.Numbers; i++) {
		if (strcmp(Target_Arg, (char *)Ina226s.Ina226[i].Name) == 0) {
			Target_Index = i;
			break;
		}
	}

	if (Target_Index == -1) {
		printf("ERROR: invalid power target\n");
		return -1;
	}

	switch (Command.CmdId) {
	case GETPOWER:
		/* Voltage */
		sprintf(System_Cmd,
		    "sensors -u %s | grep in2_input | sed \'s/  in2_input: /Voltage(V):\t/\'",
		    Ina226s.Ina226[Target_Index].Sensor_Name);
		system(System_Cmd);

		/* Current */
		sprintf(System_Cmd,
		    "sensors -u %s | grep curr1_input | sed \'s/  curr1_input: /Current(A):\t/\'",
		    Ina226s.Ina226[Target_Index].Sensor_Name);
		system(System_Cmd);

		/* Power */
		sprintf(System_Cmd,
		    "sensors -u %s | grep power1_input | sed \'s/  power1_input: /Power(W):\t/\'",
		    Ina226s.Ina226[Target_Index].Sensor_Name);
		system(System_Cmd);
		break;
	default:
		printf("ERROR: invalid power command\n");
		break;
	}

	return 0;
}

/*
 * Workaround Operations
 */
int Workaround_Ops(void)
{
	int Target_Index = -1;
	unsigned long int Value;
	int Return = -1;

	if (Command.CmdId == LISTWORKAROUND) {
		for (int i = 0; i < Workarounds.Numbers; i++) {
			printf("%s\n", Workarounds.Workaround[i].Name);
		}
		return 0;
	}

	/* Validate the workaround target */
	if (T_Flag == 0) {
		printf("ERROR: no workaround target\n");
		return -1;
	}

	for (int i = 0; i < Workarounds.Numbers; i++) {
		if (strcmp(Target_Arg, (char *)Workarounds.Workaround[i].Name) == 0) {
			Target_Index = i;
			break;
		}
	}

	if (Target_Index == -1) {
		printf("ERROR: invalid workaround target\n");
		return -1;
	}

	/* Does the workaround need argument? */
	if (Workarounds.Workaround[Target_Index].Arg_Needed == 1 && V_Flag == 0) {
		printf("ERROR: no workaround value\n");
		return -1;
	}

	if (V_Flag == 0) {
		Return = (*Workarounds.Workaround[Target_Index].Plat_Workaround_Op)(NULL);
	} else {
		Value = atol(Value_Arg);
		if (Value != 0 || Value != 1) {
			printf("ERROR: invalid value\n");
			return -1;
		}

		Return = (*Workarounds.Workaround[Target_Index].Plat_Workaround_Op)(&Value);
	}

	if (Return == -1) {
		printf("ERROR: failed to apply workaround\n");
		return -1;
	}

	return 0;
}

/*
 * BIT Operations
 */
int BIT_Ops(void)
{
	int Target_Index = -1;
	int Return = -1;

	if (Command.CmdId == LISTBIT) {
		for (int i = 0; i < BITs.Numbers; i++) {
			printf("%s\n", BITs.BIT[i].Name);
		}
		return 0;
	}

	/* Validate the BIT target */
	if (T_Flag == 0) {
		printf("ERROR: no BIT target\n");
		return -1;
	}

	for (int i = 0; i < BITs.Numbers; i++) {
		if (strcmp(Target_Arg, (char *)BITs.BIT[i].Name) == 0) {
			Target_Index = i;
			break;
		}
	}

	if (Target_Index == -1) {
		printf("ERROR: invalid BIT target\n");
		return -1;
	}

	Return = (*BITs.BIT[Target_Index].Plat_BIT_Op)();

	return Return;
}
