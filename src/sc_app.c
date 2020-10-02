#include <stdio.h>
#include <stdlib.h>
#include <fcntl.h>
#include <unistd.h>
#include <string.h>
#include <time.h>
#include <math.h>
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
 * 1.6 - Support for SFP connectors.
 */
#define MAJOR	1
#define MINOR	6

#define LINUX_VERSION	"5.4.0"
#define BSP_VERSION	"2020_2"



extern I2C_Buses_t I2C_Buses;
extern BootModes_t BootModes;
extern Clocks_t Clocks;
extern Ina226s_t Ina226s;
extern Power_Domains_t Power_Domains;
extern Voltages_t Voltages;
extern Workarounds_t Workarounds;
extern BITs_t BITs;
extern struct ddr_dimm1 Dimm1;
extern struct Gpio_line_name Gpio_target[];
extern IO_Exp_t IO_Exp;
extern SFPs_t SFPs;

int Parse_Options(int argc, char **argv);
int Create_Lockfile(void);
int Destroy_Lockfile(void);
int Version_Ops(void);
int BootMode_Ops(void);
int Clock_Ops(void);
int Voltage_Ops(void);
int Power_Ops(void);
int Power_Domain_Ops(void);
int Workaround_Ops(void);
int BIT_Ops(void);
int DDR_Ops(void);
int GPIO_Ops(void);
int IO_Exp_Ops(void);
int SFP_Ops(void);
extern int Plat_Gpio_target_size(void);
extern int Plat_Gpio_match(int, char *);
extern int Plat_Version_Ops(int *Major, int *Minor);
extern int Plat_Reset_Ops(void);
extern int Plat_EEPROM_Ops(void);

static char Usage[] = "\n\
sc_app -c <command> [-t <target> [-v <value>]]\n\n\
<command>:\n\
	version - version and compatibility information\n\
	listbootmode - lists the supported boot mode targets\n\
	setbootmode - set boot mode to <target>\n\
	reset - apply power-on-reset\n\
	eeprom - lists the content of EEPROM\n\
	listclock - lists the supported clock targets\n\
	getclock - get the frequency of <target>\n\
	setclock - set <target> to <value> frequency\n\
	setbootclock - set <target> to <value> frequency at boot time\n\
	restoreclock - restore <target> to default value\n\
	listvoltage - lists the supported voltage targets\n\
	getvoltage - get the voltage of <target>\n\
	listpower - lists the supported power targets\n\
	getpower - get the voltage, current, and power of <target>\n\
	listpowerdomain - lists the supported power domain targets\n\
	powerdomain - get the power used by <target> power domain\n\
	listworkaround - lists the applicable workaround targets\n\
	workaround - apply <target> workaround (may requires <value>)\n\
	listBIT - lists the supported Board Interface Test targets\n\
	BIT - run BIT target\n\
	ddr - get DDR DIMM information: <target> is either 'spd' or 'temp'\n\
	listgpio - lists the supported gpio lines\n\
	getgpio - get the state of <target> gpio\n\
	getioexp - get IO expander <target> of either 'all', 'input', or 'output'\n\
	setioexp - set IO expander <target> of either 'direction' or 'output' to <value>\n\
	restoreioexp - restore IO expander to default values\n\
	listSFP - lists the supported SFP connectors\n\
	getSFP - get the connector information of <target> SFP\n\
	getpwmSFP - get the power mode value of <target> SFP\n\
	setpwmSFP - set the power mode value of <target> SFP to <value>\n\
";

typedef enum {
	VERSION,
	LISTBOOTMODE,
	SETBOOTMODE,
	RESET,
	EEPROM,
	LISTCLOCK,
	GETCLOCK,
	SETCLOCK,
	SETBOOTCLOCK,
	RESTORECLOCK,
	LISTVOLTAGE,
	GETVOLTAGE,
	LISTPOWER,
	GETPOWER,
	LISTPOWERDOMAIN,
	POWERDOMAIN,
	LISTWORKAROUND,
	WORKAROUND,
	LISTBIT,
	BIT,
	DDR,
	LISTGPIO,
	GETGPIO,
	GETIOEXP,
	SETIOEXP,
	RESTOREIOEXP,
	LISTSFP,
	GETSFP,
	GETPWMSFP,
	SETPWMSFP,
	COMMAND_MAX,
} CmdId_t;

typedef struct {
	CmdId_t	CmdId;	
	char CmdStr[STRLEN_MAX];
	int (*CmdOps)(void);
} Command_t;

