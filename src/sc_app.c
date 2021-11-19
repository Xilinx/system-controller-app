/*
 * Copyright (c) 2020 - 2021 Xilinx, Inc.  All rights reserved.
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
#include <sys/utsname.h>
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
 * 1.11 - Add 'geteeprom' command to get the entire content of on-board's EEPROM.
 * 1.12 - Support for FPGA Mezzanine Cards (FMCs).
 * 1.13 - Add 'list' command for features that didn't have that option.
 * 1.14 - Add 'listfeature' command to print supported features of the board.
 * 1.15 - Add support for INA226 custom calibration.
 */
#define MAJOR	1
#define MINOR	15

#define LINUX_VERSION	"5.4.0"
#define BSP_VERSION	"2020_2"

extern Plat_Devs_t *Plat_Devs;
extern Plat_Ops_t *Plat_Ops;

int Parse_Options(int argc, char **argv);
int Create_Lockfile(void);
int Destroy_Lockfile(void);
int Version_Ops(void);
int BootMode_Ops(void);
int Reset_Ops(void);
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
int QSFP_Ops(void);
int EBM_Ops(void);
int FMC_Ops(void);
extern int Board_Identification(void);
extern int Access_Regulator(Voltage_t *, float *, int);
extern int Access_IO_Exp(IO_Exp_t *, int, int, unsigned int *);
extern int GPIO_Get(char *, int *);
extern int EEPROM_Common(char *);
extern int EEPROM_Board(char *, int);
extern int EEPROM_MultiRecord(char *);
extern int Get_IDT_8A34001(Clock_t *);
extern int Set_IDT_8A34001(Clock_t *, char *, int);
extern int Restore_IDT_8A34001(Clock_t *);

static char Usage[] = "\n\
sc_app -c <command> [-t <target> [-v <value>]]\n\n\
<command>:\n\
	version - version and compatibility information\n\
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
	setbootmode - set boot mode to <target>\n\
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
	listQSFP - list the plugged QSFP transceiver targets\n\
	getQSFP - get the transceiver information of <target> QSFP\n\
\n\
	listEBM - list the plugged EBM daughter card targets\n\
	getEBM - get the content of EEPROM on <target> EBM card for either <value>:\n\
		 'all', 'common', 'board', or 'multirecord'\n\
\n\
	listFMC - list the plugged FMCs\n\
	getFMC - get the content of EEPROM on <target> FMC for either <value>:\n\
		 'all', 'common', 'board', or 'multirecord'\n\
";

typedef enum {
	VERSION,
	RESET,
	LISTFEATURE,
	LISTEEPROM,
	GETEEPROM,
	LISTTEMP,
	GETTEMP,
	LISTBOOTMODE,
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
	LISTIOEXP,
	GETIOEXP,
	SETDIRIOEXP,
	SETOUTIOEXP,
	RESTOREIOEXP,
	LISTSFP,
	GETSFP,
	LISTQSFP,
	GETQSFP,
	LISTEBM,
	GETEBM,
	LISTFMC,
	GETFMC,
	COMMAND_MAX,
} CmdId_t;

typedef struct {
	CmdId_t	CmdId;	
	char CmdStr[STRLEN_MAX];
	int (*CmdOps)(void);
} Command_t;

static Command_t Commands[] = {
	{ .CmdId = VERSION, .CmdStr = "version", .CmdOps = Version_Ops, },
	{ .CmdId = RESET, .CmdStr = "reset", .CmdOps = Reset_Ops, },
	{ .CmdId = LISTFEATURE, .CmdStr = "listfeature", .CmdOps = Feature_Ops, },
	{ .CmdId = LISTEEPROM, .CmdStr = "listeeprom", .CmdOps = EEPROM_Ops, },
	{ .CmdId = GETEEPROM, .CmdStr = "geteeprom", .CmdOps = EEPROM_Ops, },
	{ .CmdId = LISTTEMP, .CmdStr = "listtemp", .CmdOps = Temperature_Ops, },
	{ .CmdId = GETTEMP, .CmdStr = "gettemp", .CmdOps = Temperature_Ops, },
	{ .CmdId = LISTBOOTMODE, .CmdStr = "listbootmode", .CmdOps = BootMode_Ops, },
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
	{ .CmdId = LISTIOEXP, .CmdStr = "listioexp", .CmdOps = IO_Exp_Ops, },
	{ .CmdId = GETIOEXP, .CmdStr = "getioexp", .CmdOps = IO_Exp_Ops, },
	{ .CmdId = SETDIRIOEXP, .CmdStr = "setdirioexp", .CmdOps = IO_Exp_Ops, },
	{ .CmdId = SETOUTIOEXP, .CmdStr = "setoutioexp", .CmdOps = IO_Exp_Ops, },
	{ .CmdId = RESTOREIOEXP, .CmdStr = "restoreioexp", .CmdOps = IO_Exp_Ops, },
	{ .CmdId = LISTSFP, .CmdStr = "listSFP", .CmdOps = SFP_Ops, },
	{ .CmdId = GETSFP, .CmdStr = "getSFP", .CmdOps = SFP_Ops, },
	{ .CmdId = LISTQSFP, .CmdStr = "listQSFP", .CmdOps = QSFP_Ops, },
	{ .CmdId = GETQSFP, .CmdStr = "getQSFP", .CmdOps = QSFP_Ops, },
	{ .CmdId = LISTEBM, .CmdStr = "listEBM", .CmdOps = EBM_Ops, },
	{ .CmdId = GETEBM, .CmdStr = "getEBM", .CmdOps = EBM_Ops, },
	{ .CmdId = LISTFMC, .CmdStr = "listFMC", .CmdOps = FMC_Ops, },
	{ .CmdId = GETFMC, .CmdStr = "getFMC", .CmdOps = FMC_Ops, },
};

