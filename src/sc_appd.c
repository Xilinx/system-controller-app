/*
 * Copyright (c) 2020 - 2022 Xilinx, Inc.  All rights reserved.
 * Copyright (c) 2022 - 2023 Advanced Micro Devices, Inc.  All rights reserved.
 *
 * SPDX-License-Identifier: MIT
 */

#include <stdio.h>
#include <stdlib.h>
#include <fcntl.h>
#include <unistd.h>
#include <string.h>
#include <time.h>
#include <math.h>
#include <errno.h>
#include <signal.h>
#include <gpiod.h>
#include <sys/utsname.h>
#include <sys/stat.h>
#include "sc_app.h"

/*
 * Version History
 *
 * 1.0 - Added version support.
 * 1.1 - Support for reading VOUT from voltage regulators.
 * 1.2 - Support for reading DIMM's SPD EEPROM and temperature sensor.
 * 1.3 - Support for reading gpio lines.
 * 1.4 - Support for getting total power of different power domains.
 * 1.5 - Support for IO expander.
 * 1.6 - Support for SFP transceivers.
 * 1.7 - Support for QSFP transceivers.
 * 1.8 - Support for reading EBM's EEPROM.
 * 1.9 - Support for getting board temperature.
 * 1.10 - Support for setting VOUT of voltage regulators.
 * 1.11 - Added 'geteeprom' command to get the entire content of on-board's EEPROM.
 * 1.12 - Support for FPGA Mezzanine Cards (FMCs).
 * 1.13 - Added 'list' command for features that didn't have that option.
 * 1.14 - Added 'listfeature' command to print supported features of the board.
 * 1.15 - Added support for INA226 custom calibration.
 * 1.16 - Added 'board' command to return name of the board.
 * 1.17 - Added 'getbootmode' and enhanced 'setbootmode' commands.
 * 1.18 - Added 'setgpio' command.
 * 1.19 - Consolidate support for all SFP transceiver variants.
 * 1.20 - Added load PDI support.
 */
#define MAJOR	1
#define MINOR	20

#define GPIOLINE	"ZU4_TRIGGER"

int Client_FD;
char Sock_OutBuffer[SOCKBUF_MAX];
char Board_Name[LSTRLEN_MAX];
char Silicon_Revision[STRLEN_MAX];
extern Plat_Devs_t *Plat_Devs;

int Parse_Options(int, char **);
int Constraint_Pre_Ops(void);
int Version_Ops(void);
int Board_Ops(void);
int BootMode_Ops(void);
int Feature_Ops(void);
int EEPROM_Ops(void);
int Temperature_Ops(void);
int Clock_Ops(void);
int Voltage_Ops(void);
int INA226_Ops(void);
int Power_Ops(void);
int Power_Domain_Ops(void);
int Workaround_Ops(void);
int BIT_Ops(void);
int DDR_Ops(void);
int GPIO_Ops(void);
int IO_Exp_Ops(void);
int SFP_Ops(void);
int EBM_Ops(void);
int FMC_Ops(void);
int PDI_Ops(void);
int (*Workaround_Op)(void *);
int FMC_Autodetect_Vadj(void);
int Boot_Set_Clocks(void);
int Boot_Set_Voltages(void);
int Boot_Load_PDI(void);
int Apply_Workarounds(void);
int IO_Exp_Initialized(void);
void *Fancontrol(void*);
static void String_2_Argv(char *, int *, char **);
extern char *Appfile(char *);
extern int Board_Identification(char *);
extern int Silicon_Identification(char *, int);
extern int Reset_Op(void);
extern int Access_Regulator(Voltage_t *, float *, int);
extern int Access_IO_Exp(IO_Exp_t *, int, int, unsigned int *);
extern int Get_GPIO(char *, int *);
extern int Set_GPIO(char *, int);
extern int EEPROM_Common(char *);
extern int EEPROM_Board(char *, int);
extern int EEPROM_MultiRecord(char *, int);
extern int Get_Temperature(Temperature_t *);
extern int Get_BootMode(int);
extern int Set_BootMode(BootMode_t *, int);
extern int Get_IDT_8A34001(Clock_t *);
extern int Set_IDT_8A34001(Clock_t *, char *, int);
extern int Restore_IDT_8A34001(Clock_t *);
extern int QSFP_ModuleSelect(SFP_t *, int);
extern int FMCAutoVadj_Op(void);
extern int Check_Config_File(char *, char *, int *);
extern int XSDB_Op(const char *, const char *, char *, int);

static char Usage[] = "\n\
sc_app -c <command> [-t <target> [-v <value>]]\n\n\
<command>:\n\
	version - version and build information\n\
	board - name of the board\n\
	reset - apply power-on-reset\n\
\n\
	listfeature - list the supported features for this board\n\
\n\
	listeeprom - list the supported EEPROM targets\n\
	geteeprom - get the content of <target> EEPROM for either <value>:\n\
		    'summary', 'all', 'common', 'board', or 'multirecord'\n\
\n\
	listtemp - list the supported temperature sensor targets\n\
	gettemp - get the reading of <target> temperature sensor\n\
\n\
	listbootmode - list the supported boot mode targets\n\
	getbootmode - get boot mode, with optional <value> of 'alternate'\n\
	setbootmode - set boot mode to <target>, with optional <value> of 'alternate'\n\
\n\
	listclock - list the supported clock targets\n\
	getclock - get the frequency of <target>\n\
	setclock - set <target> to <value> frequency\n\
	setbootclock - set <target> to <value> frequency at boot time\n\
	restoreclock - restore <target> to default value\n\
\n\
	listvoltage - list the supported voltage targets\n\
	getvoltage - get the voltage of <target>, with optional <value> of 'all'\n\
	setvoltage - set <target> to <value> volts\n\
	setbootvoltage - set <target> to <value> volts at boot time\n\
	restorevoltage - restore <target> to default value\n\
\n\
	listpower - list the supported power targets\n\
	getpower - get the voltage, current, and power of <target>\n\
	getcalpower - get the voltage, current, and power of custom calibrated <target>\n\
	getINA226 - get the content of <target> registers\n\
	setINA226 - set the 'Configuration', 'Calibration', 'Mask/Enable', and \n\
		    'Alert Limit' registers of <target> to <value>\n\
\n\
	listpowerdomain - list the supported power domain targets\n\
	powerdomain - get the power used by <target> power domain\n\
\n\
	listworkaround - list the applicable workaround targets\n\
	workaround - apply <target> workaround (may requires <value>)\n\
\n\
	listBIT - list the supported Board Interface Test targets\n\
	BIT - run BIT target\n\
\n\
	listddr - list the supported DDR DIMM targets\n\
	getddr - get <target> DDR DIMM information for either <value>:\n\
		 'spd' or 'temp'\n\
\n\
	listgpio - list the supported gpio line targets\n\
	getgpio - get the state of <target> gpio\n\
	setgpio - set the state of <target> gpio to <value>\n\
\n\
	listioexp - list the supported IO expander targets\n\
	getioexp - get the state of <target> IO expander for either <value>:\n\
		   'all', 'input', or 'output'\n\
	setdirioexp - set direction of <target> IO expander to <value>\n\
	setoutioexp - set output of <target> IO expander to <value>\n\
	restoreioexp - restore IO expander to default values\n\
\n\
	listSFP - list the plugged SFP transceiver targets\n\
	getSFP - get the transceiver information of <target> SFP\n\
\n\
	listEBM - list the plugged EBM daughter card targets\n\
	getEBM - get the content of EEPROM on <target> EBM card for either <value>:\n\
		 'all', 'common', 'board', or 'multirecord'\n\
\n\
	listFMC - list the plugged FMCs\n\
	getFMC - get the content of EEPROM on <target> FMC for either <value>:\n\
		 'all', 'common', 'board', or 'multirecord'\n\
\n\
	loadPDI - load <target> PDI to Versal\n\
	setbootPDI - set <target> PDI to be loaded to Versal at boot time\n\
	resetbootPDI - remove any boot PDI that has been set\n\
";

typedef enum {
	VERSION,
	BOARD,
	RESET,
	LISTFEATURE,
	LISTEEPROM,
	GETEEPROM,
	LISTTEMP,
	GETTEMP,
	LISTBOOTMODE,
	GETBOOTMODE,
	SETBOOTMODE,
	LISTCLOCK,
	GETCLOCK,
	SETCLOCK,
	SETBOOTCLOCK,
	RESTORECLOCK,
	LISTVOLTAGE,
	GETVOLTAGE,
	SETVOLTAGE,
	SETBOOTVOLTAGE,
	RESTOREVOLTAGE,
	LISTPOWER,
	GETPOWER,
	GETCALPOWER,
	GETINA226,
	SETINA226,
	LISTPOWERDOMAIN,
	POWERDOMAIN,
	LISTWORKAROUND,
	WORKAROUND,
	LISTBIT,
	BIT,
	LISTDDR,
	GETDDR,
	LISTGPIO,
	GETGPIO,
	SETGPIO,
	LISTIOEXP,
	GETIOEXP,
	SETDIRIOEXP,
	SETOUTIOEXP,
	RESTOREIOEXP,
	LISTSFP,
	GETSFP,
	LISTEBM,
	GETEBM,
	LISTFMC,
	GETFMC,
	LOADPDI,
	SETBOOTPDI,
	RESETBOOTPDI,
	COMMAND_MAX,
} CmdId_t;

typedef struct {
	CmdId_t	CmdId;
	char CmdStr[STRLEN_MAX];
	int (*CmdOps)(void);
} Command_t;

static Command_t Commands[] = {
	{ .CmdId = VERSION, .CmdStr = "version", .CmdOps = Version_Ops, },
	{ .CmdId = BOARD, .CmdStr = "board", .CmdOps = Board_Ops, },
	{ .CmdId = RESET, .CmdStr = "reset", .CmdOps = Reset_Op, },
	{ .CmdId = LISTFEATURE, .CmdStr = "listfeature", .CmdOps = Feature_Ops, },
	{ .CmdId = LISTEEPROM, .CmdStr = "listeeprom", .CmdOps = EEPROM_Ops, },
	{ .CmdId = GETEEPROM, .CmdStr = "geteeprom", .CmdOps = EEPROM_Ops, },
	{ .CmdId = LISTTEMP, .CmdStr = "listtemp", .CmdOps = Temperature_Ops, },
	{ .CmdId = GETTEMP, .CmdStr = "gettemp", .CmdOps = Temperature_Ops, },
	{ .CmdId = LISTBOOTMODE, .CmdStr = "listbootmode", .CmdOps = BootMode_Ops, },
	{ .CmdId = GETBOOTMODE, .CmdStr = "getbootmode", .CmdOps = BootMode_Ops, },
	{ .CmdId = SETBOOTMODE, .CmdStr = "setbootmode", .CmdOps = BootMode_Ops, },
	{ .CmdId = LISTCLOCK, .CmdStr = "listclock", .CmdOps = Clock_Ops, },
	{ .CmdId = GETCLOCK, .CmdStr = "getclock", .CmdOps = Clock_Ops, },
	{ .CmdId = SETCLOCK, .CmdStr = "setclock", .CmdOps = Clock_Ops, },
	{ .CmdId = SETBOOTCLOCK, .CmdStr = "setbootclock", .CmdOps = Clock_Ops, },
	{ .CmdId = RESTORECLOCK, .CmdStr = "restoreclock", .CmdOps = Clock_Ops, },
	{ .CmdId = LISTVOLTAGE, .CmdStr = "listvoltage", .CmdOps = Voltage_Ops, },
	{ .CmdId = GETVOLTAGE, .CmdStr = "getvoltage", .CmdOps = Voltage_Ops, },
	{ .CmdId = SETVOLTAGE, .CmdStr = "setvoltage", .CmdOps = Voltage_Ops, },
	{ .CmdId = SETBOOTVOLTAGE, .CmdStr = "setbootvoltage", .CmdOps = Voltage_Ops, },
	{ .CmdId = RESTOREVOLTAGE, .CmdStr = "restorevoltage", .CmdOps = Voltage_Ops, },
	{ .CmdId = LISTPOWER, .CmdStr = "listpower", .CmdOps = Power_Ops, },
	{ .CmdId = GETPOWER, .CmdStr = "getpower", .CmdOps = Power_Ops, },
	{ .CmdId = GETCALPOWER, .CmdStr = "getcalpower", .CmdOps = Power_Ops, },
	{ .CmdId = GETINA226, .CmdStr = "getINA226", .CmdOps = Power_Ops, },
	{ .CmdId = SETINA226, .CmdStr = "setINA226", .CmdOps = Power_Ops, },
	{ .CmdId = LISTPOWERDOMAIN, .CmdStr = "listpowerdomain", .CmdOps = Power_Domain_Ops, },
	{ .CmdId = POWERDOMAIN, .CmdStr = "powerdomain", .CmdOps = Power_Domain_Ops, },
	{ .CmdId = LISTWORKAROUND, .CmdStr = "listworkaround", .CmdOps = Workaround_Ops, },
	{ .CmdId = WORKAROUND, .CmdStr = "workaround", .CmdOps = Workaround_Ops, },
	{ .CmdId = LISTBIT, .CmdStr = "listBIT", .CmdOps = BIT_Ops, },
	{ .CmdId = BIT, .CmdStr = "BIT", .CmdOps = BIT_Ops, },
	{ .CmdId = LISTDDR, .CmdStr = "listddr", .CmdOps = DDR_Ops, },
	{ .CmdId = GETDDR, .CmdStr = "getddr", .CmdOps = DDR_Ops, },
	{ .CmdId = LISTGPIO, .CmdStr = "listgpio", .CmdOps = GPIO_Ops, },
	{ .CmdId = GETGPIO, .CmdStr = "getgpio", .CmdOps = GPIO_Ops, },
	{ .CmdId = SETGPIO, .CmdStr = "setgpio", .CmdOps = GPIO_Ops, },
	{ .CmdId = LISTIOEXP, .CmdStr = "listioexp", .CmdOps = IO_Exp_Ops, },
	{ .CmdId = GETIOEXP, .CmdStr = "getioexp", .CmdOps = IO_Exp_Ops, },
	{ .CmdId = SETDIRIOEXP, .CmdStr = "setdirioexp", .CmdOps = IO_Exp_Ops, },
	{ .CmdId = SETOUTIOEXP, .CmdStr = "setoutioexp", .CmdOps = IO_Exp_Ops, },
	{ .CmdId = RESTOREIOEXP, .CmdStr = "restoreioexp", .CmdOps = IO_Exp_Ops, },
	{ .CmdId = LISTSFP, .CmdStr = "listSFP", .CmdOps = SFP_Ops, },
	{ .CmdId = GETSFP, .CmdStr = "getSFP", .CmdOps = SFP_Ops, },
	{ .CmdId = LISTEBM, .CmdStr = "listEBM", .CmdOps = EBM_Ops, },
	{ .CmdId = GETEBM, .CmdStr = "getEBM", .CmdOps = EBM_Ops, },
	{ .CmdId = LISTFMC, .CmdStr = "listFMC", .CmdOps = FMC_Ops, },
	{ .CmdId = GETFMC, .CmdStr = "getFMC", .CmdOps = FMC_Ops, },
	{ .CmdId = LOADPDI, .CmdStr = "loadPDI", .CmdOps = PDI_Ops, },
	{ .CmdId = SETBOOTPDI, .CmdStr = "setbootPDI", .CmdOps = PDI_Ops, },
	{ .CmdId = RESETBOOTPDI, .CmdStr = "resetbootPDI", .CmdOps = PDI_Ops, },
};