static Command_t Commands[] = {
	{ .CmdId = VERSION, .CmdStr = "version", .CmdOps = Version_Ops, },
	{ .CmdId = LISTBOOTMODE, .CmdStr = "listbootmode", .CmdOps = BootMode_Ops, },
	{ .CmdId = SETBOOTMODE, .CmdStr = "setbootmode", .CmdOps = BootMode_Ops, },
	{ .CmdId = RESET, .CmdStr = "reset", .CmdOps = Plat_Reset_Ops, },
	{ .CmdId = EEPROM, .CmdStr = "eeprom", .CmdOps = Plat_EEPROM_Ops, },
	{ .CmdId = LISTCLOCK, .CmdStr = "listclock", .CmdOps = Clock_Ops, },
	{ .CmdId = GETCLOCK, .CmdStr = "getclock", .CmdOps = Clock_Ops, },
	{ .CmdId = SETCLOCK, .CmdStr = "setclock", .CmdOps = Clock_Ops, },
	{ .CmdId = SETBOOTCLOCK, .CmdStr = "setbootclock", .CmdOps = Clock_Ops, },
	{ .CmdId = RESTORECLOCK, .CmdStr = "restoreclock", .CmdOps = Clock_Ops, },
	{ .CmdId = LISTVOLTAGE, .CmdStr = "listvoltage", .CmdOps = Voltage_Ops, },
	{ .CmdId = GETVOLTAGE, .CmdStr = "getvoltage", .CmdOps = Voltage_Ops, },
	{ .CmdId = LISTPOWER, .CmdStr = "listpower", .CmdOps = Power_Ops, },
	{ .CmdId = GETPOWER, .CmdStr = "getpower", .CmdOps = Power_Ops, },
	{ .CmdId = LISTPOWERDOMAIN, .CmdStr = "listpowerdomain", .CmdOps = Power_Domain_Ops, },
	{ .CmdId = POWERDOMAIN, .CmdStr = "powerdomain", .CmdOps = Power_Domain_Ops, },
	{ .CmdId = LISTWORKAROUND, .CmdStr = "listworkaround", .CmdOps = Workaround_Ops, },
	{ .CmdId = WORKAROUND, .CmdStr = "workaround", .CmdOps = Workaround_Ops, },
	{ .CmdId = LISTBIT, .CmdStr = "listBIT", .CmdOps = BIT_Ops, },
	{ .CmdId = BIT, .CmdStr = "BIT", .CmdOps = BIT_Ops, },
	{ .CmdId = DDR, .CmdStr = "ddr", .CmdOps = DDR_Ops, },
	{ .CmdId = LISTGPIO, .CmdStr = "listgpio", .CmdOps = GPIO_Ops, },
	{ .CmdId = GETGPIO, .CmdStr = "getgpio", .CmdOps = GPIO_Ops, },
	{ .CmdId = GETIOEXP, .CmdStr = "getioexp", .CmdOps = IO_Exp_Ops, },
	{ .CmdId = SETIOEXP, .CmdStr = "setioexp", .CmdOps = IO_Exp_Ops, },
	{ .CmdId = RESTOREIOEXP, .CmdStr = "restoreioexp", .CmdOps = IO_Exp_Ops, },
	{ .CmdId = LISTSFP, .CmdStr = "listSFP", .CmdOps = SFP_Ops, },
	{ .CmdId = GETSFP, .CmdStr = "getSFP", .CmdOps = SFP_Ops, },
	{ .CmdId = GETPWMSFP, .CmdStr = "getpwmSFP", .CmdOps = SFP_Ops, },
	{ .CmdId = SETPWMSFP, .CmdStr = "setpwmSFP", .CmdOps = SFP_Ops, },
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

	if (Create_Lockfile() != 0) {
		return -1;
	}

	Ret = Parse_Options(argc, argv);
	if (Ret != 0) {
		goto Unlock;
	}

	Ret = (*Command.CmdOps)();

Unlock:
	if (Destroy_Lockfile() != 0) {
		return -1;
	}

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

	printf("ERROR: invalid command\n");
	return -1;
}

/*
 * Create the lockfile
 */
int
Create_Lockfile(void)
{
	FILE *fp = NULL;
	time_t curtime;

	if (access(LOCKFILE, F_OK) == 0) {
		printf("ERROR: lockfile \"%s\" exists\n", LOCKFILE);
		return -1;
	}

	fp = fopen(LOCKFILE, "w");
	if (fp == NULL) {
		printf("ERROR: failed to create lockfile\n");
		return -1;
	}

	time(&curtime);
	fprintf(fp, "%s", ctime(&curtime)); 
	(void) fclose(fp);
	return 0;
}

/*
 * Destroy the lockfile
 */
int
Destroy_Lockfile(void)
{
	if (remove(LOCKFILE) == -1) {
		printf("ERROR: failed to remove lockfile\n");
		return -1;
	}

	return 0;
}

