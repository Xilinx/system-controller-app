#include <stdio.h>
#include <fcntl.h>
#include <unistd.h>
#include <stdlib.h>
#include "sc_app.h"

extern Clocks_t Clocks;
int Check_Clocks(void);

/*
 * BITs
 */
typedef enum {
	BIT_CHECK_CLOCKS,
	BIT_MAX,
} BIT_Index;

BITs_t BITs = {
	.Numbers = BIT_MAX,
	.BIT[BIT_CHECK_CLOCKS] = {
		.Name = "Check Clocks",		// Name of BIT to run
		.Plat_BIT_Op = Check_Clocks,	// BIT routine to invoke
	},
};

/*
 * This test validates whether the current clock frequency is
 * the same as default value.
 */
int
Check_Clocks()
{
	int fd;
	char ReadBuffer[STRLEN_MAX];
	int Freq, Lower, Upper, Delta;

	for (int i = 0; i < Clocks.Numbers; i++) {
		fd = open(Clocks.Clock[i].Sysfs_Path, O_RDONLY);
		ReadBuffer[0] = '\0';
		if (read(fd, ReadBuffer, sizeof(ReadBuffer)-1) == -1) {
			printf("Check Clocks:  FAIL\n");
			return -1;
		}
		/* Allow up to 100 Hz delta */
		Delta = 100;
		Freq = atoi(ReadBuffer);
		Lower = Clocks.Clock[i].Default_Freq - Delta;
		Upper = Clocks.Clock[i].Default_Freq + Delta;
		if (Freq < Lower || Freq > Upper) {
			printf("Check Clocks:  FAIL\n");
			return -1;
		}
		(void) close(fd);
	}

	printf("Check Clocks:  PASS\n");

	return 0;
}