char Command_Arg[STRLEN_MAX];
char Target_Arg[STRLEN_MAX];
char Value_Arg[LSTRLEN_MAX];
int C_Flag, T_Flag, V_Flag;
static Command_t Command;

int
main()
{
	int Sock_FD;
	unsigned int Length, Recv_Length;
	struct sockaddr_un Server, Client;
	char InBuffer[SYSCMD_MAX];
	int Argc;
	char *Argv[ITEMS_MAX];
	int Ret = -1;
	int Valid_Command;

	SC_INFO(">>> Begin");

	/* Log the version of sc_app */
	SC_INFO("Version:   %d.%d", MAJOR, MINOR);
	SC_INFO("Built:     %s %s", __DATE__, __TIME__);
#ifdef GIT_BRANCH
	SC_INFO("Branch:    %s", GIT_BRANCH);
#endif
#ifdef GIT_COMMIT
	SC_INFO("Commit:    %s", GIT_COMMIT);
#endif
	/* Identify the silicon */
	if (Silicon_Identification(Silicon_Revision,
				   sizeof(Silicon_Revision)) != 0) {
		goto Out;
	}

	/* Identify the board */
	if (Board_Identification(Board_Name) != 0) {
		goto Out;
	}

	/*
	 * Direction of IO Expander ports needs to be initialized
	 * in order for FMC modules to be detected.
	 */
	if (IO_Exp_Initialized() != 0) {
		SC_ERR("failed to initialize IO expander");
		goto Out;
	}

	/* Detect FMC modules and auto adjust voltage */
	if (FMC_Autodetect_Vadj() != 0) {
		SC_ERR("failed to FMC autodetect vadj");
		goto Out;
	}

	/* Set custom clock frequency */
	if (Boot_Set_Clocks() != 0) {
		SC_ERR("failed to set clock frequency");
		goto Out;
	}

	/* Set custom regulator voltage */
	if (Boot_Set_Voltages() != 0) {
		SC_ERR("failed to set regulator voltage");
		goto Out;
	}

	/* Load PDI */
	if (Boot_Load_PDI() != 0) {
		SC_ERR("failed to load PDI");
	}

	/* No pre-set boot mode is supported */
	(void) remove(BOOTMODEFILE);

	/* Applying any workaround */
	if (Apply_Workarounds() != 0) {
		SC_ERR("failed to apply workarounds");
		goto Out;
	}

	if ((Sock_FD = socket(AF_UNIX, SOCK_STREAM, 0)) == -1) {
		SC_ERR("failed to call socket(2): %m");
		goto Out;
	}

	Server.sun_family = AF_UNIX;
	(void) strcpy(Server.sun_path, SOCKFILE);
	unlink(Server.sun_path);
	Length = sizeof(struct sockaddr_un);
	if (bind(Sock_FD, (struct sockaddr *)&Server, Length) == -1) {
		SC_ERR("failed to call bind(2): %m");
		goto Out;
	}

	if (chmod(SOCKFILE, (S_IRWXU|S_IRWXG|S_IRWXO)) == -1) {
		SC_ERR("failed to change socket permission: %m");
		goto Out;
	}

	while (1) {
		Client_FD = 0;
		memset(InBuffer, 0, SYSCMD_MAX);
		if (listen(Sock_FD, 1) == -1) {
			SC_ERR("failed to call listen(2): %m");
			goto Out;
		}

		Client_FD = accept(Sock_FD, (struct sockaddr *)&Client, &Length);
		if (Client_FD == -1) {
			SC_ERR("failed to call accept(2): %m");
			goto Out;
		}

		Recv_Length = recv(Client_FD, InBuffer, SYSCMD_MAX, 0);
		if (Recv_Length == -1) {
			SC_ERR("failed to call recv(2): %m");
			goto Out;
		}

		Valid_Command = 0;
		if (strstr(InBuffer, Commands[GETTEMP].CmdStr) == NULL) {
			SC_INFO(">>> Command: %s", InBuffer);
		}

		String_2_Argv(InBuffer, &Argc, &Argv[0]);

		if (Parse_Options(Argc, Argv) != 0) {
			goto Next;
		}

		if ((Ret = Constraint_Pre_Ops()) != 0) {
			goto Next;
		}

		for (int i = 0; i < COMMAND_MAX; i++) {
			if (strcmp(Command_Arg, (char *)Commands[i].CmdStr) == 0) {
				Command = Commands[i];
				Valid_Command = 1;
				break;
			}
		}

		if (!Valid_Command) {
			SC_ERR("invalid command");
			goto Next;
		}

		(void) (*Command.CmdOps)();
		fflush(stdout);
Next:
		for (int i = 0; i < Argc; i++) {
			free(Argv[i]);
		}

		(void) close(Client_FD);
	}

Out:
	SC_INFO("<<< End(%d)", Ret);
	return Ret;
}

static void
String_2_Argv(char *Buffer, int *Argc, char *Argv[])
{
	char *Char_p, *Start_p, *End_p;
	char Delimiter = ' ';
	char *Alloc_p;

	*Argc = 0;
	Char_p = Buffer;
	End_p = Char_p + strlen(Buffer);
	while (Char_p < End_p) {
		if (*Char_p == '\'') {
			Delimiter = '\'';
		}

		/* Skip delimiters */
		while ((Char_p < End_p) && (*Char_p == Delimiter)) {
			Char_p++;
		}

		/* Identify the next string token */
		Start_p = Char_p;
		while ((Char_p < End_p) && (*Char_p != Delimiter)) {
			Char_p++;
		}

		*Char_p = '\0';
		Alloc_p = malloc(strlen(Start_p) + 1);
		(void) strcpy(Alloc_p, Start_p);
		Argv[*Argc] = Alloc_p;

		Char_p++;
		(*Argc)++;
		Delimiter = ' ';
	}
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
	optind = 0;
	C_Flag = T_Flag = V_Flag = 0;
	memset(Command_Arg, 0, STRLEN_MAX);
	memset(Target_Arg, 0, STRLEN_MAX);
	memset(Value_Arg, 0, LSTRLEN_MAX);
	while ((c = getopt(argc, argv, "hc:t:v:")) != -1) {
		Options++;
		switch (c) {
		case 'h':
			SC_PRINT("%s", Usage);
			return 1;
			break;
		case 'c':
			C_Flag = 1;
			(void) strncpy(Command_Arg, optarg, sizeof(Command_Arg));
			break;
		case 't':
			T_Flag = 1;
			(void) strncpy(Target_Arg, optarg, sizeof(Target_Arg));
			break;
		case 'v':
			V_Flag = 1;
			(void) strncpy(Value_Arg, optarg, sizeof(Value_Arg));
			break;
		case '?':
			SC_ERR("invalid argument");
			SC_PRINT("%s", Usage);
			return -1;
		default:
			SC_PRINT("%s", Usage);
			return -1;
		}
	}

	if (Options == 0) {
		SC_PRINT("%s", Usage);
		return -1;
	}

	return 0;
}

/*
 * Process commands with pre_phase constraints
 */
int
Constraint_Pre_Ops()
{
	int Target_Index = -1;
	Constraints_t *Constraints;
	Constraint_t *Constraint;
	Constraint_Phases_t *Pre_Phases;
	FILE *FP;
	char Output[STRLEN_MAX] = { 0 };
	char System_Cmd[SYSCMD_MAX];

	Constraints = Plat_Devs->Constraints;
	if (Constraints == NULL) {
		return 0;
	}

	for (int i = 0; i < Constraints->Numbers; i++) {
		if (strcmp(Command_Arg, Constraints->Constraint[i].Command) != 0) {
			continue;
		}

		if ((!T_Flag && (Constraints->Constraint[i].Target != NULL)) ||
		    (T_Flag && (Constraints->Constraint[i].Target == NULL))) {
			continue;
		}

		if (T_Flag &&
		    ((strcmp(Target_Arg, Constraints->Constraint[i].Target) != 0) &&
		     (strcmp("$ANY", Constraints->Constraint[i].Target) != 0))) {
			continue;
		}

		if ((!V_Flag && (Constraints->Constraint[i].Value != NULL)) ||
		    (V_Flag && (Constraints->Constraint[i].Value == NULL))) {
			continue;
		}

		if (V_Flag &&
		    ((strcmp(Value_Arg, Constraints->Constraint[i].Value) != 0) &&
		     (strcmp("$ANY", Constraints->Constraint[i].Value) != 0))) {
			continue;
		}

		Target_Index = i;
		Constraint = &Constraints->Constraint[Target_Index];
		break;
	}

	if (Target_Index == -1) {
		return 0;
	}

	SC_INFO("Constraint command: %s %s %s", Constraint->Command,
		 ((Constraint->Target != NULL) ? Constraint->Target : ""),
		 ((Constraint->Value != NULL) ? Constraint->Value : ""));

	if (Constraint->Pre_Phases == NULL) {
		return 0;
	}

	Pre_Phases = Constraint->Pre_Phases;
	for (int i = 0; i < Pre_Phases->Numbers; i++) {
		if (strcmp(Pre_Phases->Phase[i].Type, "External") == 0) {
			SC_INFO("Executing external command: %s",
				Pre_Phases->Phase[i].Command);
			if ((Pre_Phases->Phase[i].Args != NULL) &&
			    (strcmp(Pre_Phases->Phase[i].Args, "$PASSON") == 0)) {
				(void) sprintf(System_Cmd, "%s%s/%s %s %s",
					       BIT_PATH, Board_Name,
					       Pre_Phases->Phase[i].Command,
					       Target_Arg, Value_Arg);
			} else {
				(void) sprintf(System_Cmd, "%s%s/%s %s",
					       BIT_PATH, Board_Name,
					       Pre_Phases->Phase[i].Command,
					       ((Pre_Phases->Phase[i].Args != NULL) ?
					       Pre_Phases->Phase[i].Args : ""));
			}

			SC_INFO("Command in full path: %s", System_Cmd);
			FP = popen(System_Cmd, "r");
			if (FP == NULL) {
				SC_ERR("failed to invoke %s: %m", System_Cmd);
				return -1;
			}

			while (fgets(Output, sizeof(Output), FP) != NULL) {
				SC_PRINT_N("%s", Output);
				if (strstr(Output, "ERROR: ") != NULL) {
					(void) pclose(FP);
					return -1;
				}
			}

			(void) pclose(FP);

		} else if (strcmp(Pre_Phases->Phase[i].Type, "Internal") == 0) {
			SC_INFO("Executing internal command: %s",
				Pre_Phases->Phase[i].Command);

			/* These are currently supported internal routines */
			if (strcmp(Pre_Phases->Phase[i].Command, "FMC_Autodetect_Vadj") == 0) {
				if (FMC_Autodetect_Vadj() != 0) {
					SC_ERR("failed to FMC autodetect vadj");
					return -1;
				}
			}

		} else {
			SC_ERR("invalid Pre_Phase command type");
			return -1;
		}
	}

	return ((strcmp(Constraint->Type, "Terminate") == 0) ? 1 : 0);
}

/*
 * Version Operations
 */
int
Version_Ops(void)
{
	SC_PRINT("Version:\t%d.%d", MAJOR, MINOR);
	SC_PRINT("Built:\t\t%s %s", __DATE__, __TIME__);
#ifdef GIT_BRANCH
	SC_PRINT("Branch:\t\t%s", GIT_BRANCH);
#endif
#ifdef GIT_COMMIT
	SC_PRINT("Commit:\t\t%s", GIT_COMMIT);
#endif
	return 0;
}

int
Board_Ops(void)
{
	if (Board_Name[0] == 0) {
		if (Board_Identification(Board_Name) != 0) {
			return -1;
		}
	}

	SC_PRINT("%s", Board_Name);
	return 0;
}

/*
 * Boot Mode Operations
 */
int
BootMode_Ops(void)
{
	int Target_Index = -1;
	BootModes_t *BootModes;
	BootMode_t *BootMode;

	BootModes = Plat_Devs->BootModes;
	if (BootModes == NULL) {
		SC_ERR("bootmode operation is not supported");
		return -1;
	}

	if (Command.CmdId == LISTBOOTMODE) {
		for (int i = 0; i < BootModes->Numbers; i++) {
			SC_PRINT("%s\t0x%x", BootModes->BootMode[i].Name,
				 BootModes->BootMode[i].Value);
		}

		return 0;
	}

	if (Command.CmdId == GETBOOTMODE) {
		if ((T_Flag == 0) && (V_Flag == 0)) {
			return Get_BootMode(0);
		}

		if ((strcmp(Target_Arg, "alternate") != 0) &&
		    (strcmp(Value_Arg, "alternate") != 0)) {
			SC_ERR("invalid get boot mode value");
			return -1;
		}

		return Get_BootMode(1);
	}

	if (Command.CmdId != SETBOOTMODE) {
		SC_ERR("invalid boot mode command");
		return -1;
	}

	/* Validate the bootmode target */
	if (T_Flag == 0) {
		SC_ERR("no set boot mode target");
		return -1;
	}

	for (int i = 0; i < BootModes->Numbers; i++) {
		if (strcmp(Target_Arg, (char *)BootModes->BootMode[i].Name) == 0) {
			Target_Index = i;
			BootMode = &BootModes->BootMode[Target_Index];
			break;
		}
	}

	if (Target_Index == -1) {
		SC_ERR("invalid set boot mode target");
		return -1;
	}

	if (V_Flag == 0) {
		return Set_BootMode(BootMode, 0);
	}

	if (strcmp(Value_Arg, "alternate") != 0) {
		SC_ERR("invalid set boot mode value");
		return -1;
	}

	return Set_BootMode(BootMode, 1);
}

