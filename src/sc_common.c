/*
 * Copyright (c) 2021 - 2022 Xilinx, Inc.  All rights reserved.
 * Copyright (c) 2022 - 2026 Advanced Micro Devices, Inc.  All rights reserved.
 *
 * SPDX-License-Identifier: MIT
 */

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <fcntl.h>
#include <ctype.h>
#include <string.h>
#include <time.h>
#include <math.h>
#include <glob.h>
#include <libgen.h>
#include <sys/stat.h>
#include <sys/mman.h>
#include <stdint.h>
#include "sc_app.h"

Plat_Devs_t *Plat_Devs;

char SC_APP_File[SYSCMD_MAX];
extern char Board_Name[];
extern char Board_Revision[];
extern char Silicon_Revision[];

char *
Appfile(char *Filename)
{
	char Buffer[STRLEN_MAX];

	(void) sprintf(Buffer, "%s/.sc_app", INSTALLDIR);
	if (access(Buffer, F_OK) == -1) {
		if (mkdir(Buffer, 0755) == -1) {
			SC_ERR("mkdir %s failed: %m", Buffer);
			return NULL;
		}
	}

	(void) sprintf(SC_APP_File, "%s/%s", Buffer, Filename);
	return SC_APP_File;
}

#define EEPROM_END_OF_FIELDS	0xC1
#define EEPROM_BOARD_AREA	0x08
#define EEPROM_BOARD_FIELDS_START 0x0E
/*
 * Field index within board area in EEPROM that are represented as strings (from EEPROM_BOARD_FIELDS_START):
 * 0: Manufacturer, 1: Product Name, 2: Serial Number, 3: Part Number,
 * 4: FRU ID, 5: Revision
 */
#define EEPROM_BOARD_NAME_INDEX	1
#define EEPROM_BOARD_REVISION_INDEX 5

/*
 * Read string field Index by walking type/length fields from Start.
 */
static int
EEPROM_Read_Field(const char *Buffer, int Start, int Index, int Area_End,
	       char *Field, size_t Field_Size)
{
	int Offset = Start;
	int Field_Num = 0;
	unsigned char Type_Length;
	int Length;
	int Field_Type;

	Field[0] = '\0';

	while (Offset < Area_End && Offset < 256) {
		Type_Length = (unsigned char)Buffer[Offset];
		if (Type_Length == EEPROM_END_OF_FIELDS) {
			SC_INFO("EEPROM field %d not found: end of fields", Index);
			return -1;
		}

		/* Unused/padding bytes; skip without advancing the field index. */
		if (Type_Length == 0x00 || Type_Length == 0xFF) {
			Offset++;
			continue;
		}

		Field_Type = Type_Length & 0xC0;
		Length = Type_Length & 0x3F;
		if (Field_Type == 0x80) {
			SC_INFO("EEPROM field parsing: length in ASCII is not supported");
			return -1;
		}

		if (Field_Num == Index) {
			/* Board area strings are text strings (type 0xC0); length is in bytes. */
			if (Field_Type != 0xC0) {
				SC_INFO("EEPROM field %d is not a text string", Index);
				return -1;
			}

			if (Length >= (int)Field_Size) {
				Length = (int)Field_Size - 1;
			}

			(void) memcpy(Field, &Buffer[Offset + 1], Length);
			Field[Length] = '\0';
			return 0;
		}

		/* Advance past this field (1 type/length byte + Length data bytes) to the next field. */
		Offset += 1 + Length;
		Field_Num++;
	}

	SC_INFO("EEPROM field %d not found", Index);
	return -1;
}

static int
Run_SC_Board_ID(const char *Option, char *Output, size_t Output_Size)
{
	FILE *FP;
	char Buffer[LSTRLEN_MAX];
	char *SP;

	snprintf(Buffer, sizeof(Buffer), "%s %s 2>/dev/null", SC_BOARD_ID, Option);
	FP = popen(Buffer, "r");
	if (FP == NULL) {
		SC_INFO("failed to invoke '%s %s': %m", SC_BOARD_ID, Option);
		return -1;
	}

	if (fgets(Output, Output_Size, FP) == NULL) {
		SC_INFO("failed to read output from '%s %s'", SC_BOARD_ID, Option);
		(void) pclose(FP);
		return -1;
	}

	if (pclose(FP) != 0) {
		SC_INFO("'%s %s' command failed", SC_BOARD_ID, Option);
		return -1;
	}

	if (Output[0] == '\0') {
		SC_INFO("'%s %s' returned empty output", SC_BOARD_ID, Option);
		return -1;
	}

	for (SP = Output; *SP != '\0'; SP++) {
		*SP = (char)toupper((unsigned char)*SP);
	}

	return 0;
}

static int
Get_Product_Info(OnBoard_EEPROM_t *EEPROM, char *Product_Name, char *Product_Revision)
{
	int FD;
	char In_Buffer[SYSCMD_MAX];
	char Out_Buffer[LSTRLEN_MAX];
	int Board_Area_End;
	int Found = 0;

	/*
	 * Referencing EEPROM to obtain 'Product Name' can be overridden by
	 * defining 'Board' parameter in CONFIGFILE.
	 */
	if (Check_Config_File("Board", Out_Buffer, &Found) != 0) {
		return -1;
	}

	/*
	 * NOTE - if we define 'Board' variable in CONFIGFILE, we are skipping access
	 * to EEPROM.  Therefore, there is no board revision information available in
	 * such an invocation.
	 */
	if (Found) {
		(void) strcpy(Product_Name, Out_Buffer);
		return 0;
	}

	/*
	 * Use sc-board-id utility to get board name and revision to avoid direct EEPROM access.
	 */
	if (access(SC_BOARD_ID, X_OK) == 0) {
		if (Run_SC_Board_ID("--name", Product_Name, LSTRLEN_MAX) == 0 &&
		    Run_SC_Board_ID("--func-rev", Product_Revision, LSTRLEN_MAX) == 0) {
			SC_INFO("Board identity from %s", SC_BOARD_ID);
			SC_INFO("Product Name: %s", Product_Name);
			SC_INFO("Product Revision: %s", Product_Revision);
			return 0;
		}
	}

	SC_INFO("failed to obtain board info from '%s', access EEPROM directly", SC_BOARD_ID);
	FD = open(EEPROM->Path, O_RDWR);
	if (FD < 0) {
		SC_INFO("unable to open EEPROM '%s': %m", EEPROM->Path);
		return -1;
	}

	(void) memset(In_Buffer, 0, SYSCMD_MAX);
	if (read(FD, In_Buffer, 256) != 256) {
		SC_INFO("unable to read onboard EEPROM");
		(void) close(FD);
		return -1;
	}

	(void) close(FD);

	Board_Area_End = EEPROM_BOARD_AREA + ((unsigned char)In_Buffer[EEPROM_BOARD_AREA + 1] * 8);
	if (EEPROM_Read_Field(In_Buffer, EEPROM_BOARD_FIELDS_START, EEPROM_BOARD_NAME_INDEX,
			      Board_Area_End, Product_Name, LSTRLEN_MAX) != 0) {
		SC_INFO("Board EEPROM product name is missing or invalid");
		return -1;
	}

	SC_INFO("Product Name: %s", Product_Name);
	if (EEPROM_Read_Field(In_Buffer, EEPROM_BOARD_FIELDS_START, EEPROM_BOARD_REVISION_INDEX,
			      Board_Area_End, Product_Revision, LSTRLEN_MAX) != 0) {
		SC_INFO("Board EEPROM product revision is missing or invalid");
		return -1;
	}

	SC_INFO("Product Revision: %s", Product_Revision);
	return 0;
}

int
Find_OnBoard_EEPROM(OnBoard_EEPROM_t *OnBoard)
{
	glob_t Glob_Buffer;
	char *Path;

	if (glob(ONBOARD_EEPROM_PATH, 0, NULL, &Glob_Buffer) != 0) {
		SC_ERR("failed to find onboard EEPROM");
		return -1;
	}

	if (Glob_Buffer.gl_pathc == 0 || Glob_Buffer.gl_pathc > 1) {
		SC_ERR("could not find any or an unique onboard EEPROM");
		return -1;
	}

	SC_INFO("EEPROM Path: %s", Glob_Buffer.gl_pathv[0]);
	Path = strdup(Glob_Buffer.gl_pathv[0]);
	OnBoard->Path = calloc(1, (strlen(Path) + strlen("/nvmem") + 1));
	OnBoard->Path = strcat(OnBoard->Path, Path);
	OnBoard->Path = strcat(OnBoard->Path, "/nvmem");
	SC_INFO("OnBoard_EEPROM->Path = %s", OnBoard->Path);

	free(Path);
	globfree(&Glob_Buffer);

	return 0;
}

OnBoard_EEPROM_t OnBoard_EEPROM = {
	.Name = "onboard",
};

int
Board_Identification(char *Board_Name, char *Board_Revision)
{
	char Board_File[SYSCMD_MAX];
	char Board_Path[LSTRLEN_MAX];
	char Value[LSTRLEN_MAX];
	int Found = 0;

	Plat_Devs = (Plat_Devs_t *)calloc(1, sizeof(Plat_Devs_t));

	if (Find_OnBoard_EEPROM(&OnBoard_EEPROM) != 0) {
		SC_INFO("onboard EEPROM path not found");
	}

	if (Get_Product_Info(&OnBoard_EEPROM, Board_Name, Board_Revision) != 0) {
		SC_ERR("failed to identify the board");
		return -1;
	}

	/*
	 * The default location of JSON file can be overridden by defining
	 * 'Board_Path' parameter in CONFIGFILE.
	 */
	if (Check_Config_File("Board_Path", Value, &Found) != 0) {
		return -1;
	}

	(void) strcpy(Board_Path, ((Found) ? Value : BOARD_PATH));
	SC_INFO("Board Path: %s", Board_Path);

	snprintf(Board_File, SYSCMD_MAX, "%s%s-%s.json", Board_Path, Board_Name, Board_Revision);
	SC_INFO("Board File-Board Revision: %s", Board_File);
	if (access(Board_File, F_OK) != 0) {
		snprintf(Board_File, SYSCMD_MAX, "%s%s.json", Board_Path, Board_Name);
		SC_INFO("Board File: %s", Board_File);
		if (access(Board_File, F_OK) != 0) {
			(void) strcpy(Board_Name, "Unknown");
			return 0;
		}
	}

	if (Parse_JSON(Board_File, Plat_Devs) != 0) {
		SC_ERR("failed to parse JSON file for board '%s'",
		       Board_Name);
		return -1;
	}

	Plat_Devs->OnBoard_EEPROM = &OnBoard_EEPROM;
	return 0;
}

static int
Identify_UniqueID(char *Silicon_Rev, char *Board_Rev)
{
	Default_PDI_t *Default_PDI;
	char *SP;
	char UniqueID[STRLEN_MAX], Delimiter[STRLEN_MAX];

	Default_PDI = Plat_Devs->Default_PDI;
	if (Default_PDI == NULL) {
		SC_INFO("no default PDI is defined");
		return 0;
	}

	if (strcmp(Silicon_Rev, "ES1") == 0) {
		(void) snprintf(UniqueID, STRLEN_MAX, "%s", Default_PDI->UniqueID_Rev0);
	} else {
		(void) snprintf(UniqueID, STRLEN_MAX, "%s", Default_PDI->UniqueID_Rev1);
	}

	/*
	 * If unique id data read from JSON file is not of '<board revision>:<unique id>'
	 * format, then there is only one unique id and no further processing is needed.
	 */
	if (strstr(UniqueID, ":") == NULL) {
		(void) strncpy(Default_PDI->UniqueID_InEffect, UniqueID, (ITEMS_MAX - 2));
		Default_PDI->UniqueID_InEffect[ITEMS_MAX - 1] = '\0';
		SC_INFO("UniqueID_InEffect: %s", Default_PDI->UniqueID_InEffect);
		return 0;
	}

	(void) sprintf(Delimiter, "%s:", Board_Rev);
	if ((SP = strstr(UniqueID, Delimiter)) != NULL) {
		(void) strtok(SP, ":");
		(void) snprintf(Default_PDI->UniqueID_InEffect, ITEMS_MAX, "%s", strtok(NULL, " "));
		SC_INFO("UniqueID_InEffect: %s", Default_PDI->UniqueID_InEffect);
		return 0;
	}

	/*
	 * If the unique id data is of '<board revision>:<unique id>' format but none
	 * of them matches the revision of running board, therefore, reference the last
	 * one that does not have '<board revision>:' prefix.
	 */
	SP = strrchr(UniqueID, ' ');
	(void) snprintf(Default_PDI->UniqueID_InEffect, ITEMS_MAX, "%s", SP);
	SC_INFO("UniqueID_InEffect: %s", Default_PDI->UniqueID_InEffect);
	return 0;
}

