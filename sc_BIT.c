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