char Command_Arg[STRLEN_MAX];
char Target_Arg[STRLEN_MAX];
char Value_Arg[SYSCMD_MAX];
int C_Flag, T_Flag, V_Flag;
static Command_t Command;

/*
 * Main routine
 */
int
main(int argc, char **argv)
{
	char Buffer[SYSCMD_MAX] = { 0 };
	int Ret = -1;

	for (int i = 0; (i < argc && strlen(Buffer) < SYSCMD_MAX); i++) {
		(void) strncat(Buffer, argv[i], (SYSCMD_MAX - strlen(Buffer)));
		(void) strncat(Buffer, " ", (SYSCMD_MAX - strlen(Buffer)));
	}

	SC_OPENLOG("sc_app");
	SC_INFO(">>> Begin: %s", Buffer);

	if (Create_Lockfile() != 0) {
		goto Out;
	}

	if (Board_Identification() != 0) {
		goto Unlock;
	}

	if (Parse_Options(argc, argv) != 0) {
		goto Unlock;
	}

	Ret = (*Command.CmdOps)();

Unlock:
	if (Destroy_Lockfile() != 0) {
		Ret = -1;
	}

Out:
	SC_INFO("<<< End(%d): %s", Ret, Buffer);
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
			SC_PRINT("%s", Usage);
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

	for (int i = 0; i < COMMAND_MAX; i++) {
		if (strcmp(Command_Arg, (char *)Commands[i].CmdStr) == 0) {
			Command = Commands[i];
			return 0;
		}
	}

	SC_ERR("invalid command");
	return -1;
}

/*
 * Create the lockfile
 */
int
Create_Lockfile(void)
{
	FILE *FP = NULL;
	pid_t PID;
	char Output[STRLEN_MAX];

	/* If there is no lockfile, create one */
	if (access(LOCKFILE, F_OK) != 0) {
		goto LockFile;
	}

	/* Verify the validity of pid recorded in lockfile */
	FP = fopen(LOCKFILE, "r");
	if (FP == NULL) {
		SC_ERR("open %s failed: %m", LOCKFILE);
		return -1;
	}

	(void) fgets(Output, sizeof(Output), FP);
	(void) fclose(FP);
	PID = atoi(Output);
	if ((PID < 301) || (kill(PID, 0) == -1 && errno == ESRCH)) {
		/*
		 * If pid is below the minimum or pid in the lockfile is stale,
		 * replace it with current pid.  Minimum pid used for new
		 * processes is 301.
		 */
		goto LockFile;
	} else {
		/* Another instance of sc_app is running */
		SC_ERR("lockfile %s exists", LOCKFILE);
		return -1;
	}

LockFile:
	FP = fopen(LOCKFILE, "w");
	if (FP == NULL) {
		SC_ERR("open/create %s failed: %m", LOCKFILE);
		return -1;
	}

	fprintf(FP, "%d\n", getpid());
	(void) fclose(FP);
	return 0;
}

/*
 * Destroy the lockfile
 */
int
Destroy_Lockfile(void)
{
	(void) remove(LOCKFILE);
	return 0;
}

/*
 * Version Operations
 */