/*
 * Version Operations
 */
int
Version_Ops(void)
{
	int Major, Minor;
	struct utsname UTS;
	char BSP_Version[STRLEN_MAX];
	int Linux_Compatible = 1;
	int BSP_Compatible = 1;
	char *CP;

	(void) (*Plat_Version_Ops)(&Major, &Minor);
	if (Major == -1 && Minor == -1) {
		Major = MAJOR;
		Minor = MINOR;
	}

	printf("Version:\t%d.%d\n", Major, Minor);

	if (uname(&UTS) != 0) {
		printf("ERROR: failed to get OS information\n");
		return -1;
	}

	CP = strrchr(UTS.nodename, '-');
	if (CP == NULL) {
		printf("ERROR: failed to obtain BSP release\n");
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

	printf("Linux:\t\t%s (%sCompatible)\n", UTS.release,
	    (Linux_Compatible) ? "" : "Not ");
	printf("BSP:\t\t%s (%sCompatible)\n", BSP_Version,
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
	Clock_t *Clock;
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
			Clock = &Clocks.Clock[Target_Index];
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
		    Clock->Sysfs_Path);
		system(System_Cmd);
		break;
	case SETCLOCK:
	case SETBOOTCLOCK:
		/* Validate the frequency */
		if (V_Flag == 0) {
			printf("ERROR: no clock frequency\n");
			return -1;
		}

		Value = atol(Value_Arg);
		Upper = Clock->Upper_Freq;
		Lower = Clock->Lower_Freq;
		if (Value > Upper || Value < Lower) {
			printf("ERROR: valid frequency range is %d-%d\n",
			    Lower, Upper);
			return -1;
		}

		sprintf(System_Cmd, "echo %d > %s", (int)Value, Clock->Sysfs_Path);
		system(System_Cmd);

		if (Command.CmdId == SETBOOTCLOCK) {
			/* Remove the old value, if any */
			sprintf(System_Cmd, "sed -i -e \'/^%s/d\' %s 2> /dev/NULL",
			    Clock->Name, CLOCKFILE);
			system(System_Cmd);

			sprintf(System_Cmd, "echo \'%s:\t%d\' >> %s",
			    Clock->Name, (int)Value, CLOCKFILE);
			system(System_Cmd);
		}

		break;
	case RESTORECLOCK:
		Value = Clock->Default_Freq;
		sprintf(System_Cmd, "echo %d > %s", (int)Value, Clock->Sysfs_Path);
		system(System_Cmd);

		/* Remove any custom boot frequency */
		sprintf(System_Cmd, "sed -i -e \'/^%s/d\' %s 2> /dev/NULL",
		    Clock->Name, CLOCKFILE);
		system(System_Cmd);
		break;
	default:
		printf("ERROR: invalid clock command\n");
		break;
	}

	return 0;
}

int Read_Voltage(Voltage_t *Regulator, float *Voltage)
{
	int FD;
	char In_Buffer[STRLEN_MAX];
	char Out_Buffer[STRLEN_MAX];
	signed int Exponent;
	short Mantissa;
	int Get_Vout_Mode = 1;

	FD = open(Regulator->I2C_Bus, O_RDWR);
	if (FD < 0) {
		printf("ERROR: unable to open the voltage regulator\n");
		return -1;
	}

	/* Select the page, if the voltage regulator supports it */
	if (-1 != Regulator->Page_Select) {
		Out_Buffer[0] = 0x0;
		Out_Buffer[1] = Regulator->Page_Select;
		I2C_WRITE(FD, Regulator->I2C_Address, 2, Out_Buffer);
	}

	/*
	 * Reading VOUT_MODE indicates what is READ_VOUT format and
	 * its exponent.  The default format is Linear16:
	 *
	 * Voltage =  Mantissa * 2 ^ -(Exponent)
	 */

	/* IR38164 does not support VOUT_MODE PMBus command */
	if (0 == strcmp(Regulator->Part_Name, "IR38164")) {
		Get_Vout_Mode = 0;
	}

	if (1 == Get_Vout_Mode) {
		Out_Buffer[0] = PMBUS_VOUT_MODE;
		(void *) memset(In_Buffer, 0, STRLEN_MAX);
		I2C_READ(FD, Regulator->I2C_Address, 1, Out_Buffer, In_Buffer);
		Exponent = In_Buffer[0] - (sizeof(int) * 8);
	} else {
		/* For IR38164, use exponent value -8 */
		Exponent = -8;
	}

	Out_Buffer[0] = PMBUS_READ_VOUT;
	(void *) memset(In_Buffer, 0, STRLEN_MAX);
	I2C_READ(FD, Regulator->I2C_Address, 2, Out_Buffer, In_Buffer);
	Mantissa = ((unsigned char)In_Buffer[1] << 8) | (unsigned char)In_Buffer[0];
	*Voltage = Mantissa * pow(2, Exponent);

	(void) close(FD);
	return 0;
}