static int
Identify_PDI(char *Revision)
{
	char Buffer[XLSTRLEN_MAX];
	char PDI_File[LSTRLEN_MAX];

	if (Identify_UniqueID(Revision, Board_Revision) != 0) {
		SC_ERR("failed to identify PDI's unique id");
		return -1;
	}

	/* If a symbolic link already exists to default PDI, return */
	(void) sprintf(Buffer, "%s%s", CUSTOM_PDIS_PATH, "default.pdi");
	if (access(Buffer, F_OK) == 0) {
		SC_INFO("File '%s' already exists", Buffer);
		return 0;
	}

	/* First, check if there is a unique PDI for the current revision of the board */
	if (strcmp(Revision, "ES1") == 0) {
		(void) sprintf(PDI_File, "%s%s/%s_es1_%s", BIT_PATH, Board_Name, Board_Revision, DEFAULT_PDI);
	} else if (strcmp(Revision, "PROD") == 0) {
		(void) sprintf(PDI_File, "%s%s/%s_%s", BIT_PATH, Board_Name, Board_Revision, DEFAULT_PDI);
	} else {
		SC_ERR("unsupported silicon revision");
		return -1;
	}

	/* If there is no revision-based PDI found, try to locate the common default PDI */
	if (access(PDI_File, F_OK) == -1) {
		if (strcmp(Revision, "ES1") == 0) {
			(void) sprintf(PDI_File, "%s%s/es1_%s", BIT_PATH, Board_Name, DEFAULT_PDI);
		} else {
			(void) sprintf(PDI_File, "%s%s/%s", BIT_PATH, Board_Name, DEFAULT_PDI);
		}

		/* If there is no common default PDI found for this board, return */
		if (access(PDI_File, F_OK) == -1) {
			SC_INFO("PDI file '%s' does not exist", PDI_File);
			return 0;
		}
	}

	/* If '/data' directory doesn't exist, create it */
	(void) sprintf(Buffer, "%s", DATADIR);
	if (access(Buffer, F_OK) == -1) {
		if (mkdir(Buffer, 0755) == -1) {
			SC_ERR("mkdir %s failed: %m", Buffer);
			return -1;
		}
	}

	(void) sprintf(Buffer, "%s", CUSTOM_PDIS_PATH);
	if (access(Buffer, F_OK) == -1) {
		if (mkdir(Buffer, 0755) == -1) {
			SC_ERR("mkdir %s failed: %m", Buffer);
			return -1;
		}
	}

	(void) sprintf(Buffer, "%s%s", CUSTOM_PDIS_PATH, "default.pdi");
	if (symlink(PDI_File, Buffer) == -1) {
		SC_ERR("failed to create symbolic link to default PDI");
		return -1;
	}

	return 0;
}

static int
Silicon_File_Complete(void)
{
	FILE *FP;
	char Buffer[STRLEN_MAX];
	int LineCount = 0;

	FP = fopen(SILICONFILE, "r");
	if (FP == NULL) {
		return 0;
	}

	while (fgets(Buffer, sizeof(Buffer), FP) != NULL) {
		LineCount++;
	}

	(void) fclose(FP);
	return (LineCount >= 4);
}

/*
 * Identify silicon revision, IDCODE, and DNA via XSDB.
 *
 * Runs silicon_info.tcl on the DUT, stores lines 2-4 in the silicon file,
 * and preserves line 1 (PDI revision) from Silicon_Revision. Requires the
 * silicon file to exist and Silicon_Revision to be set before invocation.
 */
int
Silicon_Identification(void)
{
	FILE *FP;
	char System_Cmd[SYSCMD_MAX];
	char Buffer[STRLEN_MAX];
	char Lines[3][STRLEN_MAX];
	int LineCount = 0;
	int Ret = 0;
	int i;

	if (access(SILICONFILE, F_OK) != 0) {
		SC_ERR("silicon file %s is missing", SILICONFILE);
		return -1;
	}

	if (Silicon_Revision[0] == '\0') {
		SC_ERR("silicon revision is not available");
		return -1;
	}

	(void) memset(Lines, 0, sizeof(Lines));

	if (Set_JTAGSelect("SC") != 0) {
		return -1;
	}

	(void) sprintf(System_Cmd, "cd %s; %s; %s %s %s 2>&1",
		       SCRIPT_PATH, XSDB_ENV, XSDB_CMD, SILICON_INFO_TCL,
		       Board_Name);

	SC_INFO("Command: %s", System_Cmd);
	FP = popen(System_Cmd, "r");
	if (FP == NULL) {
		SC_ERR("failed to invoke xsdb");
		Ret = -1;
		goto Out;
	}

	while (fgets(Buffer, sizeof(Buffer), FP) != NULL) {
		SC_INFO("XSDB Output: %s", Buffer);
		if (strncmp(Buffer, "ERROR:", 6) == 0) {
			(void) strtok(Buffer, "\n");
			SC_ERR("%s", Buffer);
			(void) pclose(FP);
			Ret = -1;
			goto Out;
		}

		(void) strtok(Buffer, "\n");
		if (Buffer[0] == '\0') {
			continue;
		}

		(void) strcpy(Lines[LineCount], Buffer);
		LineCount++;
	}

	if ((pclose(FP) != 0) || (LineCount != 3)) {
		SC_ERR("failed to identify silicon");
		Ret = -1;
		goto Out;
	}

	FP = fopen(SILICONFILE, "w");
	if (FP == NULL) {
		SC_ERR("failed to write file %s: %m", SILICONFILE);
		Ret = -1;
		goto Out;
	}

	(void) strcpy(Buffer, Silicon_Revision);
	(void) strtok(Buffer, "\n");
	if (fprintf(FP, "%s\n", Buffer) < 0) {
		(void) fclose(FP);
		SC_ERR("failed to store silicon revision");
		Ret = -1;
		goto Out;
	}

	for (i = 0; i < 3; i++) {
		if (fprintf(FP, "%s\n", Lines[i]) < 0) {
			(void) fclose(FP);
			SC_ERR("failed to store silicon information");
			Ret = -1;
			goto Out;
		}
	}

	(void) fclose(FP);

Out:
	(void) Set_JTAGSelect("Current");
	return Ret;
}

int
Get_Silicon_Revision(char *Revision)
{
	FILE *FP;
	char Buffer[STRLEN_MAX];
	char Config_Var[STRLEN_MAX];
	int Found = 0;

	if (Revision[0] == 0) {
		if (access(SILICONFILE, F_OK) == 0) {
			FP = fopen(SILICONFILE, "r");
			if (FP == NULL) {
				SC_ERR("failed to read file %s: %m", SILICONFILE);
				return -1;
			}

			if (fgets(Buffer, sizeof(Buffer), FP) == NULL) {
				(void) fclose(FP);
				SC_ERR("file '%s' is empty", SILICONFILE);
				return -1;
			}

			(void) strncpy(Revision, Buffer, STRLEN_MAX);
		} else {
			if (Check_Config_File("Silicon_Revision", Config_Var, &Found) != 0) {
				return -1;
			}

			if (Found && (0 == atoi(Config_Var))) {
				SC_INFO("ignored getting silicon revision");
				return 0;
			}

			if (Get_IDCODE(Revision, STRLEN_MAX) != 0) {
				SC_ERR("failed to get silicon revision");
				return -1;
			}

			if ((strstr(Revision, "ES1") == NULL) &&
			    (strstr(Revision, "PROD") == NULL)) {
				SC_INFO("failed to get an expected silicon revision");
				Revision[0] = 0;
				return 0;
			}

			(void) strtok(Revision, "\n");

			FP = fopen(SILICONFILE, "w");
			if (FP == NULL) {
				SC_ERR("failed to write file %s: %m", SILICONFILE);
				return -1;
			}

			if (fputs(Revision, FP) == EOF) {
				(void) fclose(FP);
				SC_ERR("failed to store silicon revision");
				return -1;
			}
		}

		(void) fclose(FP);
		SC_INFO("Silicon Revision: %s", Revision);
	}

	if (Identify_PDI(Revision) != 0) {
		SC_ERR("failed to identify PDI");
		return -1;
	}

	return 0;
}

/*
 * Print silicon revision, IDCODE, and DNA for geteeprom summary.
 *
 * Ensures the PDI revision in line 1 of the silicon file is present,
 * runs Silicon_Identification() if the file is incomplete, then prints
 * lines 2-4 (Silicon Revision, IDCODE, and DNA).
 */
int
Print_Silicon_Info(void)
{
	FILE *FP;
	char System_Cmd[SYSCMD_MAX];
	char Buffer[STRLEN_MAX];

	if (Get_Silicon_Revision(Silicon_Revision) != 0) {
		return -1;
	}

	if (!Silicon_File_Complete()) {
		if (Silicon_Identification() != 0) {
			return -1;
		}
	}

	(void) sprintf(System_Cmd, "tail -3 %s", SILICONFILE);
	FP = popen(System_Cmd, "r");
	if (FP == NULL) {
		SC_ERR("failed to read silicon info");
		return -1;
	}

	while (fgets(Buffer, sizeof(Buffer), FP) != NULL) {
		(void) strtok(Buffer, "\n");
		SC_PRINT("%s", Buffer);
	}

	if (pclose(FP) != 0) {
		SC_ERR("failed to read silicon info");
		return -1;
	}

	return 0;
}

int
Shell_Execute(char *Command)
{
	FILE *FP;

	FP = popen(Command, "r");
	if (FP == NULL) {
		SC_ERR("failed to invoke '%s': %m", Command);
		return -1;
	}

	SC_INFO("Shell Command: %s", Command);
	return pclose(FP);
}