int
Feature_Ops(void)
{
	FeatureList_t *FeatureList;

	if (Plat_Devs->FeatureList == NULL) {
		SC_ERR("no feature list!");
		return -1;
	}

	FeatureList = Plat_Devs->FeatureList;
	for (int i = 0; i < FeatureList->Numbers; i++) {
		SC_PRINT("%s", FeatureList->Feature[i]);
	}

	return 0;
}

void
EEPROM_Print_All(char *Buffer, int Size, int Partition)
{
	SC_PRINT_N("    ");
	for (int i = 0; i < Partition; i++) {
		SC_PRINT_N("%2x ", i);
	}

	SC_PRINT_N("\n00: ");
	for (int i = 0, j = Partition; i < Size; i++) {
		SC_PRINT_N("%.2x ", Buffer[i]);
		if (((i + 1) % Partition) == 0) {
			if ((i + 1) < Size) {
				SC_PRINT_N("\n%.2x: ", j);
				j += Partition;
			}
		}
	}

	SC_PRINT_N("\n");
	return;
}

/*
 * EEPROM Operations
 */
int
EEPROM_Ops(void)
{
	EEPROM_Targets Target;
	OnBoard_EEPROM_t *OnBoard_EEPROM;
	int FD;
	char In_Buffer[SYSCMD_MAX];
	char Out_Buffer[STRLEN_MAX];
	char Buffer[STRLEN_MAX];
	struct tm BuildDate = { 0 };
	time_t Time;
	int Offset, Length;

	OnBoard_EEPROM = Plat_Devs->OnBoard_EEPROM;
	if (OnBoard_EEPROM == NULL) {
		SC_ERR("eeprom operation is not supported");
		return -1;
	}

	if (Command.CmdId == LISTEEPROM) {
		SC_PRINT("%s", OnBoard_EEPROM->Name);
		return 0;
	}

	/* Validate the target for geteeprom command */
	if (T_Flag == 0) {
		SC_ERR("no geteeprom target");
		return -1;
	}

	if (strcmp(Target_Arg, OnBoard_EEPROM->Name) != 0) {
		SC_ERR("invalid geteeprom target");
		return -1;
	}

	if (V_Flag == 0) {
		SC_ERR("no value is provided for geteeprom");
		return -1;
	}

	if (strcmp(Value_Arg, "summary") == 0) {
		Target = EEPROM_SUMMARY;
	} else if (strcmp(Value_Arg, "all") == 0) {
		Target = EEPROM_ALL;
	} else if (strcmp(Value_Arg, "common") == 0) {
		Target = EEPROM_COMMON;
	} else if (strcmp(Value_Arg, "board") == 0) {
		Target = EEPROM_BOARD;
	} else if (strcmp(Value_Arg, "multirecord") == 0) {
		Target = EEPROM_MULTIRECORD;
	} else {
		SC_ERR("invalid geteeprom value");
		return -1;
	}

	FD = open(OnBoard_EEPROM->I2C_Bus, O_RDWR);
	if (FD < 0) {
		SC_ERR("unable to access I2C bus %s: %m", OnBoard_EEPROM->I2C_Bus);
		return -1;
	}

	if (ioctl(FD, I2C_SLAVE_FORCE, OnBoard_EEPROM->I2C_Address) < 0) {
		SC_ERR("unable to access onboard EEPROM address %#x",
		       OnBoard_EEPROM->I2C_Address);
		(void) close(FD);
		return -1;
	}

	Out_Buffer[0] = 0x0;
	Out_Buffer[1] = 0x0;
	SC_INFO("Write offset address 0x%.2x%.2x", Out_Buffer[0], Out_Buffer[1]);
	if (write(FD, Out_Buffer, 2) != 2) {
		SC_ERR("unable to set the offset address on onboard EEPROM");
		(void) close(FD);
		return -1;
	}

	(void) memset(In_Buffer, 0, SYSCMD_MAX);
	if (read(FD, In_Buffer, 256) != 256) {
		SC_ERR("unable to read onboard EEPROM");
		(void) close(FD);
		return -1;
	}

	(void) close(FD);
	switch (Target) {
	case EEPROM_SUMMARY:
		SC_PRINT("Language: %d", In_Buffer[0xA]);
		if (Silicon_Identification(Silicon_Revision,
					   sizeof(Silicon_Revision)) != 0) {
			return -1;
		}

		SC_PRINT("Silicon Revision: %s", Silicon_Revision);

		/* Base build date for manufacturing is 1/1/1996 */
		SC_INFO("Manufacturing date: [0xD] = %#x, [0xC] = %#x, [0xB] = %#x",
		        In_Buffer[0xD], In_Buffer[0xC], In_Buffer[0xB]);
		BuildDate.tm_year = 96;
		BuildDate.tm_mday = 1;
		BuildDate.tm_min = (In_Buffer[0xD] << 16 | In_Buffer[0xC] << 8 |
				    In_Buffer[0xB]);
		Time = mktime(&BuildDate);
		if (Time == -1) {
			SC_ERR("invalid manufacturing date");
			return -1;
		}

		SC_PRINT_N("Manufacturing Date: %s", ctime(&Time));
		Offset = 0xE;
		Length = (In_Buffer[Offset] & 0x3F);
		snprintf(Buffer, Length + 1, "%s", &In_Buffer[Offset + 1]);
		SC_PRINT("Manufacturer: %s", Buffer);
		Offset = Offset + Length + 1;
		Length = (In_Buffer[Offset] & 0x3F);
		snprintf(Buffer, Length + 1, "%s", &In_Buffer[Offset + 1]);
		SC_PRINT("Product Name: %s", Buffer);
		Offset = Offset + Length + 1;
		Length = (In_Buffer[Offset] & 0x3F);
		snprintf(Buffer, Length + 1, "%s", &In_Buffer[Offset + 1]);
		SC_PRINT("Board Serial Number: %s", Buffer);
		Offset = Offset + Length + 1;
		Length = (In_Buffer[Offset] & 0x3F);
		snprintf(Buffer, Length + 1, "%s", &In_Buffer[Offset + 1]);
		SC_PRINT("Board Part Number: %s", Buffer);
		Offset = Offset + Length + 1;
		Length = (In_Buffer[Offset] & 0x3F);
		/* Skip FRU File ID */
		Offset = Offset + Length + 1;
		Length = (In_Buffer[Offset] & 0x3F);
		snprintf(Buffer, Length + 1, "%s", &In_Buffer[Offset + 1]);
		SC_PRINT("Board Revision: %s", Buffer);
		EEPROM_MultiRecord(In_Buffer, 1);
		break;
	case EEPROM_ALL:
		EEPROM_Print_All(In_Buffer, 256, 16);
		break;
	case EEPROM_COMMON:
		return EEPROM_Common(In_Buffer);
	case EEPROM_BOARD:
		return EEPROM_Board(In_Buffer, 1);
	case EEPROM_MULTIRECORD:
		return EEPROM_MultiRecord(In_Buffer, 0);
	default:
		SC_ERR("invalid geteeprom target");
		return -1;
	}

	return 0;
}

int
Temperature_Ops(void)
{
	Temperature_t *Temperature;

	Temperature = Plat_Devs->Temperature;
	if (Temperature == NULL) {
		SC_ERR("temp operation is not supported");
		return -1;
	}

	if (Command.CmdId == LISTTEMP) {
		SC_PRINT("%s", Temperature->Name);
		return 0;
	}

	/* Validate the target for gettemp command */
	if (T_Flag == 0) {
		SC_ERR("no gettemp target");
		return -1;
	}

	if (strcmp(Target_Arg, Temperature->Name) != 0) {
		SC_ERR("invalid gettemp target");
		return -1;
	}

	return Get_Temperature(Temperature);
}

/*
 * Clock Operations
 */
int
Clock_Ops(void)
{
	int Target_Index = -1;
	Clocks_t *Clocks;
	Clock_t *Clock;
	FILE *FP;
	char System_Cmd[2 * SYSCMD_MAX];
	char Output[STRLEN_MAX];
	double Frequency;
	double Upper, Lower;

	Clocks = Plat_Devs->Clocks;
	if (Clocks == NULL) {
		SC_ERR("clock operation is not supported");
		return -1;
	}

	if (Command.CmdId == LISTCLOCK) {
		for (int i = 0; i < Clocks->Numbers; i++) {
			if (Clocks->Clock[i].Type == IDT_8A34001) {
				SC_PRINT("%s", Clocks->Clock[i].Name);
			} else if (Clocks->Clock[i].Lower_Freq == -1 &&
				   Clocks->Clock[i].Upper_Freq == -1) {
				SC_PRINT("%s - (%.3f MHz)",
					 Clocks->Clock[i].Name,
					 Clocks->Clock[i].Default_Freq);
			} else {
				SC_PRINT("%s - (%.3f MHz - %.3f MHz)",
					 Clocks->Clock[i].Name,
					 Clocks->Clock[i].Lower_Freq,
					 Clocks->Clock[i].Upper_Freq);
			}
		}

		return 0;
	}

	/* Validate the clock target */
	if (T_Flag == 0) {
		SC_ERR("no clock target");
		return -1;
	}

	for (int i = 0; i < Clocks->Numbers; i++) {
		if (strcmp(Target_Arg, (char *)Clocks->Clock[i].Name) == 0) {
			Target_Index = i;
			Clock = &Clocks->Clock[Target_Index];
			break;
		}
	}

	if (Target_Index == -1) {
		SC_ERR("invalid clock target");
		return -1;
	}

	switch (Command.CmdId) {
	case GETCLOCK:
		if (Clock->Type == IDT_8A34001) {
			return Get_IDT_8A34001(Clock);
		}

		(void) sprintf(System_Cmd, "cat %s", Clock->Sysfs_Path);
		FP = popen(System_Cmd, "r");
		if (FP == NULL) {
			SC_ERR("failed to access sysfs path %s: %m",
			       Clock->Sysfs_Path);
			return -1;
		}

		(void) fgets(Output, sizeof(Output), FP);
		(void) pclose(FP);
		SC_INFO("%s: %s", Clock->Sysfs_Path, Output);
		Frequency = strtod(Output, NULL) / 1000000.0;	// In MHz
		/* Print out 3-digit after decimal point without rounding */
		SC_PRINT("Frequency(MHz):\t%.3f",
		   ((signed long)(Frequency * 1000) * 0.001f));
		break;
	case SETCLOCK:
	case SETBOOTCLOCK:
		/* Validate the frequency */
		if (V_Flag == 0) {
			SC_ERR("no value is provided for clock");
			return -1;
		}

		if (Clock->Type == IDT_8A34001) {
			if (Command.CmdId == SETCLOCK) {
				return Set_IDT_8A34001(Clock, Value_Arg, 0);
			} else {
				return Set_IDT_8A34001(Clock, Value_Arg, 1);
			}
		}

		Frequency = strtod(Value_Arg, NULL);
		Upper = Clock->Upper_Freq;
		Lower = Clock->Lower_Freq;

		/* restrict change in frequency if Upper & Lower are not set */
		if (Upper == -1 && Lower == -1) {
			SC_ERR("change of clock frequency is not permitted");
			return -1;
		}

		if (Frequency > Upper || Frequency < Lower) {
			SC_ERR("valid frequency range is %.3f MHz - %.3f MHz",
			    Lower, Upper);
			return -1;
		}

		(void) sprintf(System_Cmd, "echo %u > %s",
		    (unsigned int)(Frequency * 1000000), Clock->Sysfs_Path);
		SC_INFO("Command: %s", System_Cmd);
		system(System_Cmd);

		if (Command.CmdId == SETBOOTCLOCK) {
			/* Remove the old value, if any */
			(void) sprintf(System_Cmd, "sed -i -e \'/^%s:/d\' %s 2> /dev/NULL",
			    Clock->Name, CLOCKFILE);
			SC_INFO("Command: %s", System_Cmd);
			system(System_Cmd);

			(void) sprintf(System_Cmd, "%s:\t%.3f\n", Clock->Name,
			    Frequency);
			FP = fopen(CLOCKFILE, "a");
			if (FP == NULL) {
				SC_ERR("failed to append clock file %s: %m", CLOCKFILE);
				return -1;
			}

			SC_INFO("Write clock: %s", System_Cmd);
			(void) fprintf(FP, "%s", System_Cmd);
			(void) fflush(FP);
			(void) fsync(fileno(FP));
			(void) fclose(FP);
		}

		break;
	case RESTORECLOCK:
		if (Clock->Upper_Freq == -1 && Clock->Lower_Freq == -1) {
			SC_INFO("no need to restore frequency of a fixed clock");
			break;
		}

		if (Clock->Type == IDT_8A34001) {
			return Restore_IDT_8A34001(Clock);
		}

		Frequency = Clock->Default_Freq;
		(void) sprintf(System_Cmd, "echo %u > %s",
		    (unsigned int)(Frequency * 1000000), Clock->Sysfs_Path);
		SC_INFO("Command: %s", System_Cmd);
		system(System_Cmd);

		/* Remove any custom boot frequency */
		(void) sprintf(System_Cmd, "sed -i -e \'/^%s:/d\' %s 2> /dev/NULL; "
			       "sync", Clock->Name, CLOCKFILE);
		SC_INFO("Command: %s", System_Cmd);
		system(System_Cmd);
		break;
	default:
		SC_ERR("invalid clock command");
		return -1;
	}

	return 0;
}

/*
 * Voltage Operations
 */