int
Version_Ops(void)
{
	int Major = -1;
	int Minor = -1;
	struct utsname UTS;
	char BSP_Version[STRLEN_MAX];
	int Linux_Compatible = 1;
	int BSP_Compatible = 1;
	char *CP;

	if (Plat_Ops->Version_Op != NULL) {
		(void) Plat_Ops->Version_Op(&Major, &Minor);
	}

	if (Major == -1 && Minor == -1) {
		Major = MAJOR;
		Minor = MINOR;
	}

	SC_PRINT("Version:\t%d.%d", Major, Minor);

	if (uname(&UTS) != 0) {
		SC_ERR("get OS information: uname failed: %m");
		return -1;
	}

	CP = strrchr(UTS.nodename, '-');
	if (CP == NULL) {
		SC_ERR("failed to obtain BSP release");
		return -1;
	}

	(void) strcpy(BSP_Version, ++CP);

	if (Major == 1) {
		if (strcmp(UTS.release, LINUX_VERSION) != 0) {
			Linux_Compatible = 0;
		}

		if (strcmp(BSP_Version, BSP_VERSION) != 0) {
			BSP_Compatible = 0;
		}
	}

	SC_PRINT("Linux:\t\t%s (%sCompatible)", UTS.release,
	    (Linux_Compatible) ? "" : "Not ");
	SC_PRINT("BSP:\t\t%s (%sCompatible)", BSP_Version,
	    (BSP_Compatible) ? "" : "Not ");

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
	if ((BootModes == NULL) || (Plat_Ops->BootMode_Op == NULL)) {
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

	/* Validate the bootmode target */
	if (T_Flag == 0) {
		SC_ERR("no bootmode target");
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
		SC_ERR("invalid bootmode target");
		return -1;
	}

	switch (Command.CmdId) {
	case SETBOOTMODE:
		return Plat_Ops->BootMode_Op(BootMode, 1);
	default:
		SC_ERR("invalid bootmode command");
		break;
	}

	return 0;
}

int
Reset_Ops(void)
{
	if (Plat_Ops->Reset_Op == NULL) {
		SC_ERR("reset operation is not supported");
		return -1;
	}

	return Plat_Ops->Reset_Op();
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
	printf("    ");
	for (int i = 0; i < Partition; i++) {
		printf("%2x ", i);
	}

	printf("\n00: ");
	for (int i = 0, j = Partition; i < Size; i++) {
		printf("%.2x ", Buffer[i]);
		if (((i + 1) % Partition) == 0) {
			if ((i + 1) < Size) {
				printf("\n%.2x: ", j);
				j += Partition;
			}
		}
	}

	printf("\n");
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
	static struct tm BuildDate;
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
		if (Plat_Ops->IDCODE_Op == NULL) {
			SC_ERR("eeprom operation is not supported");
			return -1;
		}

		if (Plat_Ops->IDCODE_Op(Buffer, STRLEN_MAX) != 0) {
			SC_ERR("failed to get silicon revision");
			return -1;
		}

		(void) strtok(Buffer, " ");
		(void) strcpy(Buffer, strtok(NULL, "\n"));
		SC_PRINT("Silicon Revision: %s", Buffer);

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

		SC_INFO("Manufacturing Date: %s", ctime(&Time));
		printf("Manufacturing Date: %s", ctime(&Time));
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
		SC_PRINT("MAC Address 0: %.2x:%.2x:%.2x:%.2x:%.2x:%.2x",
		       In_Buffer[0x80], In_Buffer[0x81], In_Buffer[0x82],
		       In_Buffer[0x83], In_Buffer[0x84], In_Buffer[0x85]);
		SC_PRINT("MAC Address 1: %.2x:%.2x:%.2x:%.2x:%.2x:%.2x",
		       In_Buffer[0x86], In_Buffer[0x87], In_Buffer[0x88],
		       In_Buffer[0x89], In_Buffer[0x8A], In_Buffer[0x8B]);
		break;
	case EEPROM_ALL:
		EEPROM_Print_All(In_Buffer, 256, 16);
		break;
	case EEPROM_COMMON:
		return EEPROM_Common(In_Buffer);
	case EEPROM_BOARD:
		return EEPROM_Board(In_Buffer, 1);
	case EEPROM_MULTIRECORD:
		return EEPROM_MultiRecord(In_Buffer);
	default:
		SC_ERR("invalid geteeprom target");
		return -1;
	}

	return 0;
}

int
Temperature_Ops(void)
{
	if (Plat_Ops->Temperature_Op == NULL) {
		SC_ERR("temperature operation is not supported");
		return -1;
	}

	if (Command.CmdId == LISTTEMP) {
		SC_PRINT("MDIO");
		return 0;
	}

	/* Validate the target for gettemp command */
	if (T_Flag == 0) {
		SC_ERR("no gettemp target");
		return -1;
	}

	if (strcmp(Target_Arg, "MDIO") != 0) {
		SC_ERR("invalid gettemp target");
		return -1;
	}

	return Plat_Ops->Temperature_Op();
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
	char System_Cmd[SYSCMD_MAX];
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
			SC_PRINT("%s", Voltages->Voltage[i].Name);
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
	SC_INFO("Calculating Current_LSB = %#x", (unsigned short)Current_LSB);

	/* if Current is negative, use its absolute value */
	if (Regs.Current > 0x7FFF) {
		Regs.Current -= 0x10000;
	}

	*Current = ((float)Regs.Current * Current_LSB);
	*Current *= INA226->Phase_Multiplier;

	*Voltage = (float)Regs.Bus_Voltage;
	*Voltage *= 1.25;       // 1.25 mV per bit
	*Voltage /= 1000;

	/* The power LSB has a fixed ratio to the Current_LSB of 25 */
	*Power = ((float)Regs.Power * Current_LSB * 25);
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
				       "register", Value);
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

