#include <stdio.h>
#include <fcntl.h>
#include <unistd.h>
#include <stdlib.h>
#include "sc_app.h"

#define XSDB_ENV	"export XILINX_VITIS=/usr/local/xilinx_vitis; \
			 export TCLLIBPATH=/usr/local/xilinx_vitis; \
			 export TCL_LIBRARY=/usr/local/lib/tcl8.5; \
			 export LD_LIBRARY_PATH=$LD_LIBRARY_PATH:/usr/lib"
#define XSDB_CMD	"/usr/local/xilinx_vitis/xsdb"
#define BIT_PATH	"/usr/share/system-controller-app/BIT/"

extern Clocks_t Clocks;
extern Daughter_Card_t Daughter_Card;
int Clocks_Check(void *);
int XSDB_Command(void *);
int EBM_EEPROM_Check(void *);

/*
 * BITs
 */
typedef enum {
	BIT_CLOCKS_CHECK,
	BIT_IDCODE_CHECK,
	BIT_EBM_EEPROM_CHECK,
	BIT_MAX,
} BIT_Index;

BITs_t BITs = {
	.Numbers = BIT_MAX,
	.BIT[BIT_CLOCKS_CHECK] = {
		.Name = "Check Clocks",		// Name of BIT to run
		.Plat_BIT_Op = Clocks_Check,	// BIT routine to invoke
	},
	.BIT[BIT_IDCODE_CHECK] = {
		.Name = "XCVC1902 IDCODE Check",
		.TCL_File = "idcode/xcvc1902_idcode_check.tcl",
		.Plat_BIT_Op = XSDB_Command,
	},
	.BIT[BIT_EBM_EEPROM_CHECK] = {
		.Name = "EBM EEPROM Check",
		.Plat_BIT_Op = EBM_EEPROM_Check,
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