int Voltage_Ops(void)
{
	int Target_Index = -1;
	Voltages_t *Voltages;
	Voltage_t *Regulator;
	FILE *FP;
	char System_Cmd[SYSCMD_MAX];
	float Voltage;

	Voltages = Plat_Devs->Voltages;
	if (Voltages == NULL) {
		SC_ERR("voltage operation is not suppoprted");
		return -1;
	}

	if (Command.CmdId == LISTVOLTAGE) {
		for (int i = 0; i < Voltages->Numbers; i++) {
			if (Voltages->Voltage[i].Voltage_Multiplier != 0) {
				SC_PRINT("%s - (%.2f V)", Voltages->Voltage[i].Name,
					 (Voltages->Voltage[i].Typical_Volt *
					  Voltages->Voltage[i].Voltage_Multiplier));
			} else {
				SC_PRINT("%s - (%.2f V)", Voltages->Voltage[i].Name,
					 Voltages->Voltage[i].Typical_Volt);
			}
		}

		return 0;
	}

	/* Validate the voltage target */
	if (T_Flag == 0) {
		SC_ERR("no voltage target");
		return -1;
	}

	for (int i = 0; i < Voltages->Numbers; i++) {
		if (strcmp(Target_Arg, (char *)Voltages->Voltage[i].Name) == 0) {
			Target_Index = i;
			Regulator = &Voltages->Voltage[Target_Index];
			break;
		}
	}

	if (Target_Index == -1) {
		SC_ERR("invalid voltage target");
		return -1;
	}

	switch (Command.CmdId) {
	case GETVOLTAGE:
		if (Access_Regulator(Regulator, &Voltage, 0) != 0) {
			SC_ERR("failed to get voltage from regulator");
			return -1;
		}

		if (Regulator->Voltage_Multiplier != 0) {
			Voltage *= Regulator->Voltage_Multiplier;
		}

		SC_PRINT("Voltage(V):\t%.2f", Voltage);

		if (V_Flag != 0) {
			if (strcmp(Value_Arg, "all") != 0) {
				SC_ERR("invalid value argument %s", Value_Arg);
				return -1;
			}

			return Access_Regulator(Regulator, &Voltage, 2);
		}

		break;
	case SETVOLTAGE:
	case SETBOOTVOLTAGE:
		if (V_Flag == 0) {
			SC_ERR("no voltage value");
			return -1;
		}

		Voltage = strtof(Value_Arg, NULL);
		if (Access_Regulator(Regulator, &Voltage, 1) != 0) {
			SC_ERR("failed to set voltage of regulator");
			return -1;
		}

		if (Command.CmdId == SETBOOTVOLTAGE) {
			/* Remove the old value, if any */
			(void) sprintf(System_Cmd, "sed -i -e \'/^%s:/d\' %s 2> /dev/NULL",
			    Regulator->Name, VOLTAGEFILE);
			SC_INFO("Command: %s", System_Cmd);
			system(System_Cmd);

			(void) sprintf(System_Cmd, "%s:\t%.3f\n", Regulator->Name,
			    Voltage);
			FP = fopen(VOLTAGEFILE, "a");
			if (FP == NULL) {
				SC_ERR("failed to append voltage file %s: %m",
				       VOLTAGEFILE);
				return -1;
			}

			SC_INFO("Append: %s", System_Cmd);
			(void) fprintf(FP, "%s", System_Cmd);
			(void) fflush(FP);
			(void) fsync(fileno(FP));
			(void) fclose(FP);
		}

		break;
	case RESTOREVOLTAGE:
		Voltage = Regulator->Typical_Volt;
		if (Access_Regulator(Regulator, &Voltage, 1) != 0) {
			SC_ERR("failed to set voltage of regulator");
			return -1;
		}

		/* Remove any custom boot voltage */
		(void) sprintf(System_Cmd, "sed -i -e \'/^%s:/d\' %s 2> /dev/NULL; "
			       "sync", Regulator->Name, VOLTAGEFILE);
		SC_INFO("Command: %s", System_Cmd);
		system(System_Cmd);
		break;
	default:
		SC_ERR("invalid voltage command");
		break;
	}

	return 0;
}

typedef struct {
	unsigned short Configuration;
	unsigned short Shunt_Voltage;
	unsigned short Bus_Voltage;
	unsigned short Power;
	unsigned short Current;
	unsigned short Calibration;
	unsigned short Mask_Enable;
	unsigned short Alert_Limit;
	unsigned short Die_ID;
	unsigned short Set_Registers;
} INA226_Regs_t;

typedef enum {
	INA226_Configuration = 0x1,
	INA226_Calibration = 0x2,
	INA226_Mask_Enable = 0x4,
	INA226_Alert_Limit = 0x8,
} INA226_RegsMap_t;

int
Read_INA226(INA226_t *INA226, INA226_Regs_t *Regs)
{
	int FD;
	char In_Buffer[STRLEN_MAX];
	char Out_Buffer[STRLEN_MAX];
	int Ret = 0;

	FD = open(INA226->I2C_Bus, O_RDWR);
	if (FD < 0) {
		SC_ERR("unable to access I2C bus %s: %m", INA226->I2C_Bus);
		return -1;
	}

	/* Read 'Configuration' register */
	(void) memset(Out_Buffer, 0, STRLEN_MAX);
	(void) memset(In_Buffer, 0, STRLEN_MAX);
	Out_Buffer[0] = 0x0;	// Configuration Register(00h)
	I2C_READ(FD, INA226->I2C_Address, 2, Out_Buffer, In_Buffer, Ret);
	if (Ret != 0) {
		(void) close(FD);
		return Ret;
	}

	SC_INFO("Configuration Register(00h): %#x %#x", In_Buffer[0],
		In_Buffer[1]);
	Regs->Configuration = ((In_Buffer[0] << 8) | In_Buffer[1]);

	/* Read 'Shunt Voltage' register */
	(void) memset(Out_Buffer, 0, STRLEN_MAX);
	(void) memset(In_Buffer, 0, STRLEN_MAX);
	Out_Buffer[0] = 0x1;	// Shunt Voltage Register(01h)
	I2C_READ(FD, INA226->I2C_Address, 2, Out_Buffer, In_Buffer, Ret);
	if (Ret != 0) {
		(void) close(FD);
		return Ret;
	}

	SC_INFO("Shunt Voltage Register(01h): %#x %#x", In_Buffer[0],
		In_Buffer[1]);
	Regs->Shunt_Voltage = ((In_Buffer[0] << 8) | In_Buffer[1]);

	/* Read 'Bus Voltage' register */
	(void) memset(Out_Buffer, 0, STRLEN_MAX);
	(void) memset(In_Buffer, 0, STRLEN_MAX);
	Out_Buffer[0] = 0x2;	// Bus Voltage Register(02h)
	I2C_READ(FD, INA226->I2C_Address, 2, Out_Buffer, In_Buffer, Ret);
	if (Ret != 0) {
		(void) close(FD);
		return Ret;
	}

	SC_INFO("Bus Voltage Register(02h): %#x %#x", In_Buffer[0],
		In_Buffer[1]);
	Regs->Bus_Voltage = ((In_Buffer[0] << 8) | In_Buffer[1]);

	/* Read 'Power' register */
	(void) memset(Out_Buffer, 0, STRLEN_MAX);
	(void) memset(In_Buffer, 0, STRLEN_MAX);
	Out_Buffer[0] = 0x3;	// Power Register(03h)
	I2C_READ(FD, INA226->I2C_Address, 2, Out_Buffer, In_Buffer, Ret);
	if (Ret != 0) {
		(void) close(FD);
		return Ret;
	}

	SC_INFO("Power Register(03h): %#x %#x", In_Buffer[0],
		In_Buffer[1]);
	Regs->Power = ((In_Buffer[0] << 8) | In_Buffer[1]);

	/* Read 'Current' register */
	(void) memset(Out_Buffer, 0, STRLEN_MAX);
	(void) memset(In_Buffer, 0, STRLEN_MAX);
	Out_Buffer[0] = 0x4;	// Current Register(04h)
	I2C_READ(FD, INA226->I2C_Address, 2, Out_Buffer, In_Buffer, Ret);
	if (Ret != 0) {
		(void) close(FD);
		return Ret;
	}

	SC_INFO("Current Register(04h): %#x %#x", In_Buffer[0],
		In_Buffer[1]);
	Regs->Current = ((In_Buffer[0] << 8) | In_Buffer[1]);

	/* Read 'Calibration' register */
	(void) memset(Out_Buffer, 0, STRLEN_MAX);
	(void) memset(In_Buffer, 0, STRLEN_MAX);
	Out_Buffer[0] = 0x5;	// Calibration Register(05h)
	I2C_READ(FD, INA226->I2C_Address, 2, Out_Buffer, In_Buffer, Ret);
	if (Ret != 0) {
		(void) close(FD);
		return Ret;
	}

	SC_INFO("Calibration Register(05h): %#x %#x", In_Buffer[0],
		In_Buffer[1]);
	Regs->Calibration = ((In_Buffer[0] << 8) | In_Buffer[1]);

	/* Read 'Mask/Enable' register */
	(void) memset(Out_Buffer, 0, STRLEN_MAX);
	(void) memset(In_Buffer, 0, STRLEN_MAX);
	Out_Buffer[0] = 0x6;	// Mask/Enable Register(06h)
	I2C_READ(FD, INA226->I2C_Address, 2, Out_Buffer, In_Buffer, Ret);
	if (Ret != 0) {
		(void) close(FD);
		return Ret;
	}

	SC_INFO("Mask/Enable Register(06h): %#x %#x", In_Buffer[0],
		In_Buffer[1]);
	Regs->Mask_Enable = ((In_Buffer[0] << 8) | In_Buffer[1]);

	/* Read 'Alert Limit' register */
	(void) memset(Out_Buffer, 0, STRLEN_MAX);
	(void) memset(In_Buffer, 0, STRLEN_MAX);
	Out_Buffer[0] = 0x7;	// Alert Limit Register(07h)
	I2C_READ(FD, INA226->I2C_Address, 2, Out_Buffer, In_Buffer, Ret);
	if (Ret != 0) {
		(void) close(FD);
		return Ret;
	}

	SC_INFO("Alert Limit Register(07h): %#x %#x", In_Buffer[0],
		In_Buffer[1]);
	Regs->Alert_Limit = ((In_Buffer[0] << 8) | In_Buffer[1]);

	/* Read 'Die ID' register */
	(void) memset(Out_Buffer, 0, STRLEN_MAX);
	(void) memset(In_Buffer, 0, STRLEN_MAX);
	Out_Buffer[0] = 0xFF;	// Die ID Register(FFh)
	I2C_READ(FD, INA226->I2C_Address, 2, Out_Buffer, In_Buffer, Ret);
	if (Ret != 0) {
		(void) close(FD);
		return Ret;
	}

	SC_INFO("Die ID Register(FFh): %#x %#x", In_Buffer[0],
		In_Buffer[1]);
	Regs->Die_ID = ((In_Buffer[0] << 8) | In_Buffer[1]);

	(void) close(FD);
	return 0;
}

int
Write_INA226(INA226_t *INA226, INA226_Regs_t *Regs)
{
	int FD;
	char Out_Buffer[STRLEN_MAX];
	int Ret = 0;

	FD = open(INA226->I2C_Bus, O_RDWR);
	if (FD < 0) {
		SC_ERR("unable to access I2C bus %s: %m", INA226->I2C_Bus);
		return -1;
	}

	if (Regs->Set_Registers & INA226_Configuration) {
		/* Write 'Configuration' register */
		(void) memset(Out_Buffer, 0, STRLEN_MAX);
		Out_Buffer[0] = 0x0;   // Configuration Register(00h)
		Out_Buffer[1] = (Regs->Configuration >> 8);
		Out_Buffer[2] = (Regs->Configuration & 0xFF);
		SC_INFO("Configuration Register(00h): %#x %#x", Out_Buffer[1],
			Out_Buffer[2]);
		I2C_WRITE(FD, INA226->I2C_Address, 3, Out_Buffer, Ret);
		if (Ret != 0) {
			(void) close(FD);
			return Ret;
		}
	}

	if (Regs->Set_Registers & INA226_Calibration) {
		/* Write 'Calibration' register */
		(void) memset(Out_Buffer, 0, STRLEN_MAX);
		Out_Buffer[0] = 0x5;   // Calibration Register(05h)
		Out_Buffer[1] = (Regs->Calibration >> 8);
		Out_Buffer[2] = (Regs->Calibration & 0xFF);
		SC_INFO("Calibration Register(05h): %#x %#x", Out_Buffer[1],
			Out_Buffer[2]);
		I2C_WRITE(FD, INA226->I2C_Address, 3, Out_Buffer, Ret);
		if (Ret != 0) {
			(void) close(FD);
			return Ret;
		}
	}

	if (Regs->Set_Registers & INA226_Mask_Enable) {
		/* Write 'Mask/Enable' register */
		(void) memset(Out_Buffer, 0, STRLEN_MAX);
		Out_Buffer[0] = 0x6;   // Mask/Enable Register(06h)
		Out_Buffer[1] = (Regs->Mask_Enable >> 8);
		Out_Buffer[2] = (Regs->Mask_Enable & 0xFF);
		SC_INFO("Mask/Enable Register(06h): %#x %#x", Out_Buffer[1],
			Out_Buffer[2]);
		I2C_WRITE(FD, INA226->I2C_Address, 3, Out_Buffer, Ret);
		if (Ret != 0) {
			(void) close(FD);
			return Ret;
		}
	}

	if (Regs->Set_Registers & INA226_Alert_Limit) {
		/* Write 'Alert Limit' register */
		(void) memset(Out_Buffer, 0, STRLEN_MAX);
		Out_Buffer[0] = 0x7;   // Alert Limit Register(07h)
		Out_Buffer[1] = (Regs->Alert_Limit >> 8);
		Out_Buffer[2] = (Regs->Alert_Limit & 0xFF);
		SC_INFO("Alert Limit Register(07h): %#x %#x", Out_Buffer[1],
			Out_Buffer[2]);
		I2C_WRITE(FD, INA226->I2C_Address, 3, Out_Buffer, Ret);
		if (Ret != 0) {
			(void) close(FD);
			return Ret;
		}
	}

	(void) close(FD);
	return 0;
}