/*
 * Voltage Operations
 */
int Voltage_Ops(void)
{
	int Target_Index = -1;
	Voltage_t *Regulator;
	float Voltage;

	if (Command.CmdId == LISTVOLTAGE) {
		for (int i = 0; i < Voltages.Numbers; i++) {
			printf("%s\n", Voltages.Voltage[i].Name);
		}
		return 0;
	}

	/* Validate the voltage target */
	if (T_Flag == 0) {
		printf("ERROR: no voltage target\n");
		return -1;
	}

	for (int i = 0; i < Voltages.Numbers; i++) {
		if (strcmp(Target_Arg, (char *)Voltages.Voltage[i].Name) == 0) {
			Target_Index = i;
			Regulator = &Voltages.Voltage[Target_Index];
			break;
		}
	}

	if (Target_Index == -1) {
		printf("ERROR: invalid voltage target\n");
		return -1;
	}

	switch (Command.CmdId) {
	case GETVOLTAGE:
		if (Read_Voltage(Regulator, &Voltage) != 0) {
			printf("ERROR: failed to read voltage from regulator\n");
			return -1;
		}

		printf("Voltage(V):\t%.2f\n", Voltage);
		break;
	default:
		printf("ERROR: invalid voltage command\n");
		break;
	}

	return 0;
}

int Read_Sensor(Ina226_t *Ina226, float *Voltage, float *Current, float *Power)
{
	int FD;
	char In_Buffer[STRLEN_MAX];
	char Out_Buffer[STRLEN_MAX];
	float Shunt_Voltage;

	FD = open(Ina226->I2C_Bus, O_RDWR);
	if (FD < 0) {
		printf("ERROR: unable to open INA226 sensor\n");
		return -1;
	}

	(void *) memset(Out_Buffer, 0, STRLEN_MAX);
	(void *) memset(In_Buffer, 0, STRLEN_MAX);
	Out_Buffer[0] = 0x1;	// Shunt Voltage Register(01h)
	I2C_READ(FD, Ina226->I2C_Address, 2, Out_Buffer, In_Buffer);
	Shunt_Voltage = ((In_Buffer[0] << 8) | In_Buffer[1]);
	/* Ignore negative reading */
	if (Shunt_Voltage >= 0x8000) {
		Shunt_Voltage = 0;
	}
	Shunt_Voltage *= 2.5;	// 2.5 μV per bit
	*Current = (Shunt_Voltage / (float)Ina226->Shunt_Resistor);
	*Current *= Ina226->Phase_Multiplier;

	(void *) memset(Out_Buffer, 0, STRLEN_MAX);
	(void *) memset(In_Buffer, 0, STRLEN_MAX);
	Out_Buffer[0] = 0x2;	// Bus Voltage Register(02h)
	I2C_READ(FD, Ina226->I2C_Address, 2, Out_Buffer, In_Buffer);
	*Voltage = ((In_Buffer[0] << 8) | In_Buffer[1]);
	*Voltage *= 1.25;	// 1.25 mV per bit
	*Voltage /= 1000;

	*Power = *Current * (*Voltage);
	(void) close(FD);
	return 0;
}

/*
 * Power Operations
 */