int
Access_Regulator(Voltage_t *Regulator, float *Voltage, int Access)
{
	int FD;
	char In_Buffer[STRLEN_MAX];
	char Out_Buffer[STRLEN_MAX];
	signed int Exponent;
	short Mantissa;
	int Get_Vout_Mode = 1;
	int Ret = 0;
	float Current_Voltage, New_Voltage;
	int Direction, Register;
	unsigned int Value;
	unsigned int Data_Format = 0;

	if (Regulator == NULL) {
		SC_ERR("Regulator pointer is null!");
		return -1;
	}

	/* Check if setting the requested voltage is within range */
	if ((1 == Access) && (Regulator->Minimum_Volt != -1) &&
	    (Regulator->Maximum_Volt != -1)) {
		if ((*Voltage < Regulator->Minimum_Volt) ||
		    (*Voltage > Regulator->Maximum_Volt)) {
			SC_ERR("valid voltage range is %.2f V - %.2f V",
				Regulator->Minimum_Volt, Regulator->Maximum_Volt);
			return -1;
		}
	}

	FD = open(Regulator->I2C_Bus, O_RDWR);
	if (FD < 0) {
		SC_ERR("unable to access the I2C bus %s: %m", Regulator->I2C_Bus);
		return -1;
	}

	/* Select the page, if the voltage regulator supports it */
	if (Regulator->Page_Select != -1) {
		Out_Buffer[0] = 0x0;
		Out_Buffer[1] = Regulator->Page_Select;
		SC_INFO("Write to select page: 0x%x%x", Out_Buffer[0],
			Out_Buffer[1]);
		I2C_WRITE(FD, Regulator->I2C_Address, 2, Out_Buffer, Ret);
		if (Ret != 0) {
			(void) close(FD);
			return Ret;
		}
	}

	/*
	 * Reading VOUT_MODE indicates what is READ_VOUT format and
	 * its exponent.  The default format is Linear16:
	 *
	 * Voltage =  Mantissa * 2 ^ -(Exponent)
	 */

	/* Regulators that don't support VOUT_MODE PMBus command */
	if (!Regulator->PMBus_VOUT_MODE) {
		Get_Vout_Mode = 0;
	}

	if (1 == Get_Vout_Mode) {
		Out_Buffer[0] = PMBUS_VOUT_MODE;
		(void) memset(In_Buffer, 0, STRLEN_MAX);
		I2C_READ(FD, Regulator->I2C_Address, 1, Out_Buffer, In_Buffer, Ret);
		if (Ret != 0) {
			(void) close(FD);
			return Ret;
		}

		SC_INFO("VOUT_MODE: %#x", In_Buffer[0]);
		Data_Format = ((In_Buffer[0] & 0x80) >> 7);
		Exponent = (In_Buffer[0] & 0x1F) - (sizeof(int) * 8);

	} else {
		/* For non-compliant regulators, use exponent value -8 */
		Exponent = -8;
	}

	/* Get the current VOUT */
	Out_Buffer[0] = PMBUS_READ_VOUT;
	(void) memset(In_Buffer, 0, STRLEN_MAX);
	I2C_READ(FD, Regulator->I2C_Address, 2, Out_Buffer, In_Buffer, Ret);
	if (Ret != 0) {
		(void) close(FD);
		return Ret;
	}

	Mantissa = ((unsigned char)In_Buffer[1] << 8) | (unsigned char)In_Buffer[0];
	Current_Voltage = Mantissa * pow(2, Exponent);
	SC_INFO("Current Voltage(V): %.2f, Mantissa: %#x, Exponent: %#x",
		Current_Voltage, Mantissa, Exponent);

	switch (Access) {
	case 0:
		*Voltage = Current_Voltage;
		break;
	case 1:
		/* 1: voltage is increasing, 0: voltage is decreasing */
		Direction = (*Voltage > Current_Voltage) ? 1 : 0;

		/* Disable VOUT */
		Out_Buffer[0] = PMBUS_OPERATION;
		Out_Buffer[1] = 0x0;
		SC_INFO("OPERATION: %#x %#x", Out_Buffer[0], Out_Buffer[1]);
		I2C_WRITE(FD, Regulator->I2C_Address, 2, Out_Buffer, Ret);
		if (Ret != 0) {
			(void) close(FD);
			return Ret;
		}

		/*
		 * 1: relative data format, 0: absolute data format
		 * Relative data format allows hardware to manage OV/UV limits.
		 * Absolute data format requires software to update OV/UV limits.
		 */
		if (0 == Data_Format) {
			/* Set the fault limit to +/- 10% of new VOUT */
			if (Direction) {
				New_Voltage = *Voltage + (*Voltage * 0.1);
				Register = PMBUS_VOUT_OV_FAULT_LIMIT;
			} else {
				New_Voltage = *Voltage - (*Voltage * 0.1);
				New_Voltage = (New_Voltage < 0) ? 0 : New_Voltage;
				Register = PMBUS_VOUT_UV_FAULT_LIMIT;
			}

			SC_INFO("Adjusted %svoltage Fault Limit(V):\t%.2f",
				((Direction) ? "Over" : "Under"), New_Voltage);

			/* Get the current fault limit */
			Out_Buffer[0] = Register;
			(void) memset(In_Buffer, 0, STRLEN_MAX);
			I2C_READ(FD, Regulator->I2C_Address, 2, Out_Buffer, In_Buffer, Ret);
			if (Ret != 0) {
				(void) close(FD);
				return Ret;
			}

			Mantissa = ((unsigned char)In_Buffer[1] << 8) |
					(unsigned char)In_Buffer[0];
			Current_Voltage = Mantissa * pow(2, Exponent);
			SC_INFO("Current %svoltage Fault Limit(V): %.2f, Mantissa: %#x, \
				Exponent: %#x", ((Direction) ? "Over" : "Under"),
				Current_Voltage, Mantissa, Exponent);

			/* Adjust the limit register only if it is needed */
			if (((Direction == 1) && (Current_Voltage < New_Voltage)) ||
				((Direction == 0) && (Current_Voltage > New_Voltage))) {
				Value = round(New_Voltage / pow(2, Exponent));
				SC_INFO("New %svoltage Fault Limit(V):\t%.2f\t(Reg 0x%x:\t0x%x)",
					((Direction) ? "Over" : "Under"),
					New_Voltage, Register, Value);
				Out_Buffer[0] = Register;
				Out_Buffer[1] = Value & 0xFF;
				Out_Buffer[2] = Value >> 8;
				SC_INFO("Write %svoltage Fault Limit: %#x %#x %#x",
					((Direction) ? "Over" : "Under"), Out_Buffer[0],
					Out_Buffer[1], Out_Buffer[2]);
				I2C_WRITE(FD, Regulator->I2C_Address, 3, Out_Buffer, Ret);
				if (Ret != 0) {
					(void) close(FD);
					return Ret;
				}
			}

			/* Set the warning limit to +/- 3% of new VOUT*/
			if (Direction) {
				New_Voltage = *Voltage + (*Voltage * 0.03);
				Register = PMBUS_VOUT_OV_WARN_LIMIT;
			} else {
				New_Voltage = *Voltage - (*Voltage * 0.03);
				New_Voltage = (New_Voltage < 0) ? 0 : New_Voltage;
				Register = PMBUS_VOUT_UV_WARN_LIMIT;
			}

			SC_INFO("Adjusted %svoltage Warn Limit(V):\t%.2f",
				((Direction) ? "Over" : "Under"), New_Voltage);

			/* Get the current warning limit */
			Out_Buffer[0] = Register;
			(void) memset(In_Buffer, 0, STRLEN_MAX);
			I2C_READ(FD, Regulator->I2C_Address, 2, Out_Buffer, In_Buffer, Ret);
			if (Ret != 0) {
				(void) close(FD);
				return Ret;
			}

			Mantissa = ((unsigned char)In_Buffer[1] << 8) |
					(unsigned char)In_Buffer[0];
			Current_Voltage = Mantissa * pow(2, Exponent);
			SC_INFO("Current %svoltage Warn Limit(V): %.2f, Mantissa: %#x, \
				Exponent: %#x", ((Direction) ? "Over" : "Under"),
				Current_Voltage, Mantissa, Exponent);

			/* Adjust the limit register only if it is needed */
			if (((Direction == 1) && (Current_Voltage < New_Voltage)) ||
				((Direction == 0) && (Current_Voltage > New_Voltage))) {
				Value = round(New_Voltage / pow(2, Exponent));
				SC_INFO("New %svoltage Warn Limit(V):\t%.2f\t(Reg 0x%x:\t0x%x)",
					((Direction) ? "Over" : "Under"),
					New_Voltage, Register, Value);
				Out_Buffer[0] = Register;
				Out_Buffer[1] = Value & 0xFF;
				Out_Buffer[2] = Value >> 8;
				SC_INFO("Write %svoltage Warn Limit: %#x %#x %#x",
					((Direction) ? "Over" : "Under"), Out_Buffer[0],
					Out_Buffer[1], Out_Buffer[2]);
				I2C_WRITE(FD, Regulator->I2C_Address, 3, Out_Buffer, Ret);
				if (Ret != 0) {
					(void) close(FD);
					return Ret;
				}
			}
		}

		/* Set VOUT */
		Value = round(*Voltage / pow(2, Exponent));
		SC_INFO("New Voltage(V):\t%.2f\t(Reg 0x%x:\t0x%x)", *Voltage,
			PMBUS_VOUT_COMMAND, Value);
		Out_Buffer[0] = PMBUS_VOUT_COMMAND;
		Out_Buffer[1] = Value & 0xFF;
		Out_Buffer[2] = Value >> 8;
		SC_INFO("VOUT_COMMAND: %#x %#x %#x", Out_Buffer[0],
			Out_Buffer[1], Out_Buffer[2]);
		I2C_WRITE(FD, Regulator->I2C_Address, 3, Out_Buffer, Ret);
		if (Ret != 0) {
			(void) close(FD);
			return Ret;
		}

		/* Enable VOUT */
		(void) memset(Out_Buffer, 0, STRLEN_MAX);
		Out_Buffer[0] = PMBUS_OPERATION;
		Out_Buffer[1] = 0x80;
		SC_INFO("OPERATION: %#x %#x", Out_Buffer[0], Out_Buffer[1]);
		I2C_WRITE(FD, Regulator->I2C_Address, 2, Out_Buffer, Ret);
		if (Ret != 0) {
			(void) close(FD);
			return Ret;
		}

		break;
	case 2:
		/* Get Overvoltage Fault Limit */
		Out_Buffer[0] = PMBUS_VOUT_OV_FAULT_LIMIT;
		(void) memset(In_Buffer, 0, STRLEN_MAX);
		I2C_READ(FD, Regulator->I2C_Address, 2, Out_Buffer, In_Buffer, Ret);
		if (Ret != 0) {
			(void) close(FD);
			return Ret;
		}

		Mantissa = ((unsigned char)In_Buffer[1] << 8) | (unsigned char)In_Buffer[0];
		*Voltage = Mantissa * pow(2, Exponent);
		if (1 == Data_Format) {
			/*
			 * In relative data format, value calculated from mantissa is the
			 * ratio of the voltage limit to the VOUT value. For actual voltage,
			 * multiply ratio with VOUT value.
			 */
			*Voltage = *Voltage * Current_Voltage;
		}

		SC_PRINT("Overvoltage Fault Limit(V):\t%.2f\t(Reg 0x%x:\t0x%x)",
		       *Voltage, PMBUS_VOUT_OV_FAULT_LIMIT, Mantissa);

		/* Get Overvoltage Warning Limit */
		Out_Buffer[0] = PMBUS_VOUT_OV_WARN_LIMIT;
		(void) memset(In_Buffer, 0, STRLEN_MAX);
		I2C_READ(FD, Regulator->I2C_Address, 2, Out_Buffer, In_Buffer, Ret);
		if (Ret != 0) {
			(void) close(FD);
			return Ret;
		}

		Mantissa = ((unsigned char)In_Buffer[1] << 8) | (unsigned char)In_Buffer[0];
		*Voltage = Mantissa * pow(2, Exponent);
		if (1 == Data_Format) {
			*Voltage = *Voltage * Current_Voltage;
		}

		SC_PRINT("Overvoltage Warning Limit(V):\t%.2f\t(Reg 0x%x:\t0x%x)",
		       *Voltage, PMBUS_VOUT_OV_WARN_LIMIT, Mantissa);

		/* Get Undervoltage Warning Limit */
		Out_Buffer[0] = PMBUS_VOUT_UV_WARN_LIMIT;
		(void) memset(In_Buffer, 0, STRLEN_MAX);
		I2C_READ(FD, Regulator->I2C_Address, 2, Out_Buffer, In_Buffer, Ret);
		if (Ret != 0) {
			(void) close(FD);
			return Ret;
		}

		Mantissa = ((unsigned char)In_Buffer[1] << 8) | (unsigned char)In_Buffer[0];
		*Voltage = Mantissa * pow(2, Exponent);
		if (1 == Data_Format) {
			*Voltage = *Voltage * Current_Voltage;
		}

		SC_PRINT("Undervoltage Warning Limit(V):\t%.2f\t(Reg 0x%x:\t0x%x)",
		       *Voltage, PMBUS_VOUT_UV_WARN_LIMIT, Mantissa);

		/* Get Undervoltage Fault Limit */
		Out_Buffer[0] = PMBUS_VOUT_UV_FAULT_LIMIT;
		(void) memset(In_Buffer, 0, STRLEN_MAX);
		I2C_READ(FD, Regulator->I2C_Address, 2, Out_Buffer, In_Buffer, Ret);
		if (Ret != 0) {
			(void) close(FD);
			return Ret;
		}

		Mantissa = ((unsigned char)In_Buffer[1] << 8) | (unsigned char)In_Buffer[0];
		*Voltage = Mantissa * pow(2, Exponent);
		if (1 == Data_Format) {
			*Voltage = *Voltage * Current_Voltage;
		}

		SC_PRINT("Undervoltage Fault Limit(V):\t%.2f\t(Reg 0x%x:\t0x%x)",
		       *Voltage, PMBUS_VOUT_UV_FAULT_LIMIT, Mantissa);
		break;
	default:
		SC_ERR("invalid regulator access");
		(void) close(FD);
		return -1;
	}

	(void) close(FD);
	return 0;
}

/*
 * Routine to access IO expander chip.
 *
 * Input -
 *      IO_Exp: Pointer to IO expander structure.
 *      Op:     0 for read operation, 1 for write operation.
 *      Offset: 0x2 output register offset, 0x6 direction register offset.
 *      *Data:  Pointer to output value to be written to the device.
 * Output -
 *      *Data:  Pointer to input value read from the device.
 */
int
Access_IO_Exp(IO_Exp_t *IO_Exp, int Op, int Offset, unsigned int *Data)
{
	int FD;
	char In_Buffer[STRLEN_MAX];
	char Out_Buffer[STRLEN_MAX];
	int Ret = 0;

	if (IO_Exp == NULL) {
		SC_ERR("IO expander is not defined");
		return -1;
	}

	if (Op == 1 && *Data > ((1 << IO_Exp->Numbers) - 1)) {
		SC_ERR("invalid data, valid value is between 0 and %#x",
		       ((1 << IO_Exp->Numbers) - 1));
		return -1;
	}

	FD = open(IO_Exp->I2C_Bus, O_RDWR);
	if (FD < 0) {
		SC_ERR("unable to access I2C bus %s: %m", IO_Exp->I2C_Bus);
		return -1;
	}

	(void) memset(Out_Buffer, 0, STRLEN_MAX);
	(void) memset(In_Buffer, 0, STRLEN_MAX);
	if (Op == 0) {	// Read operation
		Out_Buffer[0] = Offset;
		I2C_READ(FD, IO_Exp->I2C_Address, 2, Out_Buffer, In_Buffer, Ret);
		if (Ret != 0) {
			(void) close(FD);
			return Ret;
		}

		SC_INFO("Read (%#x): %#x %#x", Offset, In_Buffer[0],
			In_Buffer[1]);
		*Data = ((In_Buffer[0] << 8) | In_Buffer[1]);

	} else if (Op == 1) {	// Write operation
		Out_Buffer[0] = Offset;
		Out_Buffer[1] = ((*Data >> 8) & 0xFF);
		Out_Buffer[2] = (*Data & 0xFF);
		SC_INFO("Write (%#x): %#x %#x", Offset, Out_Buffer[1],
			Out_Buffer[2]);
		I2C_WRITE(FD, IO_Exp->I2C_Address, 3, Out_Buffer, Ret);
		if (Ret != 0) {
			(void) close(FD);
			return Ret;
		}

	} else {
		SC_ERR("invalid access operation");
		(void) close(FD);
		return -1;
	}

	(void) close(FD);
	return 0;
}

void
FMC_Access(FMC_t *FMC, bool State) {
	int Level;

	if (FMC->Access_Label == 0) {
		return;
	}

#if !defined (LIBGPIOD_V1)
	SC_INFO("%s access to FMC", (State == true) ? "enable" : "disable");
	if (State) {
		Level = FMC->Access_Level;
		if (Set_GPIO(FMC->Access_Label, FMC->Access_Level) != 0) {
			SC_ERR("failed to set '%s' to %d", FMC->Access_Label, Level);
		}
	} else {
		if (Get_GPIO(FMC->Access_Label, &Level, GPIOD_LINE_DIRECTION_INPUT) != 0) {
			SC_ERR("failed to reset '%s'", FMC->Access_Label);
		}
	}
#else
	if (State) {
		Level = FMC->Access_Level;
	} else {
		Level = (~FMC->Access_Level & 0x1);
	}

	SC_INFO("%s access to FMC", (State == true) ? "enable" : "disable");
	if (Set_GPIO(FMC->Access_Label, Level) != 0) {
		SC_ERR("failed to set '%s' to %d", FMC->Access_Label, Level);
	}
#endif
}