int
Get_Power(INA226_t *INA226, int Mode, float *Voltage, float *Current, float *Power)
{
	int FD;
	INA226_Regs_t Regs;
	char Out_Buffer[STRLEN_MAX];
	float Current_LSB;
	float Calibration;
	int Ret = 0;

	if (Mode != 0 && Mode != 1) {
		SC_ERR("invalid mode for getting power");
		return -1;
	}

	if (Mode == 0) {
		FD = open(INA226->I2C_Bus, O_RDWR);
		if (FD < 0) {
			SC_ERR("unable to access I2C bus %s: %m", INA226->I2C_Bus);
			return -1;
		}

		/*
		 * The per bit value for current is determined by
		 * following equation.  The 'Maximum Expected Current'
		 * unit is in Amps:
		 * 	Current_LSB = Maximum Expected Current / 2^15
		 * The unit of 'INA226->Maximum_Current' is in milli-Amps.
		 */
		Current_LSB = (float)INA226->Maximum_Current / (32768 * 1000);

		/*
		 * The value of Calibration register is determined by:
		 * 	Calibration = 0.00512 / (Current_LSB * R shunt)
		 * The unit of 'INA226->Shunt_Resistor' is in micro-Ohms,
		 * and the unit of 'R shunt' is in Ohms.
		 */
		Calibration = (0.00512 * 1000000) /
			      (Current_LSB * INA226->Shunt_Resistor);

		/* Prevent the overflow of Calibration register[14:0] */
		if (Calibration > 32767) {
			Calibration = 32767;
		}

		/* Write 'Calibration' register */
		(void) memset(Out_Buffer, 0, STRLEN_MAX);
		Out_Buffer[0] = 0x5;   // Calibration Register(05h)
		Out_Buffer[1] = ((unsigned short)Calibration >> 8);
		Out_Buffer[2] = ((unsigned short)Calibration & 0xFF);
		SC_INFO("Calibration Register(05h): %#x %#x", Out_Buffer[1],
			 Out_Buffer[2]);
		I2C_WRITE(FD, INA226->I2C_Address, 3, Out_Buffer, Ret);
		if (Ret != 0) {
			(void) close(FD);
			return Ret;
		}

		(void) close(FD);
	}

	if (Read_INA226(INA226, &Regs) != 0) {
		SC_ERR("failed to read INA226 registers");
		return -1;
	}

	if (Regs.Calibration == 0) {
		SC_ERR("invalid calibration register value of 0");
		return -1;
	}

	Current_LSB = (0.00512 * 1000000) /
		      (float)(Regs.Calibration * INA226->Shunt_Resistor);
	SC_INFO("Calculating Current_LSB = %f", Current_LSB);

	/* if Current is negative, use its absolute value */
	*Current = (float)Regs.Current;
	if (*Current > 0x7FFF) {
		*Current -= 0x10000;
		*Current = abs(*Current);
	}

	SC_INFO("Current before LSB scaling %f", *Current);
	*Current = ((*Current) * Current_LSB);
	SC_INFO("Current before phase multiplier scaling %f", *Current);
	*Current *= INA226->Phase_Multiplier;

	*Voltage = (float)Regs.Bus_Voltage;
	SC_INFO("Voltage before LSB scaling %f", *Voltage);
	*Voltage *= 1.25;       // 1.25 mV per bit
	*Voltage /= 1000;

	/* The power LSB has a fixed ratio to the Current_LSB of 25 */
	*Power = ((float)Regs.Power * Current_LSB * 25);
	SC_INFO("Power before phase multiplier scaling %f", *Power);
	*Power *= INA226->Phase_Multiplier;

	return 0;
}

/*
 * Power Operations
 */
int Power_Ops(void)
{
	int Target_Index = -1;
	INA226s_t *INA226s;
	INA226_t *INA226;
	INA226_Regs_t Regs = { 0 };
	unsigned long int Value;
	char *Next_Token;
	float Voltage;
	float Current;
	float Power;

	INA226s = Plat_Devs->INA226s;
	if (INA226s == NULL) {
		SC_ERR("power operation is not supported");
		return -1;
	}

	if (Command.CmdId == LISTPOWER) {
		for (int i = 0; i < INA226s->Numbers; i++) {
			SC_PRINT("%s", INA226s->INA226[i].Name);
		}

		return 0;
	}

	/* Validate the power target */
	if (T_Flag == 0) {
		SC_ERR("no power target");
		return -1;
	}

	for (int i = 0; i < INA226s->Numbers; i++) {
		if (strcmp(Target_Arg, (char *)INA226s->INA226[i].Name) == 0) {
			Target_Index = i;
			INA226 = &INA226s->INA226[Target_Index];
			break;
		}
	}

	if (Target_Index == -1) {
		SC_ERR("invalid power target");
		return -1;
	}

	switch (Command.CmdId) {
	case GETPOWER:
		if (Get_Power(INA226, 0, &Voltage, &Current, &Power) != 0) {
			SC_ERR("failed to get power");
			return -1;
		}

		SC_PRINT("Voltage(V):\t%.4f", Voltage);
		SC_PRINT("Current(A):\t%.4f", Current);
		SC_PRINT("Power(W):\t%.4f", Power);
		break;

	case GETCALPOWER:
		if (Get_Power(INA226, 1, &Voltage, &Current, &Power) != 0) {
			SC_ERR("failed to get power");
			return -1;
		}

		SC_PRINT("Voltage(V):\t%.4f", Voltage);
		SC_PRINT("Current(A):\t%.4f", Current);
		SC_PRINT("Power(W):\t%.4f", Power);
		break;

	case GETINA226:
		if (Read_INA226(INA226, &Regs) != 0) {
			SC_ERR("failed to read INA226");
			return -1;
		}

		SC_PRINT("Configuration:\t%#x", Regs.Configuration);
		SC_PRINT("Shunt Voltage:\t%#x", Regs.Shunt_Voltage);
		SC_PRINT("Bus Voltage:\t%#x", Regs.Bus_Voltage);
		SC_PRINT("Power:\t%#x", Regs.Power);
		SC_PRINT("Current:\t%#x", Regs.Current);
		SC_PRINT("Calibration:\t%#x", Regs.Calibration);
		SC_PRINT("Mask/Enable:\t%#x", Regs.Mask_Enable);
		SC_PRINT("Alert Limit:\t%#x", Regs.Alert_Limit);
		SC_PRINT("Die ID:\t%#x", Regs.Die_ID);
		break;

	case SETINA226:
		if (V_Flag == 0) {
			SC_ERR("no INA226 value");
			return -1;
		}

		Next_Token = strtok(Value_Arg, " ");
		if (Next_Token == NULL) {
			SC_ERR("no value given for 'Configuration' register");
			return -1;
		}

		if ((Next_Token[0] != 'x') && (Next_Token[0] != 'X')) {
			Value = strtol(Next_Token, NULL, 16);
			if ((Value > 0xFFFF) || (Value == 0 &&
			    (strcmp(Next_Token, "0") != 0) &&
			    (strcmp(Next_Token, "0x0") != 0))) {
				SC_ERR("invalid value for 'Configuration' "
				       "register");
				return -1;
			}

			Regs.Configuration = Value;
			Regs.Set_Registers |= INA226_Configuration;
		}

		Next_Token = strtok(NULL, " ");
		if (Next_Token == NULL) {
			SC_ERR("no value given for 'Calibration' register");
			return -1;
		}

		if ((Next_Token[0] != 'x') && (Next_Token[0] != 'X')) {
			Value = strtol(Next_Token, NULL, 16);
			if ((Value > 0xFFFF) || (Value == 0 &&
			    (strcmp(Next_Token, "0") != 0) &&
			    (strcmp(Next_Token, "0x0") != 0))) {
				SC_ERR("invalid value for 'Calibration' "
				       "register");
				return -1;
			}

			if (Value == 0) {
				SC_ERR("value 0 is invalid for 'Calibration' "
				       "register");
				return -1;
			}

			Regs.Calibration = Value;
			Regs.Set_Registers |= INA226_Calibration;
		}

		Next_Token = strtok(NULL, " ");
		if (Next_Token == NULL) {
			SC_ERR("no value given for 'Mask/Enable' register");
			return -1;
		}

		if ((Next_Token[0] != 'x') && (Next_Token[0] != 'X')) {
			Value = strtol(Next_Token, NULL, 16);
			if ((Value > 0xFFFF) || (Value == 0 &&
			    (strcmp(Next_Token, "0") != 0) &&
			    (strcmp(Next_Token, "0x0") != 0))) {
				SC_ERR("invalid value for 'Mask/Enable' "
				       "register");
				return -1;
			}

			Regs.Mask_Enable = Value;
			Regs.Set_Registers |= INA226_Mask_Enable;
		}

		Next_Token = strtok(NULL, "\n");
		if (Next_Token == NULL) {
			SC_ERR("no value given for 'Alert Limit' register");
			return -1;
		}

		if ((Next_Token[0] != 'x') && (Next_Token[0] != 'X')) {
			Value = strtol(Next_Token, NULL, 16);
			if ((Value > 0xFFFF) || (Value == 0 &&
			    (strcmp(Next_Token, "0") != 0) &&
			    (strcmp(Next_Token, "0x0") != 0))) {
				SC_ERR("invalid value for 'Alert Limit' "
				       "register: %lx", Value);
				return -1;
			}

			Regs.Alert_Limit = Value;
			Regs.Set_Registers |= INA226_Alert_Limit;
		}

		if (Write_INA226(INA226, &Regs) != 0) {
			SC_ERR("failed to write to INA226 registers");
			return -1;
		}

		break;

	default:
		SC_ERR("invalid power command");
		break;
	}

	return 0;
}

/*
 * Power Domain Operations
 */