int Power_Ops(void)
{
	int Target_Index = -1;
	Ina226_t *Ina226;
	float Voltage;
	float Current;
	float Power;

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
			Ina226 = &Ina226s.Ina226[Target_Index];
			break;
		}
	}

	if (Target_Index == -1) {
		printf("ERROR: invalid power target\n");
		return -1;
	}

	switch (Command.CmdId) {
	case GETPOWER:
		if (Read_Sensor(Ina226, &Voltage, &Current, &Power) == -1) {
			printf("ERROR: failed to get power\n");
			return -1;
		}

		printf("Voltage(V):\t%.4f\n", Voltage);
		printf("Current(A):\t%.4f\n", Current);
		printf("Power(W):\t%.4f\n", Power);
		break;

	default:
		printf("ERROR: invalid power command\n");
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
	Power_Domain_t *Power_Domain;
	Ina226_t *Ina226;
	float Voltage;
	float Current;
	float Power;
	float Total_Power = 0;

	if (Command.CmdId == LISTPOWERDOMAIN) {
		for (int i = 0; i < Power_Domains.Numbers; i++) {
			printf("%s\n", Power_Domains.Power_Domain[i].Name);
		}
		return 0;
	}

	/* Validate the power domain target */
	if (T_Flag == 0) {
		printf("ERROR: no power domain target\n");
		return -1;
	}

	for (int i = 0; i < Power_Domains.Numbers; i++) {
		if (strcmp(Target_Arg, (char *)Power_Domains.Power_Domain[i].Name) == 0) {
			Target_Index = i;
			Power_Domain = &Power_Domains.Power_Domain[Target_Index];
			break;
		}
	}

	if (Target_Index == -1) {
		printf("ERROR: invalid power domain target\n");
		return -1;
	}

	switch (Command.CmdId) {
	case POWERDOMAIN:
		for (int i = 0; i < Power_Domain->Numbers; i++) {
			Ina226 = &Ina226s.Ina226[Power_Domain->Rails[i]];
			if (Read_Sensor(Ina226, &Voltage, &Current, &Power) == -1) {
				printf("ERROR: failed to get total power\n");
				return -1;
			}

			Total_Power += Power;
		}

		printf("Power(W):\t%.4f\n", Total_Power);
		break;

	default:
		printf("ERROR: invalid power domain command\n");
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
		if (Value != 0 && Value != 1) {
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

	Return = (*BITs.BIT[Target_Index].Plat_BIT_Op)(&BITs.BIT[Target_Index]);

	return Return;
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
	if (ioctl(fd, I2C_SLAVE_FORCE, Iic->Bus_addr) < 0) {
		perror(Dimm1.I2C_Bus);
		return -1;
	}
	I2C_READ(fd, Iic->Bus_addr, Iic->Read_len, &Iic->Reg_addr, Buf);
	return 0;
}

#ifdef DEBUG
void showbuf(const char *b, unsigned int sz)
{
	for (int j = 0; j < sz; j++)
		printf("%c%02x", (0xf & j) ? ' ' : '\n', b[j]);
	putchar('\n');
}
#endif

/*
 * DDRSPD is the EEPROM on a DIMM card. First 16 bytes indicate type.
 * First 16 bytes indicate type.
 * Nibbles 4 and 5 (numbered from 0) indicates ddr4ram: "0C" in ASCII,
 * Nibble 9 indicates size: 0, .5G, 1G, 2G, 4G, 8G, or 16G
 * Nibble 28 temp sensor: 8 = present, 0 = not present
 */
int DIMM_spd(int fd)
{
	union spd_ddr Spd_buf = {.a64 = {0, 0}};
	struct spd_ddr4 *p = &Spd_buf.ddr4;
	__u8 Sz256;
	int Ret;

	Ret = DDRi2c_read(fd, Spd_buf.b, &Dimm1.Spd);
	if (Ret < 0) {
		perror(Dimm1.I2C_Bus);
		return Ret;
	}

	Sz256 = 0xf & p->spd_mem_size;
	printf("DDR4 SDRAM?\t%s\n",
		((p->spd_mem_type == 0xc) ? "Yes" : "No"));
	if (Sz256 > 1) {
		printf("Size(Gb):\t%u\n", (1 << (Sz256 - 2)));
	} else {
		printf("Size(Mb):\t%s\n", (Sz256 ? "512" : "0"));
	}
	printf("Temp. Sensor?\t%s\n",
		((0x80 & p->spd_tsensor) ? "Yes" : "No"));
#ifdef DEBUG
	showbuf(Spd_buf.b, sizeof(Spd_buf.b));
	printf("spd_bytes, revision = %u, %u\n", p->spd_bytes, p->spd_rev);
	printf("spd_mem_type = %u\n", p->spd_mem_type);
	printf("spd_mod_type = %u\n", 0xf & p->spd_mod_type);
	printf("spd_mem_size = %u or %u MB\n", Sz256, Sz256 ? (256 << Sz256) : 0);
	printf("spd_tsensor  = %c\n", (0x80 & p->spd_tsensor) ? 'Y' : 'N');
#endif
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
	__u16 Tbuf = 0;
	int Ret = 0;

	Ret = DDRi2c_read(fd, (void *)&Tbuf, &Dimm1.Therm);
	if (Ret == 0) {
		__s16 t = (Tbuf << 8 | Tbuf >> 8) << 3;
		printf("Temperature(C):\t%.2f\n", (.125 * t / 16));
	}
	return Ret;
}

int target_match(const char *str)
{
	return strcmp(Target_Arg, str) == 0;
}

int valid_ddr_target(void)
{
	int Ret = !T_Flag || target_match("spd") || target_match("temp");

	if (!Ret)
		fprintf(stderr, "%sERROR: no %s target for ddr command\n",
			Usage, Target_Arg);
	return Ret;
}

/*
 * DDRSPD is the EEPROM on a DIMM card. There might be a temperature sensor.
 * Assume there is only one dimm on our boards
 */
int DDR_Ops(void)
{
	int fd, Ret = 0;

	if (!valid_ddr_target())
		return -1;

	fd = open(Dimm1.I2C_Bus, O_RDWR);
	if (fd < 0) {
		perror(Dimm1.I2C_Bus);
		return -1;
	}

	// Skip if SPD only, else show Temperature
	if (Target_Arg[0] != 's')
		Ret = DIMM_temperature(fd);

	// Skip if Temperature only, else show SPD
	if (Target_Arg[0] != 't')
		Ret = DIMM_spd(fd);

	(void) close(fd);
	return Ret;
}

static void Gpio_get1(const struct Gpio_line_name *p)
{
	char Cmd[STRLEN_MAX];

	sprintf(Cmd, "gpioget gpiochip0 %d", p->Line);
	printf("%s (line %2d):\t", p->Name, p->Line);
	fflush(NULL);
	system(Cmd);
}

static const char Cmd_awk[] = "/usr/bin/gpioinfo |awk -e \'/unnamed/ {next};"
	"/gpiochip.*lines:/ {e=z=\"\"; chip=$1; c=1 + $3 };"
	"/line / { gsub(/[:\"]/,e); a[$2]=$3;"
	" if ($NF ==\"[used]\") used[$2]=$4; else z=z \" \" $2; };"
	"END { \"/usr/bin/gpioget gpiochip0 \" z |getline r;"
	" ct =split(z, az); split(r, ar); for (i=1; i<=ct; i++){"
	" g=az[i]; printf \"%-28s(line %2u): %u\\n\", a[g], g, ar[i];};"
	" for (u in used) printf \"%-28s(line %2u): busy, used by %s\\n\","
	" a[u], u, used[u];}\'";

static void Gpio_get(void)
{
	int i, Total = Plat_Gpio_target_size();
	long Tval = strtol(Target_Arg, NULL, 0);

	if (!strcmp("all", Target_Arg)) {
		system(Cmd_awk);
		return;
	}
	for (i = 0; i < Total; i++) {
		if (Tval == Gpio_target[i].Line || Plat_Gpio_match(i, Target_Arg)) {
			Gpio_get1(&Gpio_target[i]);
			return;
		}
	}
	fprintf(stderr, "%sERROR: no %s target for %s command\n",
		Usage, Target_Arg, Command.CmdStr);
}

void Gpio_list(void)
{
	int i, Total = Plat_Gpio_target_size();

	for (i = 0; i < Total; i++) {
		puts(Gpio_target[i].Name);
	}
}

/*
 * GPIO_Ops lists the supported gpio lines
 * "-c getgpio -t n" - get the state of gpio line "n"
 */
int GPIO_Ops(void)
{
	if (Command.CmdId == LISTGPIO) {
		Gpio_list();
	} else {
		Gpio_get();
	}
	return 0;
}

/*
 * Routine to access IO expander chip.
 *
 * Input -
 *	IO_Exp:	Pointer to IO expander structure.
 *	Op:	0 for read operation, 1 for write operation.
 *	Offset:	0x2 output register offset, 0x6 direction register offset.
 *	*Out:	Pointer to output value to be written to device.
 * Output -
 *	*In:	Pointer to input value read from the device.
 */
int Access_IO_Exp(IO_Exp_t *IO_Exp, int Op, int Offset, unsigned int *Out,
    unsigned int *In)
{
	int FD;
	char In_Buffer[STRLEN_MAX];
	char Out_Buffer[STRLEN_MAX];

	FD = open(IO_Exp->I2C_Bus, O_RDWR);
	if (FD < 0) {
		printf("ERROR: unable to open IO expander\n");
		return -1;
	}

	(void *) memset(Out_Buffer, 0, STRLEN_MAX);
	(void *) memset(In_Buffer, 0, STRLEN_MAX);
	if (Op == 0) {
		Out_Buffer[0] = Offset;
		I2C_READ(FD, IO_Exp->I2C_Address, 2, Out_Buffer, In_Buffer);
		*In = ((In_Buffer[0] << 8) | In_Buffer[1]);

	} else if (Op == 1) {
		Out_Buffer[0] = Offset;
		Out_Buffer[1] = ((*Out >> 8) & 0xFF);
		Out_Buffer[2] = (*Out & 0xFF);
		I2C_WRITE(FD, IO_Exp->I2C_Address, 3, Out_Buffer);

	} else {
		printf("ERROR: invalid access operation\n");
		(void) close(FD);
		return -1;
	}

	(void) close(FD);
	return 0;
}

/*
 * IO Expander Operations
 */
int IO_Exp_Ops(void)
{
	unsigned long int Value;

	if (strcmp(IO_Exp.Name, "TCA6416A") != 0) {
		printf("ERROR: unsupported IO expander chip\n");
		return -1;
	}

	switch (Command.CmdId) {
	case GETIOEXP:
		/* A target argument is required */
		if (T_Flag == 0) {
			printf("ERROR: no IO expander target\n");
			return -1;
		}

		if (strcmp(Target_Arg, "all") == 0) {
			if (Access_IO_Exp(&IO_Exp, 0, 0x0, NULL,
			    (unsigned int *)&Value) != 0) {
				printf("ERROR: failed to read input\n");
				return -1;
			}

			printf("Input GPIO:\t0x%x\n", Value);

			if (Access_IO_Exp(&IO_Exp, 0, 0x2, NULL,
			    (unsigned int *)&Value) != 0) {
				printf("ERROR: failed to read output\n");
				return -1;
			}

			printf("Output GPIO:\t0x%x\n", Value);

			if (Access_IO_Exp(&IO_Exp, 0, 0x6, NULL,
			    (unsigned int *)&Value) != 0) {
				printf("ERROR: failed to read direction\n");
				return -1;
			}

			printf("Direction:\t0x%x\n", Value);

		} else if (strcmp(Target_Arg, "input") == 0) {
			if (Access_IO_Exp(&IO_Exp, 0, 0x0, NULL,
			    (unsigned int *)&Value) != 0) {
				printf("ERROR: failed to read input\n");
				return -1;
			}

			for (int i = 0; i < IO_Exp.Numbers; i++) {
				if (IO_Exp.Directions[i] == 1) {
					printf("%s:\t%d\n", IO_Exp.Labels[i],
					    ((Value >> (IO_Exp.Numbers - i - 1)) & 1));
				}
			}

		} else if (strcmp(Target_Arg, "output") == 0) {
			if (Access_IO_Exp(&IO_Exp, 0, 0x2, NULL,
			    (unsigned int *)&Value) != 0) {
				printf("ERROR: failed to read output\n");
				return -1;
			}

			for (int i = 0; i < IO_Exp.Numbers; i++) {
				if (IO_Exp.Directions[i] == 0) {
					printf("%s:\t%d\n", IO_Exp.Labels[i],
					    ((Value >> (IO_Exp.Numbers - i - 1)) & 1));
				}
			}

		} else {
			printf("ERROR: invalid getioexp target\n");
			return -1;
		}

		break;

	case SETIOEXP:
		/* A target argument is required */
		if (T_Flag == 0) {
			printf("ERROR: no IO expander target\n");
			return -1;
		}

		/* Validate the value argument */
		if (V_Flag == 0) {
			printf("ERROR: no IO expander value\n");
			return -1;
		}

		Value = strtol(Value_Arg, NULL, 16);
		if (strcmp(Target_Arg, "direction") == 0) {
			if (Access_IO_Exp(&IO_Exp, 1, 0x6, (unsigned int *)&Value,
			    NULL) != 0) {
				printf("ERROR: failed to set direction\n");
				return -1;
			}

		} else if (strcmp(Target_Arg, "output") == 0) {
			if (Access_IO_Exp(&IO_Exp, 1, 0x2, (unsigned int *)&Value,
			    NULL) != 0) {
				printf("ERROR: failed to set output\n");
				return -1;
			}

		} else {
			printf("ERROR: invalid setioexp target\n");
			return -1;
		}

		break;

	case RESTOREIOEXP:
		Value = 0;
		for (int i = 0; i < IO_Exp.Numbers; i++) {
			Value <<= 1;
			if ((IO_Exp.Directions[i] == 1) ||
			   (IO_Exp.Directions[i] == -1)) {
				Value |= 1;
			}
		}

		if (Access_IO_Exp(&IO_Exp, 1, 0x6, (unsigned int *)&Value,
		    NULL) != 0) {
			printf("ERROR: failed to set direction\n");
			return -1;
		}

		Value = ~Value;
		if (Access_IO_Exp(&IO_Exp, 1, 0x2, (unsigned int *)&Value,
		    NULL) != 0) {
			printf("ERROR: failed to set output\n");
			return -1;
		}

		break;

	default:
		printf("ERROR: invalid IO expander command\n");
		return -1;
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
	SFP_t *SFP;
	int FD;
	char In_Buffer[STRLEN_MAX];
	char Out_Buffer[STRLEN_MAX];

	if (Command.CmdId == LISTSFP) {
		for (int i = 0; i < SFPs.Numbers; i++) {
			printf("%s\n", SFPs.SFP[i].Name);
		}
		return 0;
	}

	/* Validate the SFP target */
	if (T_Flag == 0) {
		printf("ERROR: no voltage target\n");
		return -1;
	}

	for (int i = 0; i < SFPs.Numbers; i++) {
		if (strcmp(Target_Arg, (char *)SFPs.SFP[i].Name) == 0) {
			Target_Index = i;
			SFP = &SFPs.SFP[Target_Index];
			break;
		}
	}

	if (Target_Index == -1) {
		printf("ERROR: invalid SFP target\n");
		return -1;
	}

	FD = open(SFP->I2C_Bus, O_RDWR);
	if (FD < 0) {
		printf("ERROR: unable to open IO expander\n");
		return -1;
	}

	switch (Command.CmdId) {
	case GETSFP:
		(void *) memset(Out_Buffer, 0, STRLEN_MAX);
		(void *) memset(In_Buffer, 0, STRLEN_MAX);
		Out_Buffer[0] = 0x14;	// 0x14-0x23: SFP Vendor Name
		I2C_READ(FD, SFP->I2C_Address, 16, Out_Buffer, In_Buffer);
		printf("Manufacturer:\t%s\n", In_Buffer);

		(void *) memset(Out_Buffer, 0, STRLEN_MAX);
		(void *) memset(In_Buffer, 0, STRLEN_MAX);
		Out_Buffer[0] = 0x44;	// 0x44-0x53: Serial Number
		I2C_READ(FD, SFP->I2C_Address, 16, Out_Buffer, In_Buffer);
		printf("Serial Number:\t%s\n", In_Buffer);

		(void *) memset(Out_Buffer, 0, STRLEN_MAX);
		(void *) memset(In_Buffer, 0, STRLEN_MAX);
		Out_Buffer[0] = 0x60;	// 0x60-0x61: Temperature Monitor
		I2C_READ(FD, SFP->I2C_Address + 1, 2, Out_Buffer, In_Buffer);
		Value = (In_Buffer[0] << 8) | In_Buffer[1];
		Value = (Value & 0x7FFF) - (Value & 0x8000);
		/* Each bit of low byte is equivalent to 1/256 celsius */
		printf("Internal Temperature(C):\t%.3f\n", ((float)Value / 256));

		(void *) memset(Out_Buffer, 0, STRLEN_MAX);
		(void *) memset(In_Buffer, 0, STRLEN_MAX);
		Out_Buffer[0] = 0x62;	// 0x62-0x63: Voltage Sense
		I2C_READ(FD, SFP->I2C_Address + 1, 2, Out_Buffer, In_Buffer);
		Value = (In_Buffer[0] << 8) | In_Buffer[1];
		/* Each bit is 100 uV */
		printf("Supply Voltage(V):\t%.2f\n", ((float)Value * 0.0001));

		(void *) memset(Out_Buffer, 0, STRLEN_MAX);
		(void *) memset(In_Buffer, 0, STRLEN_MAX);
		Out_Buffer[0] = 0x70;	// 0x70-0x71: Alarm
		I2C_READ(FD, SFP->I2C_Address + 1, 2, Out_Buffer, In_Buffer);
		printf("Alarm:\t%x\n", (In_Buffer[0] << 8) | In_Buffer[1]);
		break;

	case GETPWMSFP:
		(void *) memset(Out_Buffer, 0, STRLEN_MAX);
		(void *) memset(In_Buffer, 0, STRLEN_MAX);
		Out_Buffer[0] = 0x80;	// 0x80-0x81: PWM1 & PWM2 Controller
		I2C_READ(FD, SFP->I2C_Address + 1, 2, Out_Buffer, In_Buffer);
		printf("Power Mode(0-2W):\t%x\n", (In_Buffer[0] << 8) | In_Buffer[1]);
		break;

	case SETPWMSFP:
		/* Validate the value */
		if (V_Flag == 0) {
			printf("ERROR: no PWM value\n");
			(void) close(FD);
			return -1;
		}

		Value = strtol(Value_Arg, NULL, 16);
		if (Value > 0xFF) {
			printf("ERROR: invalid PWM value\n");
			(void) close(FD);
			return -1;
		}

		(void *) memset(Out_Buffer, 0, STRLEN_MAX);
		Out_Buffer[0] = 0x80;	// 0x80: PWM1 Controller
		Out_Buffer[1] = (Value & 0xFF);
		I2C_WRITE(FD, SFP->I2C_Address + 1, 2, Out_Buffer);
		/* Add a delay, since back-to-back write fails for this device. */
		sleep(1);
		Out_Buffer[0] = 0x81;	// 0x81: PWM2 Controller
		Out_Buffer[1] = (Value & 0xFF);
		I2C_WRITE(FD, SFP->I2C_Address + 1, 2, Out_Buffer);
		break;

	default:
		printf("ERROR: invalid SFP command\n");
		(void) close(FD);
		return -1;
	}

	(void) close(FD);
	return 0;
}