int
FMC_Vadj_Range(FMC_t *FMC, float *Min_Voltage, float *Max_Voltage)
{
	char In_Buffer[SYSCMD_MAX];
	char Out_Buffer[SYSCMD_MAX];
	int FD;
	int Offset;
	int Found;
	int Ret = 0;

	FMC_Access(FMC, true);

	/* Read FMC's EEPROM */
	FD = open(FMC->I2C_Bus, O_RDWR);
	if (FD < 0) {
		FMC_Access(FMC, false);
		SC_ERR("unable to access I2C bus %s: %m", FMC->I2C_Bus);
		return -1;
	}

	(void) memset(Out_Buffer, 0, SYSCMD_MAX);
	(void) memset(In_Buffer, 0, SYSCMD_MAX);
	Out_Buffer[0] = 0x0;    // EEPROM offset 0
	I2C_READ(FD, FMC->I2C_Address, 0xFF, Out_Buffer, In_Buffer, Ret);
	if (Ret != 0) {
		FMC_Access(FMC, false);
		(void) close(FD);
		return Ret;
	}

	/* Common Header offset 0x5 points to Multirecord areas */
	Offset = In_Buffer[5] * 8;

	/*
	 * 'Record Type' for DC Load is 0x2, bit 7 of 'Record Format' indicates
	 * the end of Multirecord, and don't go over amount of data read from
	 * EEPROM.
	 */ 
	Found = 0;
	while ((In_Buffer[Offset] == 0x2) &&
	       ((In_Buffer[Offset + 1] & 0x80) != 0x80) && (Offset < 0xFF)) {
		/*
		 * In Multirecord area of DC Load, 'Output Number' (Offset + 5)
		 * should have value of 0 (for Vadj).  Other values belong
		 * to other power supplies.
		 */
		if (In_Buffer[Offset + 5] == 0x0) {
			Found = 1;
			break;
		}

		/*
		 * Skip to the next DC Load record.  There are 5 bytes of
		 * header in this record plus length of data in offset 0x2.
		 */
		Offset += (5 + In_Buffer[Offset + 2]);
	}

	if (Found == 1) {
		/* Unit of reading is per 10mV */
		*Min_Voltage = (float)((In_Buffer[Offset + 9] << 8) |
					In_Buffer[Offset + 8]) / 100;
		*Max_Voltage = (float)((In_Buffer[Offset + 11] << 8) |
					In_Buffer[Offset + 10]) / 100;
	} else {
		*Min_Voltage = *Max_Voltage = 0;
	}

	SC_INFO("Min Voltage: %.2f, Max Voltage: %.2f", *Min_Voltage,
		*Max_Voltage);

	FMC_Access(FMC, false);

	return 0;
}

