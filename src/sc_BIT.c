#include <stdio.h>
#include <fcntl.h>
#include <unistd.h>
#include <stdlib.h>
#include <string.h>
#include "sc_app.h"

extern Clocks_t Clocks;
extern Daughter_Card_t Daughter_Card;
extern struct ddr_dimm1 Dimm1;
extern Voltages_t Voltages;

int Clocks_Check(void *);
int XSDB_Command(void *);
int EBM_EEPROM_Check(void *);
int DIMM_EEPROM_Check(void *);
int Voltages_Check(void *);

extern int Read_Voltage(Voltage_t *, float *);
extern int Plat_Reset_Ops(void);

/*
 * BITs
 */
typedef enum {
	BIT_CLOCKS_CHECK,
	BIT_IDCODE_CHECK,
	BIT_EBM_EEPROM_CHECK,
	BIT_DIMM_EEPROM_CHECK,
	BIT_VOLTAGES_CHECK,
	BIT_MAX,
} BIT_Index;

BITs_t BITs = {
	.Numbers = BIT_MAX,
	.BIT[BIT_CLOCKS_CHECK] = {
		.Name = "Check Clocks",		// Name of BIT to run
		.Plat_BIT_Op = Clocks_Check,	// BIT routine to invoke
	},
	.BIT[BIT_IDCODE_CHECK] = {
		.Name = "IDCODE Check",
		.TCL_File = "idcode/idcode_check.tcl",
		.Plat_BIT_Op = XSDB_Command,
	},
	.BIT[BIT_EBM_EEPROM_CHECK] = {
		.Name = "EBM EEPROM Check",
		.Plat_BIT_Op = EBM_EEPROM_Check,
	},
	.BIT[BIT_DIMM_EEPROM_CHECK] = {
		.Name = "DIMM EEPROM Check",
		.Plat_BIT_Op = DIMM_EEPROM_Check,
	},
	.BIT[BIT_VOLTAGES_CHECK] = {
		.Name = "Check Voltages",
		.Plat_BIT_Op = Voltages_Check,
	},
};

/*
 * This test validates whether the current clock frequency is
 * the same as default value.
 */
int
Clocks_Check(void *Arg)
{
	BIT_t *BIT_p = Arg;
	int FD;
	char ReadBuffer[STRLEN_MAX];
	int Freq, Lower, Upper, Delta;

	for (int i = 0; i < Clocks.Numbers; i++) {
		FD = open(Clocks.Clock[i].Sysfs_Path, O_RDONLY);
		ReadBuffer[0] = '\0';
		if (read(FD, ReadBuffer, sizeof(ReadBuffer)-1) == -1) {
			printf("ERROR: failed to open the clock\n");
			printf("%s: FAIL\n", BIT_p->Name);
			(void) close(FD);
			return -1;
		}
		(void) close(FD);
		/* Allow up to 100 Hz delta */
		Delta = 100;
		Freq = atoi(ReadBuffer);
		Lower = Clocks.Clock[i].Default_Freq - Delta;
		Upper = Clocks.Clock[i].Default_Freq + Delta;
		if (Freq < Lower || Freq > Upper) {
			printf("%s: BIT failed for clock \'%s\'\n", BIT_p->Name,
			    Clocks.Clock[i].Name);
			printf("%s: FAIL\n", BIT_p->Name);
			return 0;
		}
	}

	printf("%s: PASS\n", BIT_p->Name);
	return 0;
}

/*
 * This routine is used to invoke XSDB with a tcl script that contains
 * the test.
 */
int
XSDB_Command(void *Arg)
{
	BIT_t *BIT_p = Arg;
	char System_Cmd[SYSCMD_MAX];

	(void) Plat_Reset_Ops();
	sprintf(System_Cmd, "%s; %s%s%s; %s %s%s", XSDB_ENV, "echo -n \'", 
	    BIT_p->Name, ": \'", XSDB_CMD, BIT_PATH, BIT_p->TCL_File);
	system(System_Cmd); 

	return 0;
}

/*
 * This routine checks whether EEPROM of EBM daughter card is accessible.
 */
int
EBM_EEPROM_Check(void *Arg)
{
	BIT_t *BIT_p = Arg;
	int FD;

	FD = open(Daughter_Card.I2C_Bus, O_RDWR);
	if (FD < 0) {
		printf("ERROR: unable to open I2C bus\n");
		printf("%s: FAIL\n", BIT_p->Name);
		return -1;
	}

	if (ioctl(FD, I2C_SLAVE_FORCE, Daughter_Card.I2C_Address) < 0) {
		printf("ERROR: unable to access EEPROM device\n");
		printf("%s: FAIL\n", BIT_p->Name);
		(void) close(FD);
		return -1;
	}

	(void) close(FD);
	printf("%s: PASS\n", BIT_p->Name);
	return 0;
}

/*
 * This routine reads DRAM type from DIMM EEPROM to validate that
 * it is accessible.
 */
int
DIMM_EEPROM_Check(void *Arg)
{
	BIT_t *BIT_p = Arg;
	int FD;
	char In_Buffer[STRLEN_MAX];
	char Out_Buffer[STRLEN_MAX];

	FD = open(Dimm1.I2C_Bus, O_RDWR);
	if (FD < 0) {
		printf("ERROR: unable to open I2C bus\n");
		printf("%s: FAIL\n", BIT_p->Name);
		return -1;
	}

	if (ioctl(FD, I2C_SLAVE_FORCE, Dimm1.Spd.Bus_addr) < 0) {
		printf("ERROR: unable to access EEPROM device\n");
		printf("%s: FAIL\n", BIT_p->Name);
		(void) close(FD);
		return -1;
	}

	(void *) memset(Out_Buffer, 0, STRLEN_MAX);
	(void *) memset(In_Buffer, 0, STRLEN_MAX);
	Out_Buffer[0] = 0x2;	// Byte 2: DRAM Device Type
	I2C_READ(FD, Dimm1.Spd.Bus_addr, 1, Out_Buffer, In_Buffer);
	if (In_Buffer[0] != 0xc) {
		printf("ERROR: DIMM is not DDR4\n");
		printf("%s: FAIL\n", BIT_p->Name);
		(void) close(FD);
		return -1;
	}

	(void) close(FD);
	printf("%s: PASS\n", BIT_p->Name);
	return 0;
}

/*
 * This routine gets the voltage from a regulator and compares it against
 * expected minimum and maximum values.
 */
int
Voltages_Check(void *Arg)
{
	BIT_t *BIT_p = Arg;
	Voltage_t *Regulator;
	float Voltage;

	for (int i = 0; i < Voltages.Numbers; i++) {
		Regulator = &Voltages.Voltage[i];
		if (Read_Voltage(Regulator, &Voltage) != 0) {
			printf("ERROR: failed to get voltage for %s\n",
			    Regulator->Name);
			printf("%s: FAIL\n", BIT_p->Name);
			return -1;
		}

		if ((Voltage < Regulator->Minimum_Volt) ||
		    (Voltage > Regulator->Maximum_Volt)) {
			printf("ERROR: voltage for %s is out-of-range\n",
			   Regulator->Name);
			printf("%s: FAIL\n", BIT_p->Name);
			return -1;
		}
	}

	printf("%s: PASS\n", BIT_p->Name);
	return 0;
}