int Power_Domain_Ops(void)
{
	int Target_Index = -1;
	Power_Domains_t *Power_Domains;
	Power_Domain_t *Power_Domain;
	INA226s_t *INA226s;
	INA226_t *INA226;
	float Voltage;
	float Current;
	float Power;
	float Total_Power = 0;

	Power_Domains = Plat_Devs->Power_Domains;
	if (Power_Domains == NULL) {
		SC_ERR("power domain operation is not supported");
		return -1;
	}

	if (Command.CmdId == LISTPOWERDOMAIN) {
		for (int i = 0; i < Power_Domains->Numbers; i++) {
			SC_PRINT("%s", Power_Domains->Power_Domain[i].Name);
		}

		return 0;
	}

	/* Validate the power domain target */
	if (T_Flag == 0) {
		SC_ERR("no power domain target");
		return -1;
	}

	for (int i = 0; i < Power_Domains->Numbers; i++) {
		if (strcmp(Target_Arg, (char *)Power_Domains->Power_Domain[i].Name) == 0) {
			Target_Index = i;
			Power_Domain = &Power_Domains->Power_Domain[Target_Index];
			break;
		}
	}

	if (Target_Index == -1) {
		SC_ERR("invalid power domain target");
		return -1;
	}

	switch (Command.CmdId) {
	case POWERDOMAIN:
		INA226s = Plat_Devs->INA226s;
		for (int i = 0; i < Power_Domain->Numbers; i++) {
			INA226 = &INA226s->INA226[Power_Domain->Rails[i]];
			if (Get_Power(INA226, 0, &Voltage, &Current, &Power) == -1) {
				SC_ERR("failed to get total power");
				return -1;
			}

			Total_Power += Power;
		}

		SC_PRINT("Power(W):\t%.4f", Total_Power);
		break;

	default:
		SC_ERR("invalid power domain command");
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
	Workarounds_t *Workarounds;
	unsigned long int Value;
	int Return = -1;

	Workarounds = Plat_Devs->Workarounds;
	if (Workarounds == NULL) {
		SC_ERR("workaround operation is not supported");
		return -1;
	}

	if (Command.CmdId == LISTWORKAROUND) {
		for (int i = 0; i < Workarounds->Numbers; i++) {
			SC_PRINT("%s", Workarounds->Workaround[i].Name);
		}

		return 0;
	}

	/* Validate the workaround target */
	if (T_Flag == 0) {
		SC_ERR("no workaround target");
		return -1;
	}

	for (int i = 0; i < Workarounds->Numbers; i++) {
		if (strcmp(Target_Arg, (char *)Workarounds->Workaround[i].Name) == 0) {
			Target_Index = i;
			break;
		}
	}

	if (Target_Index == -1) {
		SC_ERR("invalid workaround target");
		return -1;
	}

	/* Does the workaround need argument? */
	if (Workarounds->Workaround[Target_Index].Arg_Needed == 1 && V_Flag == 0) {
		SC_ERR("no workaround value");
		return -1;
	}

	if (V_Flag == 0) {
		Return = (*Workarounds->Workaround[Target_Index].Plat_Workaround_Op)(NULL);
	} else {
		Value = atol(Value_Arg);
		if (Value != 0 && Value != 1) {
			SC_ERR("invalid value");
			return -1;
		}

		Return = (*Workarounds->Workaround[Target_Index].Plat_Workaround_Op)(&Value);
	}

	if (Return == -1) {
		SC_ERR("failed to apply workaround");
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
	BITs_t *BITs;
	BIT_t *BIT;
	unsigned long int Value;
	int Level = 0;

	BITs = Plat_Devs->BITs;
	if (BITs == NULL) {
		SC_ERR("BIT operation is not supported");
		return -1;
	}

	if (Command.CmdId == LISTBIT) {
		for (int i = 0; i < BITs->Numbers; i++) {
			if (BITs->BIT[i].Manual) {
				SC_PRINT("%s - Manual Test(%d)", BITs->BIT[i].Name,
					 BITs->BIT[i].Levels);
			} else {
				SC_PRINT("%s", BITs->BIT[i].Name);
			}
		}

		return 0;
	}

	/* Validate the BIT target */
	if (T_Flag == 0) {
		SC_ERR("no BIT target");
		return -1;
	}

	for (int i = 0; i < BITs->Numbers; i++) {
		if (strcmp(Target_Arg, (char *)BITs->BIT[i].Name) == 0) {
			Target_Index = i;
			BIT = &BITs->BIT[Target_Index];
			break;
		}
	}

	if (Target_Index == -1) {
		SC_ERR("invalid BIT target");
		return -1;
	}

	if (!BIT->Manual || BIT->Levels == 1) {
		return BIT->Level[0].Plat_BIT_Op(BIT, &Level);
	}

	if (V_Flag == 0) {
		SC_ERR("no value is provided for multi-level BIT");
		return -1;
	}

	Value = strtol(Value_Arg, NULL, 16);
	if (Value == 0 || Value > BIT->Levels) {
		SC_ERR("invalid value for multi-level BIT");
		return -1;
	}

	Level = Value - 1;
	return BIT->Level[Level].Plat_BIT_Op(BIT, &Level);
}

int SDRAM_Size[] = { 0, 512, 1, 2, 4, 8, 16 };

/*
 * DDR Operations
 */
int DDR_Ops(void)
{
	int Target_Index = -1;
	DIMMs_t	*DIMMs;
	DIMM_t	*DIMM;
	int FD;
	char In_Buffer[STRLEN_MAX] = { 0 };
	char Out_Buffer[STRLEN_MAX] = { 0 };
	short Temp;
	int DRAM_Size_Index;
	int Ret = 0;

	DIMMs = Plat_Devs->DIMMs;
	if (DIMMs == NULL) {
		SC_ERR("ddr operation is not supported");
		return -1;
	}

	if (Command.CmdId == LISTDDR) {
		for (int i = 0; i < DIMMs->Numbers; i++) {
			SC_PRINT("%s", DIMMs->DIMM[i].Name);
		}

		return 0;
	}

	if (T_Flag == 0) {
		SC_ERR("no target is provided for getddr command");
		return -1;
	}

	for (int i = 0; i < DIMMs->Numbers; i++) {
		if (strcmp(Target_Arg, (char *)DIMMs->DIMM[i].Name) == 0) {
			Target_Index = i;
			DIMM = &DIMMs->DIMM[Target_Index];
			break;
		}
	}

	if (Target_Index == -1) {
		SC_ERR("invalid ddr target");
		return -1;
	}

	if (V_Flag == 0) {
		SC_ERR("no value is provided for getddr command");
		return -1;
	}

	FD = open(DIMM->I2C_Bus, O_RDWR);
	if (FD < 0) {
		SC_ERR("failed to open I2C bus %s: %m", DIMM->I2C_Bus);
		return -1;
	}

	if (strcmp(Value_Arg, "temp") == 0) {
		/*
		 * From SE98A datasheet:
		 *	Temperature register (address 0x5, 16-bit value)
		 *	Byte 0: XXXS_TTTT
		 *	Byte 1: tttt_tttt
		 *
		 *	STTT_Tttt_tttt_t000
		 *
		 * Upper 3-bit of Byte 1 is Status bits and is not involved in
		 * temperature value.  Shift-left the 16-bit value by 3 to get
		 * the temperature value, and then divide the value by (2 ^ 4 = 16)
		 * to adjust for the shift-left.  Resolution of each bit is 0.125 C.
		 */
		Out_Buffer[0] = 0x5;
		I2C_READ(FD, DIMM->I2C_Address_Thermal, 2, Out_Buffer, In_Buffer, Ret);
		if (Ret != 0) {
			(void) close(FD);
			return Ret;
		}

		SC_INFO("Temperature Register: %#x %#x", In_Buffer[0], In_Buffer[1]);
		Temp = ((In_Buffer[0] << 8) | In_Buffer[1]);
		Temp <<= 3;
		Temp /= 16;
		SC_PRINT("Temperature(C):\t%.2f", ((float)Temp) * 0.125);

	} else if (strcmp(Value_Arg, "spd") == 0) {
		Out_Buffer[0] = 0x0;
		I2C_READ(FD, DIMM->I2C_Address_SPD, 0xF, Out_Buffer, In_Buffer, Ret);
		if (Ret < 0) {
			(void) close(FD);
			return Ret;
		}

		/*
		 * DDR Serial Presence Detect (SPD) specification for DDR4:
		 *
		 * Byte 0x2 - DRAM Type = (0xC: DDR4 SDRAM)
		 * Byte 0x4 - DRAM Size (lower nibble) =
		 *			(0: 0MB, 1: 512MB, 2: 1GB, 3: 2GB, 4: 4GB,
		 *			 5: 8GB, 6: 16GB)
		 * Byte 0xE - Thermal Sensor = (0x0: No, 0x80: Yes)
		 */
		SC_INFO("DRAM Type: %#x", In_Buffer[2]);
		SC_PRINT("DDR4 SDRAM?\t%s", ((In_Buffer[2] == 0xC) ? "Yes" : "No"));

		SC_INFO("DRAM Size: %#x", In_Buffer[4]);
		DRAM_Size_Index = In_Buffer[4] & 0xF;
		SC_PRINT("Size(%cB):\t%d", ((DRAM_Size_Index >= 2) ? 'G' : 'M'),
			 SDRAM_Size[DRAM_Size_Index]);

		SC_INFO("Thermal Sensor: %#x", In_Buffer[0xE]);
		SC_PRINT("Temp. Sensor?\t%s", ((In_Buffer[0xE] & 0x80) ? "Yes" : "No"));

	} else {
		SC_ERR("%s is not a valid value", Value_Arg);
		Ret = -1;
	}

	(void) close(FD);
	return Ret;
}

static int GPIO_Get_All(void)
{
	FILE *FP;
	int State;
	char Buffer[SYSCMD_MAX];
	char Label[STRLEN_MAX];
	char Usage[STRLEN_MAX];

	(void) strcpy(Buffer, "/usr/bin/gpioinfo");
	FP = popen(Buffer, "r");
	if (FP == NULL) {
		SC_ERR("failed to run command %s: %m", Buffer);
		return -1;
	}

	while (fgets(Buffer, sizeof(Buffer), FP) != NULL) {
		if ((strstr(Buffer, "unnamed") != NULL) ||
		    (strstr(Buffer, "lines") != NULL)) {
			continue;
		}

		(void) strtok(Buffer, " :\"");
		(void) strtok(NULL, " :\"");
		(void) strcpy(Label, strtok(NULL, " :\""));
		(void) strcpy(Usage, strtok(NULL, " :\""));
		if (strcmp(Usage, "unused") != 0) {
			SC_PRINT("%s:\tbusy, used by %s", Label, Usage);
			continue;
		}

		if (Get_GPIO(Label, &State) != 0) {
			SC_ERR("failed to get GPIO line %s", Label);
			(void) pclose(FP);
			return -1;
		}

		SC_PRINT("%s:\t%d", Label, State);
	}

	(void) pclose(FP);
	return 0;
}

/*
 * GPIO Operations
 */
int GPIO_Ops(void)
{
	int Target_Index = -1;
	GPIOs_t *GPIOs;
	GPIO_t *GPIO = NULL;
	GPIO_Groups_t *GPIO_Groups;
	GPIO_Group_t *GPIO_Group = NULL;
	unsigned long int State;
	unsigned long int Value = 0;

	GPIOs = Plat_Devs->GPIOs;
	if (GPIOs == NULL) {
		SC_ERR("gpio operation is not supported");
		return -1;
	}

	GPIO_Groups = Plat_Devs->GPIO_Groups;
	if (Command.CmdId == LISTGPIO) {
		if (GPIO_Groups != NULL) {
			for (int i = 0; i < GPIO_Groups->Numbers; i++) {
				SC_PRINT("%s", GPIO_Groups->GPIO_Group[i].Name);
			}
		}

		for (int i = 0; i < GPIOs->Numbers; i++) {
			SC_PRINT("%s", GPIOs->GPIO[i].Display_Name);
		}

		return 0;
	}

	/* A target argument is required */
	if (T_Flag == 0) {
		SC_ERR("no gpio target");
		return -1;
	}

	/* Process '-c getgpio -t all' command here */
	if ((Command.CmdId == GETGPIO) && (strcmp(Target_Arg, "all") == 0)) {
		if (GPIO_Get_All() != 0) {
			SC_ERR("failed to get all GPIO lines");
			return -1;
		}

		return 0;
	}

	for (int i = 0; i < GPIOs->Numbers; i++) {
		if (!strncmp(GPIOs->GPIO[i].Display_Name, Target_Arg, STRLEN_MAX) ||
		    !strncmp(GPIOs->GPIO[i].Internal_Name, Target_Arg, STRLEN_MAX)) {
			Target_Index = i;
			GPIO = &GPIOs->GPIO[Target_Index];
			break;
		}
	}

	if (GPIO_Groups != NULL) {
		for (int i = 0; i < GPIO_Groups->Numbers; i++) {
			if (!strncmp(GPIO_Groups->GPIO_Group[i].Name, Target_Arg,
				     STRLEN_MAX)) {
				Target_Index = i;
				GPIO_Group = &GPIO_Groups->GPIO_Group[Target_Index];
				break;
			}
		}
	}

	if (Target_Index == -1) {
		SC_ERR("invalid gpio target");
		return -1;
	}

	switch (Command.CmdId) {
	case GETGPIO:
		if (GPIO_Group != NULL) {
			for (int i = 0; i < GPIO_Group->Numbers; i++) {
				if (Get_GPIO((char *)GPIO_Group->GPIO_Lines[i],
					     (int *)&State) != 0) {
					SC_ERR("failed to get GPIO line %s",
					       GPIO_Group->GPIO_Lines[i]);
					return -1;
				}

				SC_INFO("%s: %d", GPIO_Group->GPIO_Lines[i], (int)State);
				Value = ((Value << 1) | State);
			}

			SC_PRINT("%s:\t%#x", GPIO_Group->Name, (int)Value);
			break;
		}

		if (Get_GPIO((char *)GPIO->Internal_Name, (int *)&State) != 0) {
			SC_ERR("failed to get GPIO line %s", GPIO->Display_Name);
			return -1;
		}

		SC_PRINT("%s:\t%d", GPIO->Display_Name, (int)State);
		break;

	case SETGPIO:
		if (V_Flag == 0) {
			SC_ERR("no gpio value");
			return -1;
		}

		State = strtol(Value_Arg, NULL, 16);

		if (GPIO_Group != NULL) {
			if (GPIO_Group->Type == RW) {
				for (int i = 0; i < GPIO_Group->Numbers; i++) {
					Value = (State >> ((GPIO_Group->Numbers - 1) - i)) & 0x1;
					SC_INFO("Set %s to %d", GPIO_Group->GPIO_Lines[i],
							(int)Value);
					if (Set_GPIO((char *)GPIO_Group->GPIO_Lines[i],
								(int)Value) != 0) {
						SC_ERR("failed to set GPIO line %s",
								GPIO_Group->GPIO_Lines[i]);
						return -1;
					}
				}
			}
			else if (GPIO_Group->Type == OD) {
				for (int i = 0; i < GPIO_Group->Numbers; i++) {
					Value = (State >> ((GPIO_Group->Numbers - 1) - i)) & 0x1;
					SC_INFO("Set %s to %d", GPIO_Group->GPIO_Lines[i],
							(int)Value);
					if (Value == 0) {
						if (Set_GPIO((char *)GPIO_Group->GPIO_Lines[i],
									(int)Value) != 0) {
							SC_ERR("failed to set GPIO line %s",
									GPIO_Group->GPIO_Lines[i]);
							return -1;
						}
					} else {
						if (Get_GPIO((char *)GPIO_Group->GPIO_Lines[i],
									(int *)&Value) != 0) {
							SC_ERR("failed to set GPIO line %s",
									GPIO_Group->GPIO_Lines[i]);
							return -1;
						}
					}
				}
			}

			break;
		}

		if ((State != 0) && (State != 1)) {
			SC_ERR("invalid gpio value");
			return -1;
		}

		if (GPIO->Type == RW) {
			if (Set_GPIO((char *)GPIO->Internal_Name, (int)State) != 0) {
				SC_ERR("failed to set GPIO line %s", GPIO->Display_Name);
				return -1;
			}
		} else if (GPIO->Type == OD) {
			if (State == 0) {
				// Open drain.  Only set to 0
				if (Set_GPIO((char *)GPIO->Internal_Name, (int)State) != 0) {
					SC_ERR("failed to set GPIO line %s", GPIO->Display_Name);
					return -1;
				}
			} else {
				// Get will set the IO to an inputer when user sets to 1
				if (Get_GPIO((char *)GPIO->Internal_Name, (int *)&State) != 0) {
					SC_ERR("failed to get GPIO line %s", GPIO->Display_Name);
					return -1;
				}
			}
		}

		break;

	default:
		SC_ERR("invalid gpio command");
		return -1;
	}

	return 0;
}

int IO_Exp_Initialized(void)
{
	IO_Exp_t *IO_Exp;
	unsigned int Value = 0;

	IO_Exp = Plat_Devs->IO_Exp;
	if (IO_Exp == NULL) {
		SC_INFO("no ioexp device is present");
		return 0;
	}

	for (int i = 0; i < IO_Exp->Numbers; i++) {
		Value <<= 1;
		if ((IO_Exp->Directions[i] == 1) ||
		   (IO_Exp->Directions[i] == -1)) {
			Value |= 1;
		}
	}

	if (Access_IO_Exp(IO_Exp, 1, 0x6, &Value) != 0) {
		SC_ERR("failed to set direction");
		return -1;
	}

	Value = (~Value & ((1 << IO_Exp->Numbers) - 1));
	if (Access_IO_Exp(IO_Exp, 1, 0x2, &Value) != 0) {
		SC_ERR("failed to set output");
		return -1;
	}

	return 0;
}

/*
 * IO Expander Operations
 */
int IO_Exp_Ops(void)
{
	IO_Exp_t *IO_Exp;
	unsigned long int Value;

	IO_Exp = Plat_Devs->IO_Exp;
	if (IO_Exp == NULL) {
		SC_ERR("ioexp operation is not supported");
		return -1;
	}

	if (Command.CmdId == LISTIOEXP) {
		SC_PRINT("%s", IO_Exp->Name);
		return 0;
	}

	if (T_Flag == 0) {
		SC_ERR("no IO expander target");
		return -1;
	}

	if (strcmp(Target_Arg, IO_Exp->Name) != 0) {
		SC_ERR("invalid IO expander target");
		return -1;
	}

	switch (Command.CmdId) {
	case GETIOEXP:
		/* A value argument is required */
		if (V_Flag == 0) {
			SC_ERR("no IO expander value");
			return -1;
		}

		if (strcmp(Value_Arg, "all") == 0) {
			if (Access_IO_Exp(IO_Exp, 0, 0x0,
					  (unsigned int *)&Value) != 0) {
				SC_ERR("failed to read input");
				return -1;
			}

			SC_PRINT("Input GPIO:\t%#x", (unsigned short) Value);

			if (Access_IO_Exp(IO_Exp, 0, 0x2,
					  (unsigned int *)&Value) != 0) {
				SC_ERR("failed to read output");
				return -1;
			}

			SC_PRINT("Output GPIO:\t%#x", (unsigned short) Value);

			if (Access_IO_Exp(IO_Exp, 0, 0x6,
					  (unsigned int *)&Value) != 0) {
				SC_ERR("failed to read direction");
				return -1;
			}

			SC_PRINT("Direction:\t%#x", (unsigned short) Value);

		} else if (strcmp(Value_Arg, "input") == 0) {
			if (Access_IO_Exp(IO_Exp, 0, 0x0,
					  (unsigned int *)&Value) != 0) {
				SC_ERR("failed to read input");
				return -1;
			}

			for (int i = 0; i < IO_Exp->Numbers; i++) {
				if (IO_Exp->Directions[i] == 1) {
					SC_PRINT("%s:\t%lu", IO_Exp->Labels[i],
					    ((Value >> (IO_Exp->Numbers - i - 1)) & 1));
				}
			}

		} else if (strcmp(Value_Arg, "output") == 0) {
			if (Access_IO_Exp(IO_Exp, 0, 0x2,
					  (unsigned int *)&Value) != 0) {
				SC_ERR("failed to read output");
				return -1;
			}

			for (int i = 0; i < IO_Exp->Numbers; i++) {
				if (IO_Exp->Directions[i] == 0) {
					SC_PRINT("%s:\t%lu", IO_Exp->Labels[i],
					    ((Value >> (IO_Exp->Numbers - i - 1)) & 1));
				}
			}

		} else {
			SC_ERR("invalid getioexp value");
			return -1;
		}

		break;

	case SETDIRIOEXP:
		/* Validate the value argument */
		if (V_Flag == 0) {
			SC_ERR("no IO expander value");
			return -1;
		}

		Value = strtol(Value_Arg, NULL, 16);
		if (Access_IO_Exp(IO_Exp, 1, 0x6, (unsigned int *)&Value) != 0) {
			SC_ERR("failed to set direction");
			return -1;
		}

		break;

	case SETOUTIOEXP:
		/* Validate the value argument */
		if (V_Flag == 0) {
			SC_ERR("no IO expander value");
			return -1;
		}

		Value = strtol(Value_Arg, NULL, 16);
		if (Access_IO_Exp(IO_Exp, 1, 0x2, (unsigned int *)&Value) != 0) {
			SC_ERR("failed to set output");
			return -1;
		}

		break;

	case RESTOREIOEXP:
		return IO_Exp_Initialized();
		break;

	default:
		SC_ERR("invalid IO expander command");
		return -1;
	}

	return 0;
}

int SFP_List(void)
{
	SFPs_t *SFPs;
	SFP_t *SFP;
	int FD;
	char Buffer[STRLEN_MAX];

	SFPs = Plat_Devs->SFPs;
	for (int i = 0; i < SFPs->Numbers; i++) {
		SFP = &SFPs->SFP[i];
		if (SFP->Type == qsfp) {
			SC_PRINT("%s - Connection unknown", SFP->Name);
			continue;
		}

		if (QSFP_ModuleSelect(SFP, 1) != 0) {
			return -1;
		}

		FD = open(SFP->I2C_Bus, O_RDWR);
		if (FD < 0) {
			SC_ERR("failed to access I2C bus %s: %m", SFP->I2C_Bus);
			(void) QSFP_ModuleSelect(SFP, 0);
			return -1;
		}

		if (ioctl(FD, I2C_SLAVE_FORCE, SFP->I2C_Address) < 0) {
			SC_ERR("failed to configure I2C bus for access to "
			       "device address %#x: %m", SFP->I2C_Address);
			(void) QSFP_ModuleSelect(SFP, 0);
			(void) close(FD);
			return -1;
		}

		/*
		 * If the read operation fails, it indicates that there is
		 * no SFP device plugged into the connector referenced by
		 * the I2C device address.
		 */
		if (read(FD, Buffer, 1) != 1) {
			SC_PRINT("%s - Not connected", SFP->Name);
			(void) QSFP_ModuleSelect(SFP, 0);
			(void) close(FD);
			continue;
		}

		SC_PRINT("%s", SFP->Name);
		(void) QSFP_ModuleSelect(SFP, 0);
		(void) close(FD);
	}

	return 0;
}

/*
 * SFP Operations
 */
int SFP_Ops(void)
{
	int Target_Index = -1;
	unsigned long int Value;
	SFPs_t *SFPs;
	SFP_t *SFP;
	int FD;
	char In_Buffer[STRLEN_MAX];
	char Out_Buffer[STRLEN_MAX];
	int I2C_Address;
	int Ret = 0;

	SFPs = Plat_Devs->SFPs;
	if (SFPs == NULL) {
		SC_ERR("SFP operation is not supported");
		return -1;
	}

	if (Command.CmdId == LISTSFP) {
		return SFP_List();
	}

	/* Validate the SFP target */
	if (T_Flag == 0) {
		SC_ERR("no SFP target");
		return -1;
	}

	for (int i = 0; i < SFPs->Numbers; i++) {
		if (strcmp(Target_Arg, (char *)SFPs->SFP[i].Name) == 0) {
			Target_Index = i;
			SFP = &SFPs->SFP[Target_Index];
			break;
		}
	}

	if (Target_Index == -1) {
		SC_ERR("invalid SFP target");
		return -1;
	}

	if (QSFP_ModuleSelect(SFP, 1) != 0) {
		return -1;
	}

	FD = open(SFP->I2C_Bus, O_RDWR);
	if (FD < 0) {
		SC_ERR("unable to access I2C bus %s: %m", SFP->I2C_Bus);
		Ret = -1;
		goto Out;
	}

	switch (Command.CmdId) {
	case GETSFP:
		(void) memset(Out_Buffer, 0, STRLEN_MAX);
		(void) memset(In_Buffer, 0, STRLEN_MAX);
		if (SFP->Type == sfp) {
			Out_Buffer[0] = 0x14;	// 0x14-0x23: Vendor Name
		} else if (SFP->Type == qsfp) {
			Out_Buffer[0] = 0x94;	// 0x94-0xA3: Vendor Name
		} else if (SFP->Type == sfpdd || SFP->Type == qsfpdd ||
			   SFP->Type == osfp) {
			Out_Buffer[0] = 0x81;	// 0x81-0x90: Vendor Name
		} else {
			SC_ERR("Unsupported SFP");
			Ret = -1;
			goto Out;
		}

		I2C_READ(FD, SFP->I2C_Address, 16, Out_Buffer, In_Buffer, Ret);
		if (Ret != 0) {
			goto Out;
		}

		SC_PRINT("Manufacturer (%#x-%#x):\t%s", Out_Buffer[0],
			 (Out_Buffer[0] + 15), In_Buffer);

		(void) memset(Out_Buffer, 0, STRLEN_MAX);
		(void) memset(In_Buffer, 0, STRLEN_MAX);
		if (SFP->Type == sfp) {
			Out_Buffer[0] = 0x28;	// 0x28-0x37: Part Number
		} else if (SFP->Type == qsfp) {
			Out_Buffer[0] = 0xA8;	// 0xA8-0xB7: Part Number
		} else if (SFP->Type == sfpdd || SFP->Type == qsfpdd ||
			   SFP->Type == osfp) {
			Out_Buffer[0] = 0x94;	// 0x94-0xA3: Part Number
		} else {
			SC_ERR("Unsupported SFP");
			Ret = -1;
			goto Out;
		}

		I2C_READ(FD, SFP->I2C_Address, 16, Out_Buffer, In_Buffer, Ret);
		if (Ret != 0) {
			goto Out;
		}

		SC_PRINT("Part Number (%#x-%#x):\t%s", Out_Buffer[0],
			 (Out_Buffer[0] + 15), In_Buffer);

		(void) memset(Out_Buffer, 0, STRLEN_MAX);
		(void) memset(In_Buffer, 0, STRLEN_MAX);
		if (SFP->Type == sfp) {
			Out_Buffer[0] = 0x44;	// 0x44-0x53: Serial Number
		} else if (SFP->Type == qsfp) {
			Out_Buffer[0] = 0xC4;	// 0xC4-0xD3: Serial Number
		} else if (SFP->Type == sfpdd || SFP->Type == qsfpdd ||
			   SFP->Type == osfp) {
			Out_Buffer[0] = 0xA6;	// 0xA6-0xB5: Serial Number
		} else {
			SC_ERR("Unsupported SFP");
			Ret = -1;
			goto Out;
		}

		I2C_READ(FD, SFP->I2C_Address, 16, Out_Buffer, In_Buffer, Ret);
		if (Ret != 0) {
			goto Out;
		}

		SC_PRINT("Serial Number (%#x-%#x):\t%s", Out_Buffer[0],
			 (Out_Buffer[0] + 15), In_Buffer);

		(void) memset(Out_Buffer, 0, STRLEN_MAX);
		(void) memset(In_Buffer, 0, STRLEN_MAX);
		I2C_Address = SFP->I2C_Address;
		if (SFP->Type == sfp) {
			Out_Buffer[0] = 0x60;	// 0x60-0x61: Temperature
			I2C_Address = SFP->I2C_Address + 1;
		} else if (SFP->Type == qsfp) {
			Out_Buffer[0] = 0x16;	// 0x16-0x17: Temperature
		} else if (SFP->Type == sfpdd || SFP->Type == qsfpdd ||
			   SFP->Type == osfp) {
			Out_Buffer[0] = 0xE;	// 0xE-0xF: Temperature
		} else {
			SC_ERR("Unsupported SFP");
			Ret = -1;
			goto Out;
		}

		I2C_READ(FD, I2C_Address, 2, Out_Buffer, In_Buffer, Ret);
		if (Ret != 0) {
			goto Out;
		}

		Value = (In_Buffer[0] << 8) | In_Buffer[1];
		SC_INFO("Temperature (%#x-%#x): %#lx", Out_Buffer[0],
			(Out_Buffer[0] + 1), Value);
		Value = (Value & 0x7FFF) - (Value & 0x8000);
		/* Each bit of low byte is equivalent to 1/256 celsius */
		SC_PRINT("Temperature(C) (%#x-%#x):\t%.2f", Out_Buffer[0],
			 (Out_Buffer[0] + 1), ((float)Value / 256));

		(void) memset(Out_Buffer, 0, STRLEN_MAX);
		(void) memset(In_Buffer, 0, STRLEN_MAX);
		if (SFP->Type == sfp) {
			Out_Buffer[0] = 0x62;	// 0x62-0x63: Supply Voltage
		} else if (SFP->Type == qsfp) {
			Out_Buffer[0] = 0x1A;	// 0x1A-0x1B: Supply Voltage
		} else if (SFP->Type == sfpdd || SFP->Type == qsfpdd ||
			   SFP->Type == osfp) {
			Out_Buffer[0] = 0x10;	// 0x10-0x11: Supply Voltage
		} else {
			SC_ERR("Unsupported SFP");
			Ret = -1;
			goto Out;
		}

		I2C_READ(FD, I2C_Address, 2, Out_Buffer, In_Buffer, Ret);
		if (Ret != 0) {
			goto Out;
		}

		Value = (In_Buffer[0] << 8) | In_Buffer[1];
		SC_INFO("Supply Voltage (%#x-%#x): %#lx", Out_Buffer[0],
			(Out_Buffer[0] + 1), Value);
		/* Each bit is 100 uV */
		SC_PRINT("Supply Voltage(V) (%#x-%#x):\t%.2f", Out_Buffer[0],
			 (Out_Buffer[0] + 1), ((float)Value * 0.0001));

		if (SFP->Type == sfp) {
			(void) memset(Out_Buffer, 0, STRLEN_MAX);
			(void) memset(In_Buffer, 0, STRLEN_MAX);
			Out_Buffer[0] = 0x70;	// 0x70-0x71: Alarm
			I2C_READ(FD, I2C_Address, 2, Out_Buffer, In_Buffer, Ret);
			if (Ret != 0) {
				goto Out;
			}

			SC_PRINT("Alarm (0x70-0x71):\t%#x", (In_Buffer[0] << 8) |
				 In_Buffer[1]);

		} else if (SFP->Type == sfpdd) {
			(void) memset(Out_Buffer, 0, STRLEN_MAX);
			(void) memset(In_Buffer, 0, STRLEN_MAX);
			Out_Buffer[0] = 0x5;	// 0x5-0xD: Alarms
			I2C_READ(FD, I2C_Address, 9, Out_Buffer, In_Buffer, Ret);
			if (Ret != 0) {
				goto Out;
			}

			SC_PRINT("Alarms (0x5-0xd):\t%#x %#x %#x %#x %#x",
				 ((In_Buffer[0] << 8) | In_Buffer[1]),
				 ((In_Buffer[2] << 8) | In_Buffer[3]),
				 ((In_Buffer[4] << 8) | In_Buffer[5]),
				 ((In_Buffer[6] << 8) | In_Buffer[7]), In_Buffer[8]);

		} else if (SFP->Type == qsfp) {
			(void) memset(Out_Buffer, 0, STRLEN_MAX);
			(void) memset(In_Buffer, 0, STRLEN_MAX);
			Out_Buffer[0] = 0x3;	// 0x3-0x4: Alarms
			I2C_READ(FD, I2C_Address, 2, Out_Buffer, In_Buffer, Ret);
			if (Ret != 0) {
				goto Out;
			}

			SC_PRINT("Alarms (0x3-0x4):\t%#x", (In_Buffer[0] << 8) |
				 In_Buffer[1]);

			(void) memset(Out_Buffer, 0, STRLEN_MAX);
			(void) memset(In_Buffer, 0, STRLEN_MAX);
			Out_Buffer[0] = 0x6;	// 0x6-0x7: Alarms
			I2C_READ(FD, I2C_Address, 2, Out_Buffer, In_Buffer, Ret);
			if (Ret != 0) {
				goto Out;
			}

			SC_PRINT("Alarms (0x6-0x7):\t%#x", (In_Buffer[0] << 8) |
				 In_Buffer[1]);

			(void) memset(Out_Buffer, 0, STRLEN_MAX);
			(void) memset(In_Buffer, 0, STRLEN_MAX);
			Out_Buffer[0] = 0x9;	// 0x9-0xC: Alarms
			I2C_READ(FD, I2C_Address, 4, Out_Buffer, In_Buffer, Ret);
			if (Ret != 0) {
				goto Out;
			}

			SC_PRINT("Alarms (0x9-0xc):\t%#x %#x", ((In_Buffer[0] << 8) |
				 In_Buffer[1]), ((In_Buffer[2] << 8) |
				 In_Buffer[3]));

		} else if (SFP->Type == qsfpdd || SFP->Type == osfp) {
			(void) memset(Out_Buffer, 0, STRLEN_MAX);
			(void) memset(In_Buffer, 0, STRLEN_MAX);
			Out_Buffer[0] = 0x8;	// 0x8-0xB: Alarms
			I2C_READ(FD, I2C_Address, 4, Out_Buffer, In_Buffer, Ret);
			if (Ret != 0) {
				goto Out;
			}

			SC_PRINT("Alarms (0x8-0xb):\t%#x %#x", ((In_Buffer[0] << 8) |
				 In_Buffer[1]), ((In_Buffer[2] << 8) |
				 In_Buffer[3]));

		} else {
			SC_ERR("Unsupported SFP");
			Ret = -1;
			goto Out;
		}

		break;

	default:
		SC_ERR("invalid SFP command");
		Ret = -1;
	}

Out:
	(void) QSFP_ModuleSelect(SFP, 0);
	(void) close(FD);
	return Ret;
}

int EBM_List(void)
{
	Daughter_Card_t *Daughter_Card;
	int FD;
	char Buffer[STRLEN_MAX];

	Daughter_Card = Plat_Devs->Daughter_Card;
	FD = open(Daughter_Card->I2C_Bus, O_RDWR);
	if (FD < 0) {
		SC_ERR("failed to access I2C bus %s: %m", Daughter_Card->I2C_Bus);
		return -1;
	}

	if (ioctl(FD, I2C_SLAVE_FORCE, Daughter_Card->I2C_Address) < 0) {
		SC_ERR("failed to configure I2C bus for access to "
		       "device address %#x: %m", Daughter_Card->I2C_Address);
		return -1;
	}

	/*
	 * If the read operation fails, it indicates that there is
	 * no daughter card plugged into the motherboard referenced by
	 * the I2C device address.
	 */
	if (read(FD, Buffer, 1) != 1) {
		SC_PRINT("%s - Not connected", Daughter_Card->Name);
		(void) close(FD);
		return 0;
	}

	SC_PRINT("%s", Daughter_Card->Name);
	return 0;
}

/*
 * EBM Operations
 */
int EBM_Ops(void)
{
	EEPROM_Targets Target;
	Daughter_Card_t *Daughter_Card;
	int FD;
	char In_Buffer[SYSCMD_MAX];
	char Out_Buffer[SYSCMD_MAX];
	int Ret = 0;

	Daughter_Card = Plat_Devs->Daughter_Card;
	if (Daughter_Card == NULL) {
		SC_ERR("EBM card operation is not supported");
		return -1;
	}

	if (Command.CmdId == LISTEBM) {
		return EBM_List();
	}

	/* Validate the EBM target */
	if (T_Flag == 0) {
		SC_ERR("no EBM target");
		return -1;
	}

	if (strcmp(Target_Arg, Daughter_Card->Name) != 0) {
		SC_ERR("invalid getEBM target");
		return -1;
	}

	if (V_Flag == 0) {
		SC_ERR("no value is provided for getEBM");
		return -1;
	}

	if (strcmp(Value_Arg, "all") == 0) {
		Target = EEPROM_ALL;
	} else if (strcmp(Value_Arg, "common") == 0) {
		Target = EEPROM_COMMON;
	} else if (strcmp(Value_Arg, "board") == 0) {
		Target = EEPROM_BOARD;
	} else if (strcmp(Value_Arg, "multirecord") == 0) {
		Target = EEPROM_MULTIRECORD;
	} else {
		SC_ERR("invalid getEBM value");
		return -1;
	}

	FD = open(Daughter_Card->I2C_Bus, O_RDWR);
	if (FD < 0) {
		SC_ERR("unable to access I2C bus %s: %m", Daughter_Card->I2C_Bus);
		return -1;
	}

	(void) memset(Out_Buffer, 0, SYSCMD_MAX);
	(void) memset(In_Buffer, 0, SYSCMD_MAX);
	Out_Buffer[0] = 0x0;
	I2C_READ(FD, Daughter_Card->I2C_Address, 256, Out_Buffer, In_Buffer, Ret);
	if (Ret != 0) {
		(void) close(FD);
		return Ret;
	}

	(void) close(FD);
	switch (Target) {
	case EEPROM_ALL:
		EEPROM_Print_All(In_Buffer, 256, 16);
		break;
	case EEPROM_COMMON:
		return EEPROM_Common(In_Buffer);
	case EEPROM_BOARD:
		return EEPROM_Board(In_Buffer, 0);
	case EEPROM_MULTIRECORD:
		return EEPROM_MultiRecord(In_Buffer, 0);
	default:
		SC_ERR("invalid EBM target");
		return -1;
	}

	return 0;
}

int FMC_List(void)
{
	FMCs_t *FMCs;
	FMC_t *FMC;
	int FD;
	char In_Buffer[SYSCMD_MAX];
	char Out_Buffer[SYSCMD_MAX];
	char Buffer[STRLEN_MAX];
	int Ret = 0;
	int Offset, Length;

	FMCs = Plat_Devs->FMCs;
	for (int i = 0; i < FMCs->Numbers; i++) {
		FMC = &FMCs->FMC[i];
		FD = open(FMC->I2C_Bus, O_RDWR);
		if (FD < 0) {
			SC_ERR("unable to access I2C bus %s: %m", FMC->I2C_Bus);
			return -1;
		}

		if (ioctl(FD, I2C_SLAVE_FORCE, FMC->I2C_Address) < 0) {
			SC_ERR("unable to access I2C device address %#x",
			       FMC->I2C_Address);
			(void) close(FD);
			return -1;
		}

		/*
		 * If write fails, it indicates that there is no FMC plugged into
		 * the connector referenced by the I2C device address.
		 */
		Out_Buffer[0] = 0x0;
		if (write(FD, Out_Buffer, 1) != 1) {
			SC_PRINT("%s - Not connected", FMC->Name);
			(void) close(FD);
			continue;
		}

		/*
		 * Since there is a FMC on this connector, read its Manufacturer
		 * and its Product Name.
		 */
		(void) memset(Out_Buffer, 0, SYSCMD_MAX);
		(void) memset(In_Buffer, 0, SYSCMD_MAX);
		Out_Buffer[0] = 0x0;    // EEPROM offset 0
		I2C_READ(FD, FMC->I2C_Address, 0xFF, Out_Buffer, In_Buffer, Ret);
		if (Ret != 0) {
			(void) close(FD);
			return -1;
		}

		(void) close(FD);
		Offset = 0xE;
		Length = (In_Buffer[Offset] & 0x3F);
		snprintf(Buffer, Length + 1, "%s", &In_Buffer[Offset + 1]);
		SC_INFO("%s - %s ", FMC->Name, Buffer);
		SC_PRINT_N("%s - %s ", FMC->Name, Buffer);
		Offset = Offset + Length + 1;
		Length = (In_Buffer[Offset] & 0x3F);
		snprintf(Buffer, Length + 1, "%s", &In_Buffer[Offset + 1]);
		SC_PRINT("%s", Buffer);
	}

	return 0;
}

/*
 * FMC Operations
 */
int FMC_Ops(void)
{
	int Target_Index = -1;
	FMCs_t *FMCs;
	FMC_t *FMC;
	EEPROM_Targets Area;
	int FD;
	char In_Buffer[SYSCMD_MAX];
	char Out_Buffer[SYSCMD_MAX];
	int Ret = 0;

	FMCs = Plat_Devs->FMCs;
	if (FMCs == NULL) {
		SC_ERR("FMC operation is not supported");
		return -1;
	}

	if (Command.CmdId == LISTFMC) {
		return FMC_List();
	}

	if (T_Flag == 0) {
		SC_ERR("no FMC target");
		return -1;
	}

	(void) strcpy(Out_Buffer, strtok(Target_Arg, " - "));
	for (int i = 0; i < FMCs->Numbers; i++) {
		if (strcmp(Out_Buffer, FMCs->FMC[i].Name) == 0) {
			Target_Index = i;
			FMC = &FMCs->FMC[Target_Index];
			break;
		}
	}

	if (Target_Index == -1) {
		SC_ERR("invalid FMC target");
		return -1;
	}

	if (V_Flag == 0) {
		SC_ERR("no FMC value");
		return -1;
	}

	if (strcmp(Value_Arg, "all") == 0) {
		Area = EEPROM_ALL;
	} else if (strcmp(Value_Arg, "common") == 0) {
		Area = EEPROM_COMMON;
	} else if (strcmp(Value_Arg, "board") == 0) {
		Area = EEPROM_BOARD;
	} else if (strcmp(Value_Arg, "multirecord") == 0) {
		Area = EEPROM_MULTIRECORD;
	} else {
		SC_ERR("invalid FMC value");
		return -1;
	}

	FD = open(FMC->I2C_Bus, O_RDWR);
	if (FD < 0) {
		SC_ERR("unable to access I2C bus %s: %m", FMC->I2C_Bus);
		return -1;
	}

	(void) memset(Out_Buffer, 0, SYSCMD_MAX);
	(void) memset(In_Buffer, 0, SYSCMD_MAX);
	Out_Buffer[0] = 0x0;
	I2C_READ(FD, FMC->I2C_Address, 256, Out_Buffer, In_Buffer, Ret);
	if (Ret != 0) {
		(void) close(FD);
		return Ret;
	}

	(void) close(FD);
	switch (Area) {
	case EEPROM_ALL:
		EEPROM_Print_All(In_Buffer, 256, 16);
		break;
	case EEPROM_COMMON:
		return EEPROM_Common(In_Buffer);
	case EEPROM_BOARD:
		return EEPROM_Board(In_Buffer, 0);
	case EEPROM_MULTIRECORD:
		return EEPROM_MultiRecord(In_Buffer, 0);
	default:
		SC_ERR("invalid FMC value");
		return -1;
	}

	return 0;
}

/*
 * If the PDI filename passed in PDI_File argument is the default
 * PDI (system_wrapper.pdi or es1_system_wrapper.pdi), validate
 * that it matches the silicon revision.  The Full_Path argument
 * returns the full path of PDI_File. 
 */
static int
Validate_PDI(const char *PDI_File, char *Full_Path)
{
	char Default_ES1_PDI[STRLEN_MAX];

	if (strcmp(PDI_File, DEFAULT_PDI) == 0 &&
	    strcmp(Silicon_Revision, "PROD") != 0) {
		SC_ERR("invalid default PDI");
		return -1;
	}

	(void) sprintf(Default_ES1_PDI, "es1_%s", DEFAULT_PDI);
	if (strcmp(PDI_File, Default_ES1_PDI) == 0 &&
	    strcmp(Silicon_Revision, "ES1") != 0) {
		SC_ERR("invalid default PDI");
		return -1;
	}

	if (Full_Path == NULL) {
		SC_ERR("storage Full_Path is not allocated");
		return -1;
	}

	(void) sprintf(Full_Path, "%s%s/%s", BIT_PATH, Board_Name, PDI_File);
	if (access(Full_Path, F_OK) == 0) {
		return 0;
	}

	(void) sprintf(Full_Path, "%s%s", CUSTOM_PDIS_PATH, PDI_File);
	if (access(Full_Path, F_OK) == 0) {
		return 0;
	}

	SC_ERR("could not locate PDI file %s", PDI_File);
	return -1;
}

int
PDI_Ops(void)
{
	char PDI_Path[SYSCMD_MAX], TCL_Path[SYSCMD_MAX];
	char Output[SYSCMD_MAX] = { 0 };

	switch (Command.CmdId) {
	case LOADPDI:
		if (T_Flag == 0) {
			SC_ERR("no target for PDI command");
			return -1;
		}

		if (Validate_PDI(Target_Arg, PDI_Path) != 0) {
			return -1;
		}

		if (Reset_Op() != 0) {
			return -1;
		}

		SC_INFO("Full PDI path: %s", PDI_Path);
		(void) sprintf(TCL_Path, "%s%s", SCRIPT_PATH, PDI_LOAD_TCL);
		if (XSDB_Op(TCL_Path, PDI_Path, Output, sizeof(Output)) != 0) {
			SC_ERR("failed to download PDI");
			return -1;
		}

		break;

	case SETBOOTPDI:
		if (T_Flag == 0) {
			SC_ERR("no target for PDI command");
			return -1;
		}

		if (Validate_PDI(Target_Arg, PDI_Path) != 0) {
			return -1;
		}

		(void) sprintf(Output, "echo '%s' > %s; sync", Target_Arg, PDIFILE);
		SC_INFO("Command: %s", Output);
		system(Output);
		break;

	case RESETBOOTPDI:
		/* Leave an empty PDIFILE */
		(void) sprintf(Output, "rm %s; touch %s; sync", PDIFILE, PDIFILE);
		SC_INFO("Command: %s", Output);
		system(Output);
		break;

	default:
		SC_ERR("invalid PDI command");
		return -1;
	}

	return 0;
}

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
	char Value[LSTRLEN_MAX];
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

	if (Check_Config_File("Vccaux_Workaround", Value, &Found) != 0) {
		return -1;
	}

	if (Found) {
		Workaround = atoi(Value);
	} else {
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
	/*
	 * Current workarounds:
	 *	vccaux: VCK190/VMK180 boards with ES1 part
	 */
	if ((strcmp(Board_Name, "VCK190\n") == 0) ||
	    (strcmp(Board_Name, "VMK180\n") == 0)) {
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
Boot_Set_Clocks(void)
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
Boot_Set_Voltages(void)
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

/* This routine loads any PDI that is set to be loaded at boot time */
int
Boot_Load_PDI(void)
{
	FILE *FP;
	char PDI_Path[SYSCMD_MAX], TCL_Path[SYSCMD_MAX];
	char Buffer[SYSCMD_MAX] = { 0 };

	/* If there is no PDIFILE, there is nothing to do */
	if (access(PDIFILE, F_OK) != 0) {
		return 0;
	}

	FP = fopen(PDIFILE, "r");
	if (FP == NULL) {
		SC_ERR("failed to read file %s: %m", PDIFILE);
		return -1;
	}

	/* If PDIFILE is empty, there is nothing to do */
	if (fgets(Buffer, sizeof(Buffer), FP) == NULL) {
		return 0;
	}

	/* Strip the new line character */
	(void) strtok(Buffer, "\n");
	SC_INFO("Load PDI file: %s", Buffer);
	if (Validate_PDI(Buffer, PDI_Path) != 0) {
		return -1;
	}

	SC_INFO("Full PDI path: %s", PDI_Path);
	(void) sprintf(TCL_Path, "%s%s", SCRIPT_PATH, PDI_LOAD_TCL);
	if (XSDB_Op(TCL_Path, PDI_Path, Buffer, sizeof(Buffer)) != 0) {
		SC_ERR("failed to download PDI");
		return -1;
	}

	return 0;
}

int
FMC_Autodetect_Vadj()
{
	char Value[LSTRLEN_MAX];
	int Found = 0;

	/*
	 * The FMC Autodetect Vadj task is enabled by default.  It can be
	 * disabled by adding 'FMC_Autodetect: 0' entry in CONFIGFILE.
	 */
	if (Check_Config_File("FMC_Autodetect", Value, &Found) != 0) {
		return -1;
	}

	if (Found && (atoi(Value) == 0)) {
		SC_INFO("FMC Autodetect Vadj is disabled");
		return 0;
	}

	if (FMCAutoVadj_Op() != 0) {
		SC_ERR("failed to autodetect FMC vadj");
		return -1;
	}

	return 0;
}
