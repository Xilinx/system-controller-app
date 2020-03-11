/******************************************************************************
*
* Copyright (C) 2020 Xilinx, Inc.  All rights reserved.
*
* Permission is hereby granted, free of charge, to any person obtaining a copy
* of this software and associated documentation files (the "Software"), to deal
* in the Software without restriction, including without limitation the rights
* to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
* copies of the Software, and to permit persons to whom the Software is
* furnished to do so, subject to the following conditions:
*
* The above copyright notice and this permission notice shall be included in
* all copies or substantial portions of the Software.
*
* THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
* IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
* FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL
* THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
* LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
* OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
* THE SOFTWARE.
*
******************************************************************************/

#include <stdio.h>
#include <stdlib.h>
#include <fcntl.h>
#include <unistd.h>
#include <string.h>
#include "sc_app.h"


extern I2C_Buses_t I2C_Buses;
extern Clocks_t Clocks;
extern Ina226s_t Ina226s;
extern Voltages_t Voltages;

int Parse_Options(int argc, char **argv);
int Clock_Ops(void);
int Power_Ops(void);

static char Usage[] = "\n\
sc_app -c <command> [-t <target> [-v <value>]]\n\n\
<command>:\n\
	listclock - lists the supported clock targets\n\
	getclock - get the frequency of <target>\n\
	setclock - set <target> to <value> frequency\n\
	restoreclock - restore <target> to default value\n\
	listpower - lists the supported power targets\n\
	getpower - get the voltage, current, and power of <target>\n\
";

typedef enum {
	LISTCLOCK,
	GETCLOCK,
	SETCLOCK,
	RESTORECLOCK,
	LISTPOWER,
	GETPOWER,
	COMMAND_MAX,
} CmdId_t;

typedef struct {
	CmdId_t	CmdId;	
	char CmdStr[STRLEN_MAX];
	int (*CmdOps)(void);
} Command_t;

static Command_t Commands[] = {
	{ .CmdId = LISTCLOCK, .CmdStr = "listclock", .CmdOps = Clock_Ops, },
	{ .CmdId = GETCLOCK, .CmdStr = "getclock", .CmdOps = Clock_Ops, },
	{ .CmdId = SETCLOCK, .CmdStr = "setclock", .CmdOps = Clock_Ops, },
	{ .CmdId = RESTORECLOCK, .CmdStr = "restoreclock", .CmdOps = Clock_Ops, },
	{ .CmdId = LISTPOWER, .CmdStr = "listpower", .CmdOps = Power_Ops, },
	{ .CmdId = GETPOWER, .CmdStr = "getpower", .CmdOps = Power_Ops, },
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
 * Clock Operations
 */
int
Clock_Ops(void)
{
	int Target_Index = -1;
	char System_Cmd[SYSCMD_MAX];
	int Value, Upper, Lower;

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

		Value = atoi(Value_Arg);
		Upper = Clocks.Clock[Target_Index].Upper_Freq;
		Lower = Clocks.Clock[Target_Index].Lower_Freq;
		if (Value > Upper || Value < Lower) {
			printf("ERROR: valid frequency range is %d-%d\n",
			    Lower, Upper);
			return -1;
		}
		sprintf(System_Cmd, "echo %d > %s", Value,
		    Clocks.Clock[Target_Index].Sysfs_Path);
		system(System_Cmd);
		break;
	case RESTORECLOCK:
		Value = Clocks.Clock[Target_Index].Default_Freq;
		sprintf(System_Cmd, "echo %d > %s", Value,
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