/*
 * DDR serial presence detect (SPD) EEPROM Operations.
 *
 * SPD is a standardized way to access information about a memory module.
 * It's an EEPROM on a DIMM where the lower 128 bytes contain certain
 * parameters required by the JEDEC standards, including type, size, etc.
 * The SPD EEPROM is accessed using SMBus; address range: 0x50–0x57 or
 * 0x30–0x37. The TSE2004av extension uses addresses 0x18–0x1F to access
 * an optional on-chip temperature sensor.
 */

struct spd_ddr4 {
	__u8 spd_bytes;		// 0
	__u8 spd_rev;		// 1
	__u8 spd_mem_type;  // 2
	__u8 spd_mod_type;  // 3 needs mask 0xf
	__u8 spd_mem_size;  // 4 needs mask 0xf
	__u8 spd_to14[9];   // byte 5-9
	__u8 spd_tsensor;   // msbit: Thermal sensor
};

union spd_ddr {
	__u64 a64[2];
	__u8  b[16];
	struct spd_ddr4 ddr4;
	// struct spd_ddr3 ddr3;
};

int DDRi2c_read(int fd, __u8 *Buf, struct i2c_info *Iic)
{
	int Ret = 0;

	if (ioctl(fd, I2C_SLAVE_FORCE, Iic->Bus_addr) < 0) {
		SC_ERR("failed to configure I2C bus to access device %#x: %m",
		       Iic->Bus_addr);
		return -1;
	}

	I2C_READ(fd, Iic->Bus_addr, Iic->Read_len, &Iic->Reg_addr, Buf, Ret);
	return Ret;
}

/*
 * DDRSPD is the EEPROM on a DIMM card. First 16 bytes indicate type.
 * First 16 bytes indicate type.
 * Nibbles 4 and 5 (numbered from 0) indicates ddr4ram: "0C" in ASCII,
 * Nibble 9 indicates size: 0, .5G, 1G, 2G, 4G, 8G, or 16G
 * Nibble 28 temp sensor: 8 = present, 0 = not present
 */
int DIMM_spd(int fd)
{
	char Buf_1[STRLEN_MAX];
	char Buf_2[SYSCMD_MAX] = { 0 };
	DIMM_t *DIMM;
	union spd_ddr Spd_buf = {.a64 = {0, 0}};
	struct spd_ddr4 *p = &Spd_buf.ddr4;
	__u8 Sz256;
	int Ret;

	DIMM = Plat_Devs->DIMM;
	Ret = DDRi2c_read(fd, Spd_buf.b, &DIMM->Spd);
	if (Ret < 0) {
		SC_ERR("failed to read DDR SPD");
		return Ret;
	}

	Sz256 = 0xf & p->spd_mem_size;
	SC_PRINT("DDR4 SDRAM?\t%s", ((p->spd_mem_type == 0xc) ? "Yes" : "No"));
	if (Sz256 > 1) {
		SC_PRINT("Size(Gb):\t%u", (1 << (Sz256 - 2)));
	} else {
		SC_PRINT("Size(Mb):\t%s", (Sz256 ? "512" : "0"));
	}

	SC_PRINT("Temp. Sensor?\t%s", ((0x80 & p->spd_tsensor) ? "Yes" : "No"));

	for (int j = 0; j < sizeof(Spd_buf.b); j++) {
		(void) sprintf(Buf_1, "%c%02x", (0xf & j) ? ' ' : '\n',
			       Spd_buf.b[j]);
		(void) strcat(Buf_2, Buf_1);
	}

	SC_INFO("%s", Buf_2);
	SC_INFO("spd_bytes, revision = %u, %u", p->spd_bytes, p->spd_rev);
	SC_INFO("spd_mem_type = %u", p->spd_mem_type);
	SC_INFO("spd_mod_type = %u", 0xf & p->spd_mod_type);
	SC_INFO("spd_mem_size = %u or %u MB", Sz256, Sz256 ? (256 << Sz256) : 0);
	SC_INFO("spd_tsensor  = %c", (0x80 & p->spd_tsensor) ? 'Y' : 'N');

	return Ret;
}

/*
 * See Temperature format description in SE98A data sheet
 *   tttt_tttt_XXXS_TTTT  ->  STTT_Tttt_tttt_t000
 * swap bytes, set the signed bit with shifts
 * adjust for the .125 C resolution in printf
 */
int DIMM_temperature(int fd)
{
	DIMM_t *DIMM;
	__u16 Tbuf = 0;
	int Ret = 0;

	DIMM = Plat_Devs->DIMM;
	Ret = DDRi2c_read(fd, (void *)&Tbuf, &DIMM->Therm);
	if (Ret == 0) {
		__s16 t = (Tbuf << 8 | Tbuf >> 8) << 3;
		SC_PRINT("Temperature(C):\t%.2f", (.125 * t / 16));
	}

	return Ret;
}

/*
 * DDRSPD is the EEPROM on a DIMM card. There might be a temperature sensor.
 * Assume there is only one dimm on our boards
 */