int
FMCAutoVadj_Op(void)
{
	int Target_Index = -1;
	IO_Exp_t *IO_Exp;
	unsigned int Value;
	Voltages_t *Voltages;
	Voltage_t *Regulator;
	FMCs_t *FMCs;
	FMC_t *FMC;
	int Present[2] = { 0 };
	float Voltage = 0;
	float Min_Volt[2] = { 0 };
	float Max_Volt[2] = { 0 };
	float Min_Combined = 0;
	float Max_Combined = 0;
	int Legacy_Approach = 0;
	int State;
	char *Regulator_Name;
	float Target_Volt, Default_Volt;
	int Use_RAFT;
	char System_Cmd[SYSCMD_MAX];

	FMCs = Plat_Devs->FMCs;
	if (FMCs == NULL) {
		SC_INFO("FMC operation is not supported");
		return 0;
	}

	/* Current boards support up to 2 FMC modules */
	if (FMCs->Numbers > 2) {
		SC_ERR("unsupported number of FMC modules");
		return -1;
	}

	/* Determine if gpiod knows about 'presence' lines */
	for (int i = 0; i < FMCs->Numbers; i++) {
		FMC = &FMCs->FMC[i];
		for (int j = 0; j < FMC->Label_Numbers; j++) {
#if !defined (LIBGPIOD_V1)
			if (Get_GPIO(FMC->Presence_Labels[j], &State, GPIOD_LINE_DIRECTION_INPUT) != 0) {
#else
			if (Get_GPIO(FMC->Presence_Labels[j], &State) != 0) {
#endif
				Legacy_Approach = 1;
				break;
			}
		}
	}

	if (Legacy_Approach == 1) {
		SC_INFO("Read IO Expander to determine FMC presence");

		IO_Exp = Plat_Devs->IO_Exp;
		if (IO_Exp == NULL) {
			SC_ERR("FMC voltage can not be adjusted");
			return 0;
		}

		if (Access_IO_Exp(IO_Exp, 0, 0x0, &Value) != 0) {
			SC_ERR("failed to read input of IO Expander");
			return -1;
		}

		SC_INFO("IO Expander input: %#x", Value);

		/*
		 * Presence of FMC modules are detected by input lines connected
		 * to pins 13 & 14 of TCA6416A.  If direction of these pins (Directions
		 * index 15 & 14) are configured as input and they are driven low
		 * (reading value 0), then the FMC module is present.
		 */
		if ((IO_Exp->Directions[15] == 1) && ((~Value & 0x1) == 0x1)) {
			SC_INFO("FMC 0 is present");
			Present[0] = 1;
		}

		if ((IO_Exp->Directions[14] == 1) && ((~Value & 0x2) == 0x2)) {
			SC_INFO("FMC 1 is present");
			Present[1] = 1;
		}
	} else {
		SC_INFO("Read GPIO line to determine FMC presence");
		for (int i = 0; i < FMCs->Numbers; i++) {
			FMC = &FMCs->FMC[i];
#if !defined (LIBGPIOD_V1)
			if (Get_GPIO(FMC->Presence_Labels[0], &State, GPIOD_LINE_DIRECTION_INPUT) != 0) {
#else
			if (Get_GPIO(FMC->Presence_Labels[0], &State) != 0) {
#endif
				SC_ERR("failed to determine presence of FMC %d", i);
				return -1;
			}

			/* Presence line is active low */
			Present[i] = !State;
			SC_INFO("FMC %d is %spresent", i, (Present[i] ? "" : "not "));
		}
	}

	/* Some boards may not have a software-controllable voltage regulator */
	if (FMCs->FMC[0].Voltage_Regulator == NULL) {
		SC_PRINT("NOTE: FMC is not powered by a software-controllable voltage regulator");
		return 0;
	}

	/*
	 * Assumption here is that both FMC modules are powered by the same
	 * voltage regulator and with the same voltage requirement.
	 */
	Regulator_Name = FMCs->FMC[0].Voltage_Regulator;
	Default_Volt = FMCs->FMC[0].Default_Volt;

	for (int i = 0; i < FMCs->Numbers; i++) {
		if (Present[i] == 1) {
			FMC = &FMCs->FMC[i];
			if (FMC_Vadj_Range(FMC, &Min_Volt[i], &Max_Volt[i]) != 0) {
				SC_PRINT("WARNING: unable to obtain voltage range "
					 "from FMC %d.  Voltage regulator '%s' "
					 "needs to be set manually.", i, Regulator_Name);
				return 0;
			}
		}
	}

	if (Present[0] == 1 && Present[1] == 0) {
		Min_Combined = Min_Volt[0];
		Max_Combined = Max_Volt[0];
	} else if (Present[0] == 0 && Present[1] == 1) {
		Min_Combined = Min_Volt[1];
		Max_Combined = Max_Volt[1];
	} else if (Present[0] == 1 && Present[1] == 1) {
		Min_Combined = MAX(Min_Volt[0], Min_Volt[1]);
		Max_Combined = MIN(Max_Volt[0], Max_Volt[1]);
	}

	SC_INFO("Combined Min: %.2f, Combined Max: %.2f",
		Min_Combined, Max_Combined);

	/*
	 * Both FMC modules are constrained to the same target voltage
	 * restriction defined by the board.
	 */
	for (int i = 0; i < FMCs->FMC[0].Volt_Numbers; i++) {
		Target_Volt = FMCs->FMC[0].Supported_Volts[i];
		if (Min_Combined <= Target_Volt && Max_Combined >= Target_Volt) {
			Voltage = Target_Volt;
		}
	}

	/*
	 * If no FMC module is present, or voltage range that is read from the
	 * modules don't satisfy any of target voltages supported by the board,
	 * then set the regulator to the default voltage level.
	 */
	if ((Present[0] == 0 && Present[1] == 0) || (Voltage == 0)) {
		Voltage = Default_Volt;
	}

	Use_RAFT = 1;
	for (int i = 0; i < Plat_Devs->FeatureList->Numbers; i++) {
		if (strcmp(Plat_Devs->FeatureList->Feature[i], "voltage") == 0) {
			Use_RAFT = 0;
			break;
		}
	}

	if (Use_RAFT) {
		(void) sprintf(System_Cmd, "python3 %s setvoltage %s %f >/dev/null", RAFT_CLI,
			       Regulator_Name, Voltage);
		SC_INFO("Invoke RAFT command '%s'", System_Cmd);
		if (Shell_Execute(System_Cmd) != 0) {
			SC_ERR("failed to set voltage of %s regulator", Regulator_Name);
		}

		return 0;
	}

	Voltages  = Plat_Devs->Voltages;
	for (int i = 0; i < Voltages->Numbers; i++) {
		if (strcmp(Regulator_Name, Voltages->Voltage[i].Name) == 0) {
			Target_Index = i;
			Regulator = &Voltages->Voltage[Target_Index];
			break;
		}
	}

	if (Target_Index == -1) {
		SC_ERR("no regulator exists for %s", Regulator_Name);
		return -1;
	}

	SC_INFO("Set %s voltage regulator to %.2f volts", Regulator_Name, Voltage);
	if (Access_Regulator(Regulator, &Voltage, 1) != 0) {
		SC_ERR("failed to set voltage of %s regulator", Regulator_Name);
	}

	return 0;
}

#if !defined (LIBGPIOD_V1)
static int
Find_GPIO_Line(const char *Label, unsigned int *Offset, struct gpiod_chip **Chip)
{
	char Chip_Path[STRLEN_MAX];
	unsigned int i = 0;

	while (i < ITEMS_MAX) {
		(void) snprintf(Chip_Path, STRLEN_MAX, "/dev/gpiochip%d", i);
		if (access(Chip_Path, F_OK) != 0) {
			SC_INFO("couldn't find label %s in any GPIO chip", Label);
			return -1;
		}

		if (!gpiod_is_gpiochip_device(Chip_Path)) {
			SC_INFO("invalid GPIO chip path %s", Chip_Path);
			return -1;
		}

		*Chip = gpiod_chip_open(Chip_Path);
		if (*Chip == NULL) {
			SC_INFO("failed to open GPIO chip");
			return -1;
		}

		*Offset = gpiod_chip_get_line_offset_from_name(*Chip, Label);
		if (*Offset != -1) {
			break;
		}

		gpiod_chip_close(*Chip);
		i++;
	}

	if (i == ITEMS_MAX) {
		SC_INFO("couldn't find label %s due to running out-of-range!", Label);
		return -1;
	}

	SC_INFO("label %s is offset %d in %s", Label, *Offset, Chip_Path);
	return 0;
}

static int
Request_GPIO_Line(struct gpiod_chip *Chip, unsigned int Offset, enum gpiod_line_direction Direction,
		  enum gpiod_line_value Value, struct gpiod_line_request **Request)
{
	struct gpiod_line_settings *Settings;
	struct gpiod_request_config *Request_Config;
	struct gpiod_line_config *Line_Config;
	int Ret = -1;

	Settings = gpiod_line_settings_new();
	if (Settings == NULL) {
		SC_INFO("failed to obtain new GPIO settings: %m");
		return -1;
	}

	if (gpiod_line_settings_set_direction(Settings, Direction) != 0) {
		SC_INFO("failed to set GPIO direction: %m");
		gpiod_line_settings_free(Settings);
		return -1;
	}

	Request_Config = gpiod_request_config_new();
	if (Request_Config == NULL) {
		SC_INFO("unable to allocate the request config structure: %m");
		gpiod_line_settings_free(Settings);
		return -1;
	}

	gpiod_request_config_set_consumer(Request_Config, "sc_appd");

	Line_Config = gpiod_line_config_new();
	if (Line_Config == NULL) {
		SC_INFO("failed to create a new line config: %m");
		gpiod_line_settings_free(Settings);
		gpiod_request_config_free(Request_Config);
		return -1;
	}

	if (gpiod_line_config_add_line_settings(Line_Config, &Offset, 1, Settings) != 0) {
		SC_INFO("call to 'gpiod_line_config_add_line_settings' failed: %m");
		goto Request_Free;
	}

	if ((Direction == GPIOD_LINE_DIRECTION_OUTPUT) &&
	    (gpiod_line_config_set_output_values(Line_Config, &Value, 1) != 0)) {
		SC_INFO("unable to set output value: %m");
		goto Request_Free;
	}

	*Request = gpiod_chip_request_lines(Chip, Request_Config, Line_Config);
	if (*Request == NULL) {
		SC_INFO("failed gpiod_chip_request_lines: %m");
		goto Request_Free;
	}

	Ret = 0;

Request_Free:
	gpiod_line_settings_free(Settings);
	gpiod_request_config_free(Request_Config);
	gpiod_line_config_free(Line_Config);

	return Ret;
}
#endif

#if !defined (LIBGPIOD_V1)
int
Get_GPIO(char *Label, int *State, enum gpiod_line_direction Direction)
{
	struct gpiod_chip *Chip;
	struct gpiod_line_request *Request;
	unsigned int Line_Offset;

	if (Find_GPIO_Line(Label, &Line_Offset, &Chip) != 0) {
		SC_INFO("failed to find GPIO line %s", Label);
		return -1;
	}

	if (Request_GPIO_Line(Chip, Line_Offset, Direction, 0, &Request) != 0) {
		SC_INFO("failed to request GPIO line %s", Label);
		gpiod_chip_close(Chip);
		return -1;
	}

	*State = gpiod_line_request_get_value(Request, Line_Offset);
	SC_INFO("state of GPIO line %s is %d", Label, *State);

	gpiod_line_request_release(Request);
	gpiod_chip_close(Chip);
	return 0;
}
#else
int
Get_GPIO(char *Label, int *State)
{
	FILE *FP;
	char Chip_Name[STRLEN_MAX];
	char Buffer[SYSCMD_MAX];
	char Output[STRLEN_MAX] = {'\0'};
	unsigned int Line_Offset;

	if (gpiod_ctxless_find_line(Label, Chip_Name, STRLEN_MAX,
	    &Line_Offset) != 1) {
		SC_INFO("failed to find GPIO line %s", Label);
		return -1;
	}

	(void) sprintf(Buffer, "gpioget %s %u 2>&1", Chip_Name, Line_Offset);
	SC_INFO("Command: %s", Buffer);
	FP = popen(Buffer, "r");
	if (FP == NULL) {
		SC_ERR("failed to start process '%s': %m", Buffer);
		return -1;
	}

	if (fgets(Output, sizeof(Output), FP) == NULL) {
		SC_ERR("failed to get the state of GPIO line '%s'", Label);
		(void) pclose(FP);
		return -1;
	}

	if (pclose(FP) != 0) {
		SC_ERR("failed to get the state of GPIO line '%s'", Label);
		return -1;
	}

	SC_INFO("Output: %s", Output);
	if ((strcmp(Output, "0\n") != 0) && (strcmp(Output, "1\n") != 0)) {
		SC_ERR("invalid output %s", Output);
		return -1;
	}

	*State = atoi(Output);
	return 0;
}
#endif

int
Set_GPIO(char *Label, int State)
{
#if !defined (LIBGPIOD_V1)
	struct gpiod_chip *Chip;
	struct gpiod_line_request *Request;
	enum gpiod_line_value Value;
	unsigned int Line_Offset;

	if (Find_GPIO_Line(Label, &Line_Offset, &Chip) != 0) {
		SC_INFO("failed to find GPIO line %s", Label);
		return -1;
	}

	Value = (0 == State) ? GPIOD_LINE_VALUE_INACTIVE : GPIOD_LINE_VALUE_ACTIVE;
	if (Request_GPIO_Line(Chip, Line_Offset, GPIOD_LINE_DIRECTION_OUTPUT, Value,
			      &Request) != 0) {
		SC_INFO("failed to request GPIO line %s", Label);
		gpiod_chip_close(Chip);
		return -1;
	}

	gpiod_line_request_release(Request);
	gpiod_chip_close(Chip);
#else
	char Chip_Name[STRLEN_MAX];
	char Buffer[SYSCMD_MAX];
	unsigned int Line_Offset;

	if (gpiod_ctxless_find_line(Label, Chip_Name, STRLEN_MAX,
	    &Line_Offset) != 1) {
		SC_ERR("failed to find GPIO line");
		return -1;
	}

	(void) sprintf(Buffer, "gpioset %s %u=%d 2>&1", Chip_Name, Line_Offset,
		       State);
	if (Shell_Execute(Buffer) != 0) {
		SC_ERR("failed to set GPIO line '%s': %m", Label);
		return -1;
	}
#endif
	return 0;
}

int
EEPROM_Common(char *Buffer)
{
	SC_PRINT("0x00 - Version:\t%.2x", Buffer[0x0]);
	SC_PRINT("0x01 - Internal User Area:\t%.2x", Buffer[0x1]);
	SC_PRINT("0x02 - Chassis Info Area:\t%.2x", Buffer[0x2]);
	SC_PRINT("0x03 - Board Area:\t%.2x", Buffer[0x3]);
	SC_PRINT("0x04 - Product Info Area:\t%.2x", Buffer[0x4]);
	SC_PRINT("0x05 - Multi Record Area:\t%.2x", Buffer[0x5]);
	SC_PRINT("0x06 - Pad and Check sum:\t%.2x %.2x", Buffer[0x6],
		 Buffer[0x7]);
	return 0;
}

int
EEPROM_Board(char *Buffer, int PCIe)
{
	char Buf[STRLEN_MAX];
	struct tm BuildDate = { 0 };
	time_t Time;
	int Offset, Length;

	SC_PRINT("0x08 - Version:\t%.2x", Buffer[0x8]);
	SC_PRINT("0x09 - Length:\t%.2x", Buffer[0x9]);
	SC_PRINT("0x0A - Language Code:\t%.2x", Buffer[0xA]);

	/* Base build date for manufacturing is 1/1/1996 */
	BuildDate.tm_year = 96;
	BuildDate.tm_mday = 1;
	BuildDate.tm_min = (Buffer[0xD] << 16 | Buffer[0xC] << 8 |
			    Buffer[0xB]);
	Time = mktime(&BuildDate);
	if (Time == -1) {
		SC_ERR("invalid manufacturing date");
		return -1;
	}

	SC_PRINT_N("0x0B - Manufacturing Date:\t%s", ctime(&Time));
	Offset = 0xE;
	Length = (Buffer[Offset] & 0x3F);
	snprintf(Buf, Length + 1, "%s", &Buffer[Offset + 1]);
	SC_PRINT("0x%.2x - Manufacturer:\t%s", (Offset + 1), Buf);
	Offset = Offset + Length + 1;
	Length = (Buffer[Offset] & 0x3F);
	snprintf(Buf, Length + 1, "%s", &Buffer[Offset + 1]);
	SC_PRINT("0x%.2x - Product Name:\t%s", (Offset + 1), Buf);
	Offset = Offset + Length + 1;
	Length = (Buffer[Offset] & 0x3F);
	snprintf(Buf, Length + 1, "%s", &Buffer[Offset + 1]);
	SC_PRINT("0x%.2x - Serial Number:\t%s", (Offset + 1), Buf);
	Offset = Offset + Length + 1;
	Length = (Buffer[Offset] & 0x3F);
	snprintf(Buf, Length + 1, "%s", &Buffer[Offset + 1]);
	SC_PRINT("0x%.2x - Part Number:\t%s", (Offset + 1), Buf);
	Offset = Offset + Length + 1;
	Length = (Buffer[Offset] & 0x3F);
	/* Based on which length type is used, print the FRU ID */
	if ((Buffer[Offset] & 0xC0) == 0) {
		if (Length == 1) {
			SC_PRINT("0x%.2x - FRU ID:\t%.2x", (Offset + 1), Buffer[Offset + 1]);
		} else {
			SC_ERR("unexpected length of %d for FRU ID", Length);
			return -1;
		}

	} else {
		snprintf(Buf, Length + 1, "%s", &Buffer[Offset + 1]);
		SC_PRINT("0x%.2x - FRU ID:\t%s", (Offset + 1), Buf);
	}

	Offset = Offset + Length + 1;
	Length = (Buffer[Offset] & 0x3F);
	snprintf(Buf, Length + 1, "%s", &Buffer[Offset + 1]);
	SC_PRINT("0x%.2x - Revision:\t%s", (Offset + 1), Buf);
	Offset = Offset + Length + 1;
	/* Non-PCIe boards have a 'End of Field' value (0xC1) at this Offset */
	if ((PCIe == 1) && (Buffer[Offset] != 0xC1)) {
		Length = (Buffer[Offset] & 0x3F);
		SC_PRINT_N("0x%.2x - PCIe Info:\t", (Offset + 1));
		for (int i = 0; i < Length; i++) {
			SC_PRINT_N("%.2x", Buffer[Offset + i + 1]);
		}

		SC_PRINT_N("\n");
		Offset = Offset + Length + 1;
		Length = (Buffer[Offset] & 0x3F);
		SC_PRINT_N("0x%.2x - UUID:\t", (Offset + 1));
		for (int i = 0; i < Length; i++) {
			SC_PRINT_N("%.2x", Buffer[Offset + i + 1]);
			if (i == 3 || i == 5 || i == 7 || i == 9) {
				SC_PRINT_N("-");
			}
		}

		SC_PRINT_N("\n");
		Offset = Offset + Length + 1;
		SC_PRINT("0x%.2x - EoR and Check sum:\t%.2x %.2x", Offset,
		       Buffer[Offset], Buffer[Offset + 1]);
	} else {
		SC_PRINT("0x%.2x - EoR, Pad, Check sum:\t%.2x %.2x%.2x %.2x",
		       Offset, Buffer[Offset], Buffer[Offset + 1],
		       Buffer[Offset + 2], Buffer[Offset + 3]);
	}

	return 0;
}

int Print_Filter;

#define DC_OUTPUT	0x1
#define DC_LOAD		0x2
#define OEM_D2		0xD2
#define OEM_D3		0xD3
#define OEM_VITA_57_1	0xFA
#define SC_PRINT_F(msg, ...) \
	if (!Print_Filter) { \
		SC_PRINT(msg, ##__VA_ARGS__); \
	}

int
Determine_SC_MAC(void)
{
	FILE *FP;
	char Buffer[STRLEN_MAX];

	(void) sprintf(Buffer, "cat /sys/class/net/end0/address");
	SC_INFO("Command: %s", Buffer);
	FP = popen(Buffer, "r");
	if (FP == NULL) {
		SC_INFO("failed to start process '%s': %m", Buffer);
		return -1;
	}

	if (fgets(Buffer, sizeof(Buffer), FP) == NULL) {
		SC_INFO("failed to get MAC address");
		(void) pclose(FP);
		return -1;
	}

	if (pclose(FP) != 0) {
		SC_INFO("failed to execute MAC address read");
		return -1;
	}

	SC_PRINT_N("SC MAC: %s", Buffer);

	return 0;
}

int
EEPROM_MultiRecord(char *Buffer, int MAC_Address)
{
	int Offset;
	int Type;
	int Last_Record;
	int Length;
	bool SC_MAC_Found = false;

	Print_Filter = (MAC_Address) ? 1 : 0;

	/*
	 * Common header offset 0x5 contains offset in bytes to the Multirecord
	 * areas.  If that value is 0, it indicates that no Multirecord area is
	 * present.
	 */
	Offset = Buffer[5] * 8;
	if (Offset == 0) {
		if (MAC_Address && Determine_SC_MAC() != 0) {
			SC_ERR("failed to determine SC MAC address");
			return -1;
		}

		return 0;
	}

	do {
		Type = Buffer[Offset];
		Last_Record = Buffer[Offset + 1] & 0x80;
		switch (Type) {
		case DC_OUTPUT:
			SC_PRINT_F("0x%.2x - Record Type:\t%.2x (DC Output)", Offset, Type);
			break;
		case DC_LOAD:
			SC_PRINT_F("0x%.2x - Record Type:\t%.2x (DC Load)", Offset, Type);
			break;
		case OEM_D2:
			SC_PRINT_F("0x%.2x - Record Type:\t%.2x (Mac ID)", Offset, Type);
			break;
		case OEM_D3:
			SC_PRINT_F("0x%.2x - Record Type:\t%.2x (Memory)", Offset, Type);
			break;
		case OEM_VITA_57_1:
			SC_PRINT_F("0x%.2x - Record Type:\t%.2x (Vita 57.1)", Offset, Type);
			break;
		default:
			SC_ERR("unsupported multirecord type");
			return -1;
		}

		SC_PRINT_F("0x%.2x - Record Format:\t%.2x", (Offset + 1),
			   Buffer[Offset + 1]);
		SC_PRINT_F("0x%.2x - Length:\t%.2x", (Offset + 2),
			   Buffer[Offset + 2]);
		Length = Buffer[Offset + 2];
		SC_PRINT_F("0x%.2x - Record Check sum:\t%.2x", (Offset + 3),
			   Buffer[Offset + 3]);
		SC_PRINT_F("0x%.2x - Header Check sum:\t%.2x", (Offset + 4),
			   Buffer[Offset + 4]);
		if (Type == OEM_D2 || Type == OEM_D3) {
			SC_PRINT_F("0x%.2x - Xilinx IANA ID:\t%.2x%.2x%.2x", (Offset + 5),
				   Buffer[Offset + 5], Buffer[Offset + 6], Buffer[Offset + 7]);
		}

		switch (Type) {
		case DC_OUTPUT:
			SC_PRINT_F("0x%.2x - Output Number:\t%.2x (Power Rail)",
				   (Offset + 5), Buffer[Offset + 5]);
			SC_PRINT_F("0x%.2x - Nominal Voltage:\t%.2x%.2x (%.2fV)",
				   (Offset + 6), Buffer[Offset + 6], Buffer[Offset + 7],
				   (float)(Buffer[Offset + 7] << 8 | Buffer[Offset + 6]) / 100.0);
			SC_PRINT_F("0x%.2x - Spec'd Min Voltage:\t%.2x%.2x (%.2fV)",
				   (Offset + 8), Buffer[Offset + 8], Buffer[Offset + 9],
				   (float)(Buffer[Offset + 9] << 8 | Buffer[Offset + 8]) / 100.0);
			SC_PRINT_F("0x%.2x - Spec'd Max Voltage:\t%.2x%.2x (%.2fV)",
				   (Offset + 10), Buffer[Offset + 10], Buffer[Offset + 11],
				   (float)(Buffer[Offset + 11] << 8 | Buffer[Offset + 10]) / 100.0);
			SC_PRINT_F("0x%.2x - Spec'd Ripple Noise:\t%.2x%.2x (%dmV)",
				   (Offset + 12), Buffer[Offset + 12], Buffer[Offset + 13],
				   (Buffer[Offset + 13] << 8 | Buffer[Offset + 12]));
			SC_PRINT_F("0x%.2x - Min Current Load:\t%.2x%.2x (%dmA)",
				   (Offset + 14), Buffer[Offset + 14], Buffer[Offset + 15],
				   (Buffer[Offset + 15] << 8 | Buffer[Offset + 14]));
			SC_PRINT_F("0x%.2x - Max Current Load:\t%.2x%.2x (%dmA)",
				   (Offset + 16), Buffer[Offset + 16], Buffer[Offset + 17],
				   (Buffer[Offset + 17] << 8 | Buffer[Offset + 16]));
			break;
		case DC_LOAD:
			if (Buffer[Offset + 5] == 0x0) {
				SC_PRINT_F("0x%.2x - Output Number:\t%.2x (Voltage Adjust)",
					   (Offset + 5), Buffer[Offset + 5]);
			} else if (Buffer[Offset + 5] <= 0xF) {
				SC_PRINT_F("0x%.2x - Output Number:\t%.2x (Power Rail)",
					   (Offset + 5), Buffer[Offset + 5]);
			} else {
				SC_ERR("unsupported DC Load output number");
				return -1;
			}

			SC_PRINT_F("0x%.2x - Nominal Voltage:\t%.2x%.2x (%.2fV)",
				   (Offset + 6), Buffer[Offset + 6], Buffer[Offset + 7],
				   (float)(Buffer[Offset + 7] << 8 | Buffer[Offset + 6]) / 100.0);
			SC_PRINT_F("0x%.2x - Spec'd Min Voltage:\t%.2x%.2x (%.2fV)",
				   (Offset + 8), Buffer[Offset + 8], Buffer[Offset + 9],
				   (float)(Buffer[Offset + 9] << 8 | Buffer[Offset + 8]) / 100.0);
			SC_PRINT_F("0x%.2x - Spec'd Max Voltage:\t%.2x%.2x (%.2fV)",
				   (Offset + 10), Buffer[Offset + 10], Buffer[Offset + 11],
				   (float)(Buffer[Offset + 11] << 8 | Buffer[Offset + 10]) / 100.0);
			SC_PRINT_F("0x%.2x - Spec'd Ripple Noise:\t%.2x%.2x (%dmV)",
				   (Offset + 12), Buffer[Offset + 12], Buffer[Offset + 13],
				   (Buffer[Offset + 13] << 8 | Buffer[Offset + 12]));
			SC_PRINT_F("0x%.2x - Min Current Load:\t%.2x%.2x (%dmA)",
				   (Offset + 14), Buffer[Offset + 14], Buffer[Offset + 15],
				   (Buffer[Offset + 15] << 8 | Buffer[Offset + 14]));
			SC_PRINT_F("0x%.2x - Max Current Load:\t%.2x%.2x (%dmA)",
				   (Offset + 16), Buffer[Offset + 16], Buffer[Offset + 17],
				   (Buffer[Offset + 17] << 8 | Buffer[Offset + 16]));
			break;
		case OEM_D2:
			if (Buffer[Offset + 8] == 0x11) {
				SC_PRINT_F("0x%.2x - Version Number:\t%.2x (SC Mac ID)",
					   (Offset + 8), Buffer[Offset + 8]);
				SC_PRINT_F("0x%.2x - Mac ID 0:\t%.2x:%.2x:%.2x:%.2x:%.2x:%.2x",
					   (Offset + 9), Buffer[Offset + 9],
					   Buffer[Offset + 10], Buffer[Offset + 11],
					   Buffer[Offset + 12], Buffer[Offset + 13],
					   Buffer[Offset + 14]);
				if (MAC_Address) {
					SC_PRINT("SC MAC: %.2x:%.2x:%.2x:%.2x:%.2x:%.2x",
						 Buffer[Offset + 9], Buffer[Offset + 10],
						 Buffer[Offset + 11], Buffer[Offset + 12],
						 Buffer[Offset + 13], Buffer[Offset + 14]);
					SC_MAC_Found = true;
				}

			} else if (Buffer[Offset + 8] == 0x31) {
				SC_PRINT_F("0x%.2x - Version Number:\t%.2x (Veral Mac ID)",
					   (Offset + 8), Buffer[Offset + 8]);
				SC_PRINT_F("0x%.2x - Mac ID 0:\t%.2x:%.2x:%.2x:%.2x:%.2x:%.2x",
					   (Offset + 9), Buffer[Offset + 9],
					   Buffer[Offset + 10], Buffer[Offset + 11],
					   Buffer[Offset + 12], Buffer[Offset + 13],
					   Buffer[Offset + 14]);
				if (MAC_Address) {
					SC_PRINT("DUT MAC%s: %.2x:%.2x:%.2x:%.2x:%.2x:%.2x",
						 ((Length > 0xA) ? " 1" : ""),
						 Buffer[Offset + 9], Buffer[Offset + 10],
						 Buffer[Offset + 11], Buffer[Offset + 12],
						 Buffer[Offset + 13], Buffer[Offset + 14]);
				}

				if (Length > 0xA) {
					SC_PRINT_F("0x%.2x - Mac ID 1:\t%.2x:%.2x:%.2x:%.2x:%.2x:%.2x",
						   (Offset + 15), Buffer[Offset + 15],
						   Buffer[Offset + 16], Buffer[Offset + 17],
						   Buffer[Offset + 18], Buffer[Offset + 19],
						   Buffer[Offset + 20]);
					if (MAC_Address) {
						SC_PRINT("DUT MAC 2: %.2x:%.2x:%.2x:%.2x:%.2x:%.2x",
							 Buffer[Offset + 15], Buffer[Offset + 16],
							 Buffer[Offset + 17], Buffer[Offset + 18],
							 Buffer[Offset + 19], Buffer[Offset + 20]);
					}
				}

			} else {
				SC_ERR("unsupported D2 version number");
				return -1;
			}

			break;
		case OEM_D3:
			SC_PRINT_F("0x%.2x - Memory Type:\t%s", (Offset + 8),
				   &Buffer[Offset + 8]);
			Length = strlen(&Buffer[Offset + 8]) + 1;
			SC_PRINT_F("0x%.2x - Voltage Supply:\t%s", (Offset + 8 + Length),
				   &Buffer[Offset + 8 + Length]);
			break;
		case OEM_VITA_57_1:
			SC_PRINT_F("0x%.2x - Organizationally Unique Identifier:\t%.2x%.2x%.2x",
				   (Offset + 5), Buffer[Offset + 5], Buffer[Offset + 6],
				   Buffer[Offset + 7]);
			SC_PRINT_F("0x%.2x - Subtype Version:\t%.2x", (Offset + 8),
				   Buffer[Offset + 8]);
			SC_PRINT_F("0x%.2x - Connector Type:\t%.2x", (Offset + 9),
				   Buffer[Offset + 9]);
			SC_PRINT_F("0x%.2x - P1 Bank A Number Signals:\t%.2x", (Offset + 10),
				   Buffer[Offset + 10]);
			SC_PRINT_F("0x%.2x - P1 Bank B Number Signals:\t%.2x", (Offset + 11),
				   Buffer[Offset + 11]);
			SC_PRINT_F("0x%.2x - P2 Bank A Number Signals:\t%.2x", (Offset + 12),
				   Buffer[Offset + 12]);
			SC_PRINT_F("0x%.2x - P2 Bank B Number Signals:\t%.2x", (Offset + 13),
				   Buffer[Offset + 13]);
			SC_PRINT_F("0x%.2x - P1 GBT B Number Signals:\t%.2x", (Offset + 14),
				   Buffer[Offset + 14]);
			SC_PRINT_F("0x%.2x - Max Clock for TCK:\t%.2x (%dMhz)", (Offset + 15),
				   Buffer[Offset + 15], Buffer[Offset + 15]);
			break;
		default:
			SC_ERR("unsupported multirecord type");
			return -1;
		}

		if (!Last_Record) {
			/*
			 * Skip to the next multi-record.  There are 5 bytes of
			 * header in this record plus length of data in offset 0x2.
			 */
			Offset += (5 + Buffer[Offset + 2]);
			SC_PRINT_F(" ");
		}

	} while (!Last_Record);

	if (MAC_Address && !SC_MAC_Found) {
		if (Determine_SC_MAC() != 0) {
			SC_ERR("failed to determine SC MAC address");
			return -1;
		}
	}

	return 0;
}

int
Get_Measured_Clock(char *Counter_Reg, char *Label)
{
	char TCL_Path[SYSCMD_MAX], TCL_Args[STRLEN_MAX];
	char Output[STRLEN_MAX] = { 0 };
	Default_PDI_t *Default_PDI;
	char *ImageID, *UniqueID;

	Default_PDI = Plat_Devs->Default_PDI;
	if (Default_PDI == NULL) {
		SC_ERR("no default PDI is defined");
		return -1;
	}

	/* Silicon revision is needed to identify PDI's unique id */
	if (Get_Silicon_Revision(Silicon_Revision) != 0) {
		return -1;
	}

	ImageID = Default_PDI->ImageID;
	UniqueID = Default_PDI->UniqueID_InEffect;
	if (strcmp(UniqueID, "") == 0) {
		SC_ERR("failed to identify PDI's unique id");
		return -1;
	}

	(void) sprintf(TCL_Path, "%s%s", SCRIPT_PATH, TCL_CMD_TCL);
	(void) sprintf(TCL_Args, "%s %s %s %s", ImageID, UniqueID, READ_CLOCK_CMD, Counter_Reg);
	if (XSDB_Op(TCL_Path, TCL_Args, Output, sizeof(Output)) != 0) {
		SC_ERR("failed to get measured clock");
		return -1;
	}

	SC_PRINT("%s%.3f", Label, atof(Output));
	return 0;
}

int
Get_Measured_Clock_Vendor(Clock_t *Clock)
{
	char Label[STRLEN_MAX];

	for (int i = 0; i < Clock->Outputs; i++) {
		if (Clock->FPGA_Counter_Reg[i][0] != '\0') {
			(void) sprintf(Label, "O%d - Frequency(MHz):\t", i);
			if (Get_Measured_Clock(Clock->FPGA_Counter_Reg[i], Label) != 0) {
				SC_ERR("failed to get measured clock");
				return -1;
			}
		} else {
			SC_PRINT("O%d - Not Available", i);
		}
	}

	return 0;
}

int
Vendor_Utility_Clock(Clock_t *Clock, char *Command, char *Target, char *Value)
{
	FILE *FP;
	char System_Cmd[2 * SYSCMD_MAX];

	(void) sprintf(System_Cmd, "%s%s.py %s %s %d %s %s %s %s 2>&1", SCRIPT_PATH,
		       Clock->Part_Name, Board_Name, Clock->I2C_Bus, Clock->I2C_Address,
		       Clock->Default_Design, Command, Target, Value);
	SC_INFO("Command: %s", System_Cmd);
	FP = popen(System_Cmd, "r");
	if (FP == NULL) {
		SC_ERR("failed to invoke '%s': %m", System_Cmd);
		return -1;
	}

	while (fgets(System_Cmd, sizeof(System_Cmd), FP)) {
		SC_PRINT_N("%s", System_Cmd);
	}

	return pclose(FP);
}

int
Reset_Op(void)
{
	FILE *FP;
	char Buffer[SYSCMD_MAX] = { 0 };
	BootModes_t *BootModes;
	BootMode_t *BootMode;
#if !defined (LIBGPIOD_V1)
	int State;
#endif

	/* Assert POR */
	if (Set_GPIO("SYSCTLR_POR_B_LS", 0) != 0) {
		SC_ERR("failed to assert power-on-reset");
		return -1;
	}

	sleep(1);

	/* De-assert POR */
#if !defined (LIBGPIOD_V1)
	if (Get_GPIO("SYSCTLR_POR_B_LS", &State, GPIOD_LINE_DIRECTION_INPUT) != 0) {
#else
	if (Set_GPIO("SYSCTLR_POR_B_LS", 1) != 0) {
#endif
		SC_ERR("failed to de-assert power-on-reset");
		return -1;
	}

	/* If a boot mode is defined, set it after POR */
	if (access(BOOTMODEFILE, F_OK) == 0) {
		FP = fopen(BOOTMODEFILE, "r");
		if (FP == NULL) {
			SC_ERR("failed to open boot_mode file %s: %m", BOOTMODEFILE);
			return -1;
		}

		if (fgets(Buffer, sizeof(Buffer), FP) == NULL) {
			SC_ERR("failed to read boot_mode file %s: %m", BOOTMODEFILE);
			(void) fclose(FP);
			return -1;
		}

		(void) fclose(FP);
		SC_INFO("%s: %s", BOOTMODEFILE, Buffer);
		BootModes = Plat_Devs->BootModes;
		for (int i = 0; i < BootModes->Numbers; i++) {
			BootMode = &BootModes->BootMode[i];
			if (strcmp(strtok(Buffer, "\n"), (char *)BootMode->Name) == 0) {
				if (Set_AltBootMode(BootMode->Value) != 0) {
					SC_ERR("failed to set alternative boot mode");
					return -1;
				}

				break;
			}
		}
	}

	return 0;
}

int
Set_JTAGSelect(char *Select)
{
	JTAGSelects_t *JTAGSelects;
	int State;
	int Value = -1;
	bool Current = false;

	JTAGSelects = Plat_Devs->JTAGSelects;
	if (JTAGSelects == NULL) {
		SC_ERR("JTAG select operation is not supported");
		return -1;
	}

	for (int i = 0; i < JTAGSelects->Numbers; i++) {
		if (strcmp(JTAGSelects->JTAGSelect[i].Name, Select) == 0) {
			Value = JTAGSelects->JTAGSelect[i].Value;
		}
	}

	if (Value == -1) {
		Value = JTAGSelects->Current;
		Current = true;
	}

	if (!Current) {
		if (Set_GPIO(JTAGSelects->Select_Lines[0], (Value & 0x1)) != 0) {
			SC_ERR("failed to set JTAG 0");
			return -1;
		}

		if (Set_GPIO(JTAGSelects->Select_Lines[1], ((Value >> 1) & 0x1)) != 0) {
			SC_ERR("failed to set JTAG 1");
			return -1;
		}
	} else {
		/* If the current setting is FTDI, just read the state of JTAG mux */
		if (Value == 0x1) {
#if !defined (LIBGPIOD_V1)
			if (Get_GPIO(JTAGSelects->Select_Lines[0], &State,
				     GPIOD_LINE_DIRECTION_INPUT) != 0) {
				SC_ERR("failed to release JTAG 0");
				return -1;
			}

			if (Get_GPIO(JTAGSelects->Select_Lines[1], &State,
				     GPIOD_LINE_DIRECTION_INPUT) != 0) {
				SC_ERR("failed to release JTAG 1");
				return -1;
			}
#else
			if (Get_GPIO(JTAGSelects->Select_Lines[0], &State) != 0) {
				SC_ERR("failed to release JTAG 0");
				return -1;
			}

			if (Get_GPIO(JTAGSelects->Select_Lines[1], &State) != 0) {
				SC_ERR("failed to release JTAG 1");
				return -1;
			}
#endif
		} else {
			if (Set_GPIO(JTAGSelects->Select_Lines[0], (Value & 0x1)) != 0) {
				SC_ERR("failed to set JTAG 0");
				return -1;
			}

			if (Set_GPIO(JTAGSelects->Select_Lines[1], ((Value >> 1) & 0x1)) != 0) {
				SC_ERR("failed to set JTAG 1");
				return -1;
			}
		}
	}

	return 0;
}

int
XSDB_Op(const char *TCL_File, const char *TCL_Args, char *Output, int Length)
{
	FILE *FP;
	char System_Cmd[SYSCMD_MAX];
	char Buffer[LSTRLEN_MAX];
	char *Directory, *Filename;
	int Ret = 0;

	if (access(TCL_File, F_OK) != 0) {
		SC_ERR("failed to access file %s: %m", TCL_File);
		return -1;
	}

	if (Output == NULL) {
		SC_ERR("unallocated output buffer");
		return -1;
	}

	(void) Set_JTAGSelect("SC");
	Directory = strdup(TCL_File);
	Filename = strdup(TCL_File);
	/* System_Cmd: cd TCL_FILE directory; XSDB_ENV; XSDB_CMD TCL_FILE TCL_Args */
	if (TCL_Args == NULL) {
		(void) sprintf(System_Cmd, "cd %s; %s; %s %s 2>&1 | tee -a %s",
			       dirname(Directory), XSDB_ENV, XSDB_CMD, Filename, BITLOGFILE);
	} else {
		(void) sprintf(System_Cmd, "cd %s; %s; %s %s %s 2>&1 | tee -a %s",
			       dirname(Directory), XSDB_ENV, XSDB_CMD, Filename, TCL_Args, BITLOGFILE);
	}

	SC_INFO("Command: %s", System_Cmd);
	FP = popen(System_Cmd, "r");
	if (FP == NULL) {
		SC_ERR("failed to invoke xsdb");
		Ret = -1;
		goto Out;
	}

	while (fgets(Buffer, sizeof(Buffer), FP) != NULL) {
		SC_INFO("XSDB Output: %s", Buffer);
		(void) strncpy(Output, Buffer, (Length -1));
	}

	if (pclose(FP) != 0) {
		SC_INFO("Command: %s failed!", System_Cmd);
		Ret = -1;
	}

Out:
	(void) Set_JTAGSelect("Current");
	free(Directory);
	free(Filename);
	return Ret;
}

int
Get_IDCODE(char *Output, int Length)
{
	char TCL_File[STRLEN_MAX];
	char TCL_Args[STRLEN_MAX];

	(void) sprintf(TCL_File, "%s%s", BIT_PATH, IDCODE_TCL);
	(void) strcpy(TCL_Args, "0x0");
	return XSDB_Op(TCL_File, TCL_Args, Output, Length);
}

int
Get_Temperature(Temperature_t *Temperature)
{
	FILE *FP;
	char Buffer[SYSCMD_MAX];
	char *Temp;
	float Float_Temp;

	(void) sprintf(Buffer, "/usr/bin/sensors %s 2>&1", Temperature->Sensor);
	FP = popen(Buffer, "r");
	if (FP == NULL) {
		SC_ERR("failed to invoke '%s': %m", Buffer);
		return -1;
	}

	while (fgets(Buffer, sizeof(Buffer), FP) != NULL) {
		if (strstr(Buffer, "ERROR: ") != NULL) {
			SC_ERR("temperature is not available");
			(void) pclose(FP);
			return -1;
		}

		if ((strstr(Buffer, "temp:") == NULL) &&
		    (strstr(Buffer, "temp1:") == NULL)) {
			continue;
		}

		(void) strtok(Buffer, " ");
		Temp = strtok(NULL, " ");
		Float_Temp = strtof(Temp, NULL);
		SC_PRINT("Temperature(C):\t%3.1f", Float_Temp);
	}

	(void) pclose(FP);
	return 0;
}

int
Get_BootMode_Switch(unsigned int *Value)
{
	char Buffer[SYSCMD_MAX];
	int State;
	BootModes_t *BootModes = Plat_Devs->BootModes;

	*Value = 0;
	for (int i = 0; i < BootModes->Mode_Line_Numbers; i++) {
		sprintf(Buffer, "SYSCTLR_VERSAL_MODE%d_READBACK", i);
#if !defined (LIBGPIOD_V1)
		if (Get_GPIO(Buffer, &State, GPIOD_LINE_DIRECTION_INPUT) != 0) {
#else
		if (Get_GPIO(Buffer, &State) != 0) {
#endif
			return -1;
		}

		*Value |= (State << i);
	}

	return 0;
}

int
Get_BootMode(int Method)
{
	FILE *FP;
	char Buffer[SYSCMD_MAX];
	unsigned int Value;
	BootModes_t *BootModes = Plat_Devs->BootModes;

	/*
	 * Supported methods to get the boot mode:
	 *	0: External Boot Mode
	 *	1: Alternative Boot Mode
	 */
	if (Method == 0) {
		if (Get_BootMode_Switch(&Value) != 0) {
			SC_ERR("unable to read boot mode switch");
			return -1;
		}

		SC_INFO("Value 0x%x is read from boot mode switch", Value);
		for (int i = 0; i < BootModes->Numbers; i++) {
			if (Value == BootModes->BootMode[i].Value) {
				SC_PRINT("%s", BootModes->BootMode[i].Name);
				return 0;
			}
		}

		SC_ERR("unsupported boot mode value %#x", Value);
		return -1;
	}

	if (Method == 1) {
		if (access(BOOTMODEFILE, F_OK) != 0) {
			SC_ERR("no alternative boot mode is set");
			return -1;
		}

		FP = fopen(BOOTMODEFILE, "r");
		if (FP == NULL) {
			SC_ERR("failed to read file %s: %m", BOOTMODEFILE);
			return -1;
		}

		if (fgets(Buffer, SYSCMD_MAX, FP) == NULL) {
			SC_ERR("failed to read 'boot_mode' config file");
			(void) fclose(FP);
			return -1;
		}

		SC_PRINT_N("%s", Buffer);
		(void) fclose(FP);
		return 0;
	}

	SC_ERR("invalid Get_BootMode method");
	return -1;
}

int
Set_AltBootMode(int Value)
{
	char TCL_File[STRLEN_MAX];
	char TCL_Args[STRLEN_MAX];
	char Output[STRLEN_MAX] = { 0 };

	(void) sprintf(TCL_File, "%s%s", BIT_PATH, BOOTMODE_TCL);
	(void) sprintf(TCL_Args, "%x", Value);
	return XSDB_Op(TCL_File, TCL_Args, Output, sizeof(Output));
}

int
Set_BootMode(BootMode_t *BootMode, int Method)
{
	FILE *FP;
	char Buffer[SYSCMD_MAX];
	unsigned int Value;
	BootModes_t *BootModes = Plat_Devs->BootModes;

	/*
	 * Supported methods to set the boot mode:
	 *	0: External Boot Mode
	 * 	1: Alternative Boot Mode
	 */
	if (Method == 0) {
		/* Clear previous boot mode setting first, if any */
		for (int i = 0; i < BootModes->Mode_Line_Numbers; i++) {
			if (Set_GPIO(BootModes->Mode_Lines[i], 0x1) != 0) {
				SC_ERR("failed to set GPIO line %s",
				       BootModes->Mode_Lines[i]);
				return -1;
			}
		}

		/*
		 * VCK190/VMK180 boards don't have hardware support to read
		 * back the current position of boot mode switch.
		 */
		if (Get_BootMode_Switch(&Value) != 0) {
			SC_PRINT("WARNING: SW1 needs to be in OFF positions");
			Value = 0xF;
		}

		if ((BootMode->Value & Value) != BootMode->Value) {
			if (BootModes->Mode_Line_Numbers == 3) {
				SC_ERR("unable to set boot mode to '%s' because "
				       "boot mode switch is set to '%s %s %s' position",
				       BootMode->Name,
				       ((Value & 0x4) ? "OFF" : "ON"),
				       ((Value & 0x2) ? "OFF" : "ON"),
				       ((Value & 0x1) ? "OFF" : "ON"));
			} else {
				SC_ERR("unable to set boot mode to '%s' because "
				       "boot mode switch is set to '%s %s %s %s' position",
				       BootMode->Name,
				       ((Value & 0x8) ? "OFF" : "ON"),
				       ((Value & 0x4) ? "OFF" : "ON"),
				       ((Value & 0x2) ? "OFF" : "ON"),
				       ((Value & 0x1) ? "OFF" : "ON"));
			}

			return -1;
		}

		for (int i = 0; i < BootModes->Mode_Line_Numbers; i++) {
			if (Set_GPIO(BootModes->Mode_Lines[i],
			    ((BootMode->Value >> i) & 0x1)) != 0) {
				SC_ERR("failed to set GPIO line %s",
				       BootModes->Mode_Lines[i]);
				return -1;
			}
		}

		return 0;
	}

	if (Method == 1) {
		/* Record the boot mode */
		FP = fopen(BOOTMODEFILE, "w");
		if (FP == NULL) {
			SC_ERR("failed to open boot mode file %s: %m",
			       BOOTMODEFILE);
			return -1;
		}

		(void) sprintf(Buffer, "%s\n", BootMode->Name);
		SC_INFO("Alternatve Boot Mode: %s", Buffer);
		(void) fputs(Buffer, FP);
		(void) fclose(FP);
		return 0;
	}

	SC_ERR("invalid Set_BootMode method");
	return -1;
}

int
QSFP_ModuleSelect(SFP_t *SFP, int State)
{
	IO_Exp_t *IO_Exp;
	int i, j;
	int Found = 0;
	unsigned char Upper_Mask = -1;
	unsigned char Lower_Mask = -1;
	unsigned int Mask;
	unsigned int Value;
	int Level;

	if (State != 0 && State != 1) {
		SC_ERR("invalid SFP module select state");
		return -1;
	}

	/*
	 * In current board designs, the SFP modules of type 'sfp', 'sfpdd' and
	 * 'osfp' don't require the module to be selected before it is
	 * accessed.
	 */
	if (SFP->Type == sfp || SFP->Type == osfp || SFP->Type == sfpdd) {
		return 0;
	}

	/*
	 * If 'Access_Label' is defined for a SFP module, enable/disable it
	 * through libgpiod interface.
	 */
	if (SFP->Access_Label != NULL) {
#if !defined (LIBGPIOD_V1)
		SC_INFO("%s access to '%s' SFP", ((State == 1) ? "enable" : "disable"), SFP->Name);
		Level = SFP->Access_Level;
		if (State == 1) {
			if (Set_GPIO(SFP->Access_Label, Level) != 0) {
				SC_ERR("failed to set '%s' to %d", SFP->Access_Label, Level);
			}
		} else {
			if (Get_GPIO(SFP->Access_Label, &Level, GPIOD_LINE_DIRECTION_INPUT) != 0) {
				SC_ERR("failed to set '%s' to %d", SFP->Access_Label, Level);
			}
		}
#else
		if (State == 1) {
			Level = SFP->Access_Level;
		} else {
			Level = (~SFP->Access_Level & 0x1);
		}

		SC_INFO("%s access to '%s' SFP", ((State == 1) ? "enable" : "disable"), SFP->Name);
		if (Set_GPIO(SFP->Access_Label, Level) != 0) {
			SC_ERR("failed to set '%s' to %d", SFP->Access_Label, Level);
		}
#endif
		return 0;
	}

	/*
	 * In boards that need to use a PDI to enable SFP module select,
	 * there is nothing to do for 'State == 0'.
	 */
	if (SFP->Type == qsfp) {
		if (State == 1) {
			return VCK190_QSFP_ModuleSelect(SFP, State);
		} else {
			return 0;
		}
	}

	/*
	 * The following code enables the SFP module select for type
	 * 'qsfpdd' in current board designs.
	 */

	IO_Exp = Plat_Devs->IO_Exp;

	/*
	 * IO expander outputs are labeled to follow schematics for
	 * ease of referencing.  The layout of Output Port registers are
	 * as follow:
	 *
	 * Upper Byte: P07 P06 P05 P04 P03 P02 P01 P00
	 * Lower Byte: P17 P16 P15 P14 P13 P12 P11 P10
	 *
	 * From output labels find which bit of which byte should be
	 * set to 0 (Active Low) for (State == 1).  For (State == 0),
	 * revert the same bit to 1.
	 */
	for (i = 0, j = 8; i < 8; i++, j--) {
		if (strstr(IO_Exp->Labels[i], SFP->Name) != NULL) {
			Found = 1;
			break;
		}
	}

	if (j > 0) {
		Upper_Mask &= ~(1 << (j - 1));
	} else {
		Upper_Mask = 0xff;
	}

	if (!Found) {
		for (i = 8, j = 8; i < 16; i++, j--) {
			if (strstr(IO_Exp->Labels[i], SFP->Name) != NULL) {
				break;
			}
		}

		if (j > 0) {
			Lower_Mask &= ~(1 << (j - 1));
		} else {
			Lower_Mask = 0xff;
		}
	}

	/*
	 * Read the current output value, modify the desired bit, and
	 * write back the new output value.
	 */
	if (Access_IO_Exp(IO_Exp, 0, 0x2, &Value) != 0) {
		SC_ERR("failed to get IO expander output");
		return -1;
	}

	SC_INFO("Current Output Port Registers: %#x", Value);

	Mask = (Upper_Mask << 8) | Lower_Mask;
	if (State == 1) {
		Value &= Mask;
	} else {
		Value = Value | (~Mask & ((1 << IO_Exp->Numbers) - 1));
	}

	SC_INFO("Modify Output Port Registers: %#x", Value);

	if (Access_IO_Exp(IO_Exp, 1, 0x2, &Value) != 0) {
		SC_ERR("failed to set IO expander output");
		return -1;
	}

	return 0;
}

int
Check_Config_File(char *Name, char *Value, int *Found)
{
	FILE *FP;
	char Name_String[STRLEN_MAX];
	char Buffer[LSTRLEN_MAX];

	*Found = 0;
	if (access(CONFIGFILE, F_OK) == 0) {
		FP = fopen(CONFIGFILE, "r");
		if (FP == NULL) {
			SC_ERR("failed to read file %s: %m", CONFIGFILE);
			return -1;
		}

		(void) sprintf(Name_String, "%s:", Name);
		while (fgets(Buffer, LSTRLEN_MAX, FP)) {
			if (strstr(Buffer, Name_String) != NULL) {
				SC_INFO("%s: %s", CONFIGFILE, Buffer);
				(void) strtok(Buffer, ":");
				if (strcmp(Buffer, Name) != 0) {
					continue;
				}

				(void) strcpy(Value, strtok(NULL, " \n"));
				*Found = 1;
				break;
			}
		}

		(void) fclose(FP);
	}

	return 0;
}

/* fan_tach IP: S00 control, S01 tach counter, S02 AXI timer gate */
#define FAN_BASE_S00		0x80080000U	/* S00 register block physical base */
#define FAN_BASE_S01		0x80081000U	/* S01 register block physical base */
#define FAN_BASE_S02		0x80082000U	/* S02 AXI timer register block physical base */
#define FAN_OFF_CLEAR		0x8U		/* S00 offset of CLEAR (GPIO2 bit 0) */
#define FAN_OFF_COUNT_THRESH	0x0U		/* S01 offset of COUNT_THRESH register */
#define FAN_OFF_COUNT		0x8U		/* S01 offset of COUNT (pulse tally) register */
#define FAN_OFF_TCSR0		0x0U		/* S02 offset of TCSR0 (timer control/status) */
#define FAN_OFF_TLR0		0x4U		/* S02 offset of TLR0 (timer load value) */
#define FAN_TCSR_LOAD		0x00000066U	/* TCSR0 write: load TLR0 into the timer */
#define FAN_TCSR_START		0x000000c6U	/* TCSR0 write: start gate-interval timer */
#define FAN_ENT_MASK		0x00000080U	/* TCSR0 bit mask: ENT (timer enable) */
#define FAN_T0INT_MASK		0x00000100U	/* TCSR0 bit mask: T0INT (gate done) */
#define FAN_CLEAR_MASK		0x00000001U	/* S00 CLEAR bit mask */
#define FAN_COUNT_MASK		0x03ffffffU	/* Valid bits when reading COUNT (26 bits) */
#define FAN_COUNT_THRESH	32U		/* COUNT_THRESH: min tach pulse width (clocks) */
#define FAN_WINDOW_SEC		2.0		/* Gate duration (seconds) */
#define FAN_TIMER_CLK_HZ	100000000ULL	/* AXI timer input clock frequency (Hz) */
#define FAN_PULSES_PER_REV	2U		/* Tach pulses per fan revolution (PPR) */
#define FAN_POLL_MS		20U		/* T0INT poll interval (milliseconds) */
#define FAN_GATE_POLL_MAX	150U		/* T0INT poll limit: 2 s gate + 50% @ 20 ms */
#define FAN_PWM1_SYSFS	"/sys/devices/platform/pwm-fan/hwmon/hwmon*/pwm1"
#define FAN_TACH_PLAT_SYSFS	"/sys/bus/platform/devices/80080000.fan_tach"

typedef struct {
	void		*Virt;
	uintptr_t	Phys_Base;
	size_t		Length;
} Fan_MemMap_t;

static int
Fan_Read_PWM(int *PWM)
{
	glob_t Glob_Result;
	FILE *FP;
	const char *Sysfs_Path = NULL;
	char PWM_Sysfs_Path[STRLEN_MAX];

	if (PWM == NULL) {
		return -1;
	}

	if (glob(FAN_PWM1_SYSFS, 0, NULL, &Glob_Result) != 0 || Glob_Result.gl_pathc == 0) {
		globfree(&Glob_Result);
		SC_ERR("pwm-fan pwm1 sysfs not found");
		return -1;
	}

	(void) strncpy(PWM_Sysfs_Path, Glob_Result.gl_pathv[0],
		       sizeof(PWM_Sysfs_Path) - 1);
	globfree(&Glob_Result);
	Sysfs_Path = PWM_Sysfs_Path;

	FP = fopen(Sysfs_Path, "r");
	if (FP == NULL) {
		SC_ERR("failed to open '%s': %m", Sysfs_Path);
		return -1;
	}

	if (fscanf(FP, "%d", PWM) != 1) {
		(void) fclose(FP);
		SC_ERR("failed to read PWM from '%s'", Sysfs_Path);
		return -1;
	}

	(void) fclose(FP);

	return 0;
}

static int
Fan_Map_Regs(Fan_MemMap_t *Map)
{
	/*
	 * mmap one page-aligned region spanning S00, S01, and S02.
	 * Map_End_Addr is High_Base_Addr + 0x100 (256-byte register window),
	 * rounded up to the next page boundary.
	 */
	uintptr_t Low_Base_Addr = FAN_BASE_S00;
	uintptr_t High_Base_Addr = FAN_BASE_S02;
	uintptr_t Page_Size = (uintptr_t)sysconf(_SC_PAGESIZE);
	uintptr_t Map_Phys_Addr = Low_Base_Addr & ~(Page_Size - 1);
	uintptr_t Map_End_Addr = (High_Base_Addr + 0x100U + Page_Size - 1) &
	    ~(Page_Size - 1);
	int Mem_FD;

	Map->Phys_Base = Map_Phys_Addr;
	Map->Length = Map_End_Addr - Map_Phys_Addr;
	Map->Virt = NULL;

	Mem_FD = open("/dev/mem", O_RDWR | O_SYNC);
	if (Mem_FD < 0) {
		SC_ERR("failed to open '/dev/mem': %m");
		return -1;
	}

	Map->Virt = mmap(NULL, Map->Length, PROT_READ | PROT_WRITE,
			 MAP_SHARED, Mem_FD, (off_t)Map_Phys_Addr);
	(void) close(Mem_FD);
	if (Map->Virt == MAP_FAILED) {
		Map->Virt = NULL;
		SC_ERR("failed to mmap fan_tach registers: %m");
		return -1;
	}

	return 0;
}

static void
Fan_Unmap_Regs(Fan_MemMap_t *Map)
{
	if (Map->Virt != NULL && Map->Virt != MAP_FAILED) {
		(void) munmap(Map->Virt, Map->Length);
	}

	Map->Virt = NULL;
}

static volatile uint32_t *
Fan_Reg_Ptr(Fan_MemMap_t *Map, uint32_t Phys_Base, uint32_t Offset)
{
	uintptr_t Phys_Addr = (uintptr_t)Phys_Base + Offset;
	uintptr_t Map_Offset = Phys_Addr - Map->Phys_Base;

	return (volatile uint32_t *)((uint8_t *)Map->Virt + Map_Offset);
}

static uint32_t
Fan_Reg_Read(Fan_MemMap_t *Map, uint32_t Base_Addr, uint32_t Offset)
{
	return *Fan_Reg_Ptr(Map, Base_Addr, Offset);
}

static void
Fan_Reg_Write(Fan_MemMap_t *Map, uint32_t Base_Addr, uint32_t Offset,
	      uint32_t Value)
{
	*Fan_Reg_Ptr(Map, Base_Addr, Offset) = Value;
}

/*
 * Reset the gate-interval timer and tach pulse counter.
 * Clear T0INT and disable ENT so fan_tach_det does not re-open the
 * counting window; pulse S00 CLEAR to reset the tach pulse counter.
 * Does not affect fan PWM or rotation.
 */
static void
Fan_Reset_Tach_Count(Fan_MemMap_t *Map)
{
	uint32_t TCSR;

	TCSR = Fan_Reg_Read(Map, FAN_BASE_S02, FAN_OFF_TCSR0);
	Fan_Reg_Write(Map, FAN_BASE_S02, FAN_OFF_TCSR0,
		      (TCSR | FAN_T0INT_MASK) & ~FAN_ENT_MASK);
	Fan_Reg_Write(Map, FAN_BASE_S00, FAN_OFF_CLEAR, FAN_CLEAR_MASK);
	Fan_Reg_Write(Map, FAN_BASE_S00, FAN_OFF_CLEAR, 0U);
}

/*
 * Poll the AXI timer until the gate completes (T0INT set).
 *
 * The fan_tach IP uses the S02 AXI timer as a gate: tach pulses are
 * counted only while the timer runs.  When the programmed gate expires,
 * hardware sets T0INT (bit 8) in TCSR0.  This routine polls that bit
 * instead of sleeping for a fixed interval so the sample ends as soon as
 * the gate ends.
 */
static int
Fan_Wait_Gate_Done(Fan_MemMap_t *Map)
{
	uint32_t Poll_Index;

	for (Poll_Index = 0; Poll_Index < FAN_GATE_POLL_MAX; Poll_Index++) {
		/* T0INT set means the gate-interval timer has expired */
		if (Fan_Reg_Read(Map, FAN_BASE_S02, FAN_OFF_TCSR0) & FAN_T0INT_MASK) {
			return 0;
		}

		(void) usleep(FAN_POLL_MS * 1000U);
	}

	SC_ERR("timeout waiting for fan_tach gate-interval timer");
	return -1;
}

/*
 * Arm the gate-interval timer, wait for completion, and read COUNT.
 */
static int
Fan_Tach_Sample(Fan_MemMap_t *Map, uint32_t Timer_Load, uint32_t *Count)
{
	if (Count == NULL) {
		return -1;
	}

	/* COUNT_THRESH filters glitches shorter than ~0.32 us (32 clocks @ 100 MHz). */
	Fan_Reg_Write(Map, FAN_BASE_S01, FAN_OFF_COUNT_THRESH,
		      FAN_COUNT_THRESH & FAN_COUNT_MASK);

	Fan_Reg_Write(Map, FAN_BASE_S02, FAN_OFF_TLR0, Timer_Load);
	Fan_Reg_Write(Map, FAN_BASE_S02, FAN_OFF_TCSR0, FAN_TCSR_LOAD);
	Fan_Reg_Write(Map, FAN_BASE_S02, FAN_OFF_TCSR0, FAN_TCSR_START);

	if (Fan_Wait_Gate_Done(Map) != 0) {
		return -1;
	}

	*Count = Fan_Reg_Read(Map, FAN_BASE_S01, FAN_OFF_COUNT) & FAN_COUNT_MASK;

	return 0;
}

/*
 * Read pwm-fan duty cycle and tachometer RPM.
 *
 * Takes one tach reading over a gate, converts the pulse count to RPM,
 * and prints the current PWM duty cycle and fan speed.
 */
int
Fan_Op(void)
{
	Fan_MemMap_t Mem_Map = { 0 };
	int PWM_Duty = -1;
	uint32_t Pulse_Count = 0;
	uint32_t Timer_Load;
	uint32_t Fan_RPM;

	/*
	 * The platform device appears when the SC bitstream includes fan_tach.
	 * Without it, /dev/mem accesses to the register bases can stall the CPU.
	 */
	if (access(FAN_TACH_PLAT_SYSFS, F_OK) != 0) {
		SC_ERR("fan_tach IP not present ('%s' not found)",
		    FAN_TACH_PLAT_SYSFS);
		return -1;
	}

	/* Read current fan PWM duty from pwm-fan hwmon sysfs */
	if (Fan_Read_PWM(&PWM_Duty) != 0) {
		return -1;
	}

	/* Round down gate_sec * timer_clk to TLR0 tick counts */
	Timer_Load = (uint32_t)(FAN_WINDOW_SEC * (double)FAN_TIMER_CLK_HZ);

	/* Map fan_tach S00/S01/S02 registers via /dev/mem */
	if (Fan_Map_Regs(&Mem_Map) != 0) {
		return -1;
	}

	/* Reset stale COUNT and leave timer disabled before arming */
	Fan_Reset_Tach_Count(&Mem_Map);

	if (Fan_Tach_Sample(&Mem_Map, Timer_Load, &Pulse_Count) != 0) {
		Fan_Reset_Tach_Count(&Mem_Map);
		Fan_Unmap_Regs(&Mem_Map);
		return -1;
	}

	Fan_Reset_Tach_Count(&Mem_Map);
	Fan_Unmap_Regs(&Mem_Map);

	/* RPM = (pulses / PPR) / gate_sec * 60 */
	Fan_RPM = (Pulse_Count * 60U) /
	    (FAN_PULSES_PER_REV * (uint32_t)FAN_WINDOW_SEC);

	SC_INFO("fan tach: gate COUNT=%u", Pulse_Count);
	SC_PRINT("PWM(0-255):\t\t%d", PWM_Duty);
	SC_PRINT("Tachometer(RPM):\t%u", Fan_RPM);

	return 0;
}