int DDR_Ops(void)
{
	DIMM_t *DIMM;
	int FD, Ret = 0;

	DIMM = Plat_Devs->DIMM;
	if (DIMM == NULL) {
		SC_ERR("ddr operation is not supported");
		return -1;
	}

	if (Command.CmdId == LISTDDR) {
		SC_PRINT("%s", DIMM->Name);
		return 0;
	}

	if (T_Flag == 0) {
		SC_ERR("no target is provided for getddr command");
		return -1;
	}

	if (strcmp(Target_Arg, DIMM->Name) != 0) {
		SC_ERR("invalid getddr target");
		return -1;
	}

	if (V_Flag == 0) {
		SC_ERR("no value is provided for getddr command");
		return -1;
	}

	if (strcmp(Value_Arg, "spd") != 0 && strcmp(Value_Arg, "temp") != 0) {
		SC_ERR("%s is not a valid value", Value_Arg);
		return -1;
	}

	FD = open(DIMM->I2C_Bus, O_RDWR);
	if (FD < 0) {
		SC_ERR("failed to open I2C bus %s: %m", DIMM->I2C_Bus);
		return -1;
	}

	if (Value_Arg[0] == 't') {
		Ret = DIMM_temperature(FD);
	} else {
		Ret = DIMM_spd(FD);
	}

	(void) close(FD);
	return Ret;
}

static int Gpio_get1(const GPIO_t *p)
{
	int State;

	if (GPIO_Get((char *)p->Internal_Name, &State) != 0) {
		SC_ERR("failed to get GPIO line %s", p->Display_Name);
		return -1;
	}

	SC_PRINT("%s (line %2d):\t%d", p->Display_Name, p->Line, State);
	return 0;
}

static int Gpio_get_all(void)
{
	FILE *FP;
	int Line, State;
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
		Line = atoi(strtok(NULL, " :\""));
		(void) strcpy(Label, strtok(NULL, " :\""));
		(void) strcpy(Usage, strtok(NULL, " :\""));
		if (strcmp(Usage, "unused") != 0) {
			SC_PRINT("%s (line %d):\tbusy, used by %s", Label,
			       Line, Usage);
			continue;
		}

		if (GPIO_Get(Label, &State) != 0) {
			SC_ERR("failed to get GPIO line %s", Label);
			(void) pclose(FP);
			return -1;
		}

		SC_PRINT("%s (line %d):\t%d", Label, Line, State);
	}

	(void) pclose(FP);
	return 0;
}

static int Gpio_get(void)
{
	GPIOs_t *GPIOs;
	long Tval = strtol(Target_Arg, NULL, 0);

	if (strcmp(Target_Arg, "all") == 0) {
		if (Gpio_get_all() != 0) {
			SC_ERR("failed to get all GPIO lines");
			return -1;
		};

		return 0;
	}

	GPIOs = Plat_Devs->GPIOs;
	for (int i = 0; i < GPIOs->Numbers; i++) {
		if (Tval == GPIOs->GPIO[i].Line ||
		    !strncmp(GPIOs->GPIO[i].Display_Name, Target_Arg, STRLEN_MAX) ||
		    !strncmp(GPIOs->GPIO[i].Internal_Name, Target_Arg, STRLEN_MAX)) {
			if (Gpio_get1(&GPIOs->GPIO[i]) != 0) {
				return -1;
			}

			return 0;
		}
	}

	SC_ERR("no valid target");
	return -1;
}

/*
 * GPIO_Ops lists the supported gpio lines
 */
int GPIO_Ops(void)
{
	GPIOs_t *GPIOs;

	GPIOs = Plat_Devs->GPIOs;
	if (GPIOs == NULL) {
		SC_ERR("gpio operation is not supported");
		return -1;
	}

	if (Command.CmdId == LISTGPIO) {
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

	return Gpio_get();
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

			SC_PRINT("Input GPIO:\t%#x", Value);

			if (Access_IO_Exp(IO_Exp, 0, 0x2,
					  (unsigned int *)&Value) != 0) {
				SC_ERR("failed to read output");
				return -1;
			}

			SC_PRINT("Output GPIO:\t%#x", Value);

			if (Access_IO_Exp(IO_Exp, 0, 0x6,
					  (unsigned int *)&Value) != 0) {
				SC_ERR("failed to read direction");
				return -1;
			}

			SC_PRINT("Direction:\t%#x", Value);

		} else if (strcmp(Value_Arg, "input") == 0) {
			if (Access_IO_Exp(IO_Exp, 0, 0x0,
					  (unsigned int *)&Value) != 0) {
				SC_ERR("failed to read input");
				return -1;
			}

			for (int i = 0; i < IO_Exp->Numbers; i++) {
				if (IO_Exp->Directions[i] == 1) {
					SC_PRINT("%s:\t%d", IO_Exp->Labels[i],
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
					SC_PRINT("%s:\t%d", IO_Exp->Labels[i],
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
		Value = 0;
		for (int i = 0; i < IO_Exp->Numbers; i++) {
			Value <<= 1;
			if ((IO_Exp->Directions[i] == 1) ||
			   (IO_Exp->Directions[i] == -1)) {
				Value |= 1;
			}
		}

		if (Access_IO_Exp(IO_Exp, 1, 0x6,
				  (unsigned int *)&Value) != 0) {
			SC_ERR("failed to set direction");
			return -1;
		}

		Value = (~Value & ((1 << IO_Exp->Numbers) - 1));
		if (Access_IO_Exp(IO_Exp, 1, 0x2,
				  (unsigned int *)&Value) != 0) {
			SC_ERR("failed to set output");
			return -1;
		}

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
		FD = open(SFP->I2C_Bus, O_RDWR);
		if (FD < 0) {
			SC_ERR("failed to access I2C bus %s: %m", SFP->I2C_Bus);
			return -1;
		}

		if (ioctl(FD, I2C_SLAVE_FORCE, SFP->I2C_Address) < 0) {
			SC_ERR("failed to configure I2C bus for access to "
			       "device address %#x: %m", SFP->I2C_Address);
			return -1;
		}

		/*
		 * If the read operation fails, it indicates that there is
		 * no SFP device plugged into the connector referenced by
		 * the I2C device address.
		 */
		if (read(FD, Buffer, 1) != 1) {
			(void) close(FD);
			continue;
		}

		SC_PRINT("%s", SFP->Name);
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

	FD = open(SFP->I2C_Bus, O_RDWR);
	if (FD < 0) {
		SC_ERR("unable to access I2C bus %s: %m", SFP->I2C_Bus);
		return -1;
	}

	switch (Command.CmdId) {
	case GETSFP:
		(void) memset(Out_Buffer, 0, STRLEN_MAX);
		(void) memset(In_Buffer, 0, STRLEN_MAX);
		Out_Buffer[0] = 0x14;	// 0x14-0x23: Vendor Name
		I2C_READ(FD, SFP->I2C_Address, 16, Out_Buffer, In_Buffer, Ret);
		if (Ret != 0) {
			(void) close(FD);
			return Ret;
		}

		SC_PRINT("Manufacturer (0x14-0x23):\t%s", In_Buffer);

		(void) memset(Out_Buffer, 0, STRLEN_MAX);
		(void) memset(In_Buffer, 0, STRLEN_MAX);
		Out_Buffer[0] = 0x28;	// 0x28-0x37: Part Number
		I2C_READ(FD, SFP->I2C_Address, 16, Out_Buffer, In_Buffer, Ret);
		if (Ret != 0) {
			(void) close(FD);
			return Ret;
		}

		SC_PRINT("Part Number (0x28-0x37):\t%s", In_Buffer);

		(void) memset(Out_Buffer, 0, STRLEN_MAX);
		(void) memset(In_Buffer, 0, STRLEN_MAX);
		Out_Buffer[0] = 0x44;	// 0x44-0x53: Serial Number
		I2C_READ(FD, SFP->I2C_Address, 16, Out_Buffer, In_Buffer, Ret);
		if (Ret != 0) {
			(void) close(FD);
			return Ret;
		}

		SC_PRINT("Serial Number (0x44-0x53):\t%s", In_Buffer);

		(void) memset(Out_Buffer, 0, STRLEN_MAX);
		(void) memset(In_Buffer, 0, STRLEN_MAX);
		Out_Buffer[0] = 0x60;	// 0x60-0x61: Temperature Monitor
		I2C_READ(FD, SFP->I2C_Address + 1, 2, Out_Buffer, In_Buffer, Ret);
		if (Ret != 0) {
			(void) close(FD);
			return Ret;
		}

		Value = (In_Buffer[0] << 8) | In_Buffer[1];
		Value = (Value & 0x7FFF) - (Value & 0x8000);
		/* Each bit of low byte is equivalent to 1/256 celsius */
		SC_PRINT("Temperature(C) (0x60-0x61):\t%.2f", ((float)Value / 256));

		(void) memset(Out_Buffer, 0, STRLEN_MAX);
		(void) memset(In_Buffer, 0, STRLEN_MAX);
		Out_Buffer[0] = 0x62;	// 0x62-0x63: Voltage Sense
		I2C_READ(FD, SFP->I2C_Address + 1, 2, Out_Buffer, In_Buffer, Ret);
		if (Ret != 0) {
			(void) close(FD);
			return Ret;
		}

		Value = (In_Buffer[0] << 8) | In_Buffer[1];
		/* Each bit is 100 uV */
		SC_PRINT("Supply Voltage(V) (0x62-0x63):\t%.2f", ((float)Value * 0.0001));

		(void) memset(Out_Buffer, 0, STRLEN_MAX);
		(void) memset(In_Buffer, 0, STRLEN_MAX);
		Out_Buffer[0] = 0x70;	// 0x70-0x71: Alarm
		I2C_READ(FD, SFP->I2C_Address + 1, 2, Out_Buffer, In_Buffer, Ret);
		if (Ret != 0) {
			(void) close(FD);
			return Ret;
		}

		SC_PRINT("Alarm (0x70-0x71):\t%#x", (In_Buffer[0] << 8) | In_Buffer[1]);
		break;

	default:
		SC_ERR("invalid SFP command");
		(void) close(FD);
		return -1;
	}

	(void) close(FD);
	return 0;
}

/*
 * QSFP Operations
 */
int QSFP_Ops(void)
{
	int Target_Index = -1;
	unsigned long int Value;
	QSFPs_t *QSFPs;
	QSFP_t *QSFP;
	int FD;
	char In_Buffer[STRLEN_MAX];
	char Out_Buffer[STRLEN_MAX];
	int Ret = 0;

	QSFPs = Plat_Devs->QSFPs;
	if (QSFPs == NULL) {
		SC_ERR("QSFP operation is not supported");
		return -1;
	}

	if (Command.CmdId == LISTQSFP) {
		for (int i = 0; i < QSFPs->Numbers; i++) {
			printf("%s\n", QSFPs->QSFP[i].Name);
		}

		return 0;
	}

	/* Validate the QSFP target */
	if (T_Flag == 0) {
		SC_ERR("no QSFP target");
		return -1;
	}

	for (int i = 0; i < QSFPs->Numbers; i++) {
		if (strcmp(Target_Arg, (char *)QSFPs->QSFP[i].Name) == 0) {
			Target_Index = i;
			QSFP = &QSFPs->QSFP[Target_Index];
			break;
		}
	}

	if (Target_Index == -1) {
		SC_ERR("invalid QSFP target");
		return -1;
	}

	if (Plat_Ops->QSFP_ModuleSelect_Op(QSFP, 1) != 0) {
		return -1;
	}

	FD = open(QSFP->I2C_Bus, O_RDWR);
	if (FD < 0) {
		SC_ERR("unable to access I2C bus %s: %m", QSFP->I2C_Bus);
		Ret = -1;
		goto Out;
	}

	switch (Command.CmdId) {
	case GETQSFP:
		(void) memset(Out_Buffer, 0, STRLEN_MAX);
		(void) memset(In_Buffer, 0, STRLEN_MAX);
		if (QSFP->Type == qsfp) {
			Out_Buffer[0] = 0x94;	// 0x94-0xA3: Vendor Name
		} else if (QSFP->Type == qsfpdd) {
			Out_Buffer[0] = 0x81;	// 0x81-0x90: Vendor Name
		} else {
			SC_ERR("Unsupported QSFP");
			Ret = -1;
			goto Out;
		}

		I2C_READ(FD, QSFP->I2C_Address, 16, Out_Buffer, In_Buffer, Ret);
		if (Ret != 0) {
			goto Out;
		}

		SC_PRINT("Manufacturer (%#x-%#x):\t%s", Out_Buffer[0],
			 (Out_Buffer[0] + 15), In_Buffer);

		(void) memset(Out_Buffer, 0, STRLEN_MAX);
		(void) memset(In_Buffer, 0, STRLEN_MAX);
		if (QSFP->Type == qsfp) {
			Out_Buffer[0] = 0xA8;	// 0xA8-0xB7: Part Number
		} else if (QSFP->Type == qsfpdd) {
			Out_Buffer[0] = 0x94;	// 0x94-0xA3: Part Number
		} else {
			SC_ERR("Unsupported QSFP");
			Ret = -1;
			goto Out;
		}

		I2C_READ(FD, QSFP->I2C_Address, 16, Out_Buffer, In_Buffer, Ret);
		if (Ret != 0) {
			goto Out;
		}

		SC_PRINT("Part Number (%#x-%#x):\t%s", Out_Buffer[0],
			 (Out_Buffer[0] + 15), In_Buffer);

		(void) memset(Out_Buffer, 0, STRLEN_MAX);
		(void) memset(In_Buffer, 0, STRLEN_MAX);
		if (QSFP->Type == qsfp) {
			Out_Buffer[0] = 0xC4;	// 0xC4-0xD3: Serial Number
		} else if (QSFP->Type == qsfpdd) {
			Out_Buffer[0] = 0xA6;	// 0xA6-0xB5: Serial Number
		} else {
			SC_ERR("Unsupported QSFP");
			Ret = -1;
			goto Out;
		}

		I2C_READ(FD, QSFP->I2C_Address, 16, Out_Buffer, In_Buffer, Ret);
		if (Ret != 0) {
			goto Out;
		}

		SC_PRINT("Serial Number (%#x-%#x):\t%s", Out_Buffer[0],
			 (Out_Buffer[0] + 15), In_Buffer);

		(void) memset(Out_Buffer, 0, STRLEN_MAX);
		(void) memset(In_Buffer, 0, STRLEN_MAX);
		if (QSFP->Type == qsfp) {
			Out_Buffer[0] = 0x16;	// 0x16-0x17: Temperature
		} else if (QSFP->Type == qsfpdd) {
			Out_Buffer[0] = 0xE;	// 0xE-0xF: Temperature
		} else {
			SC_ERR("Unsupported QSFP");
			Ret = -1;
			goto Out;
		}

		I2C_READ(FD, QSFP->I2C_Address, 2, Out_Buffer, In_Buffer, Ret);
		if (Ret != 0) {
			goto Out;
		}

		Value = (In_Buffer[0] << 8) | In_Buffer[1];
		SC_INFO("Temperature (%#x-%#x): %#x", Out_Buffer[0],
			(Out_Buffer[0] + 1), Value);
		Value = (Value & 0x7FFF) - (Value & 0x8000);
		/* Each bit of low byte is equivalent to 1/256 celsius */
		SC_PRINT("Temperature(C) (%#x-%#x):\t%.2f", Out_Buffer[0],
			 (Out_Buffer[0] + 1), ((float)Value / 256));

		(void) memset(Out_Buffer, 0, STRLEN_MAX);
		(void) memset(In_Buffer, 0, STRLEN_MAX);
		if (QSFP->Type == qsfp) {
			Out_Buffer[0] = 0x1A;	// 0x1A-0x1B: Supply Voltage
		} else if (QSFP->Type == qsfpdd) {
			Out_Buffer[0] = 0x10;	// 0x10-0x11: Supply Voltage
		} else {
			SC_ERR("Unsupported QSFP");
			Ret = -1;
			goto Out;
		}

		I2C_READ(FD, QSFP->I2C_Address, 2, Out_Buffer, In_Buffer, Ret);
		if (Ret != 0) {
			goto Out;
		}

		Value = (In_Buffer[0] << 8) | In_Buffer[1];
		SC_INFO("Supply Voltage (%#x-%#x): %#x", Out_Buffer[0],
			(Out_Buffer[0] + 1), Value);
		/* Each bit is 100 uV */
		SC_PRINT("Supply Voltage(V) (%#x-%#x):\t%.2f", Out_Buffer[0],
			 (Out_Buffer[0] + 1), ((float)Value * 0.0001));

		if (QSFP->Type == qsfp) {
			(void) memset(Out_Buffer, 0, STRLEN_MAX);
			(void) memset(In_Buffer, 0, STRLEN_MAX);
			Out_Buffer[0] = 0x3;	// 0x3-0x4: Alarms
			I2C_READ(FD, QSFP->I2C_Address, 2, Out_Buffer, In_Buffer, Ret);
			if (Ret != 0) {
				goto Out;
			}

			SC_PRINT("Alarms (0x3-0x4):\t%#x", (In_Buffer[0] << 8) |
				 In_Buffer[1]);

			(void) memset(Out_Buffer, 0, STRLEN_MAX);
			(void) memset(In_Buffer, 0, STRLEN_MAX);
			Out_Buffer[0] = 0x6;	// 0x6-0x7: Alarms
			I2C_READ(FD, QSFP->I2C_Address, 2, Out_Buffer, In_Buffer, Ret);
			if (Ret != 0) {
				goto Out;
			}

			SC_PRINT("Alarms (0x6-0x7):\t%#x", (In_Buffer[0] << 8) |
				 In_Buffer[1]);

			(void) memset(Out_Buffer, 0, STRLEN_MAX);
			(void) memset(In_Buffer, 0, STRLEN_MAX);
			Out_Buffer[0] = 0x9;	// 0x9-0xC: Alarms
			I2C_READ(FD, QSFP->I2C_Address, 4, Out_Buffer, In_Buffer, Ret);
			if (Ret != 0) {
				goto Out;
			}

			SC_PRINT("Alarms (0x9-0xc):\t%#x %#x", ((In_Buffer[0] << 8) |
				 In_Buffer[1]), ((In_Buffer[2] << 8) |
				 In_Buffer[3]));

		} else if (QSFP->Type == qsfpdd) {
			(void) memset(Out_Buffer, 0, STRLEN_MAX);
			(void) memset(In_Buffer, 0, STRLEN_MAX);
			Out_Buffer[0] = 0x8;	// 0x8-0xB: Alarms
			I2C_READ(FD, QSFP->I2C_Address, 4, Out_Buffer, In_Buffer, Ret);
			if (Ret != 0) {
				goto Out;
			}

			SC_PRINT("Alarms (0x8-0xb):\t%#x %#x", ((In_Buffer[0] << 8) |
				 In_Buffer[1]), ((In_Buffer[2] << 8) |
				 In_Buffer[3]));

		} else {
			SC_ERR("Unsupported QSFP");
			Ret = -1;
			goto Out;
		}

		break;

	default:
		SC_ERR("invalid QSFP command");
		Ret = -1;
	}

Out:
	(void) Plat_Ops->QSFP_ModuleSelect_Op(QSFP, 0);
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
	char Buffer[STRLEN_MAX];
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
		return EEPROM_MultiRecord(In_Buffer);
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
	char Out_Buffer[STRLEN_MAX];
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
		printf("%s - %s ", FMC->Name, Buffer);
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
		return EEPROM_MultiRecord(In_Buffer);
	default:
		SC_ERR("invalid FMC value");
		return -1;
	}

	return 0;
}
