/*
 * Copyright (c) 2021 - 2022 Xilinx, Inc.  All rights reserved.
 * Copyright (c) 2022 - 2023 Advanced Micro Devices, Inc.  All rights reserved.
 *
 * SPDX-License-Identifier: MIT
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "jsmn.h"
#include "sc_app.h"

#define JSMN_TOKENS_SIZE 4096

extern int VCK190_ES1_Vccaux_Workaround(void *);
extern int Clocks_Check(void *, void *);
extern int XSDB_BIT(void *, void *);
extern int EBM_EEPROM_Check(void *, void *);
extern int DIMM_EEPROM_Check(void *, void *);
extern int Voltages_Check(void *, void *);
extern int DDRMC_1_Test(void *, void *);
extern int DDRMC_2_Test(void *, void *);
extern int DDRMC_3_Test(void *, void *);
extern int DDRMC_4_Test(void *, void *);
extern int Display_Instruction(void *, void *);
extern int Assert_Reset(void *, void *);
extern int Reset_IDT_8A34001(void);
extern int Check_Config_File(char *, char *, int *);

int jsoneq(const char *, jsmntok_t *, const char *);
int Parse_Feature(const char *, jsmntok_t *, int *, FeatureList_t **);
int Parse_BootMode(const char *, jsmntok_t *, int *, BootModes_t **);
int Parse_Clock(const char *, jsmntok_t *, int *, Clocks_t **);
int Parse_INA226(const char *, jsmntok_t *, int *, INA226s_t **);
int Parse_PowerDomain(const char *, jsmntok_t *, int *, Power_Domains_t **,
                      INA226s_t *);
int Parse_Voltage(const char *, jsmntok_t *, int *, Voltages_t **);
int Parse_Temperature(const char *, jsmntok_t *, int *, Temperature_t **);
int Parse_DIMM(const char *, jsmntok_t *, int *, DIMMs_t **);
int Parse_GPIO(const char *, jsmntok_t *, int *, GPIOs_t **);
int Parse_GPIO_Group(const char *, jsmntok_t *, int *, GPIO_Groups_t **);
int Parse_IO_EXP(const char *, jsmntok_t *, int *, IO_Exp_t **);
int Parse_DaughterCard(const char *, jsmntok_t *, int *, Daughter_Card_t **);
int Parse_SFP(const char *, jsmntok_t *, int *, SFPs_t **);
int Parse_FMC(const char *, jsmntok_t *, int *, FMCs_t **);
int Parse_Workaround(const char *, jsmntok_t *, int *, Workarounds_t **);
int Parse_BIT(const char *, jsmntok_t *, int *, BITs_t **);
int Parse_Constraint(const char *, jsmntok_t *, int *, Constraints_t **);

#define Check_Attribute(Attribute, Feature) { \
	Value_Str = strndup(Json_File + Tokens[*Index].start, \
			    Tokens[*Index].end - Tokens[*Index].start); \
	if (strcmp(Value_Str, (Attribute)) != 0) { \
		SC_ERR("missing '%s' attribute for '%s'", (Attribute), (Feature)); \
		return -1; \
	} \
\
	(*Index)++; \
}

#define Validate_Str_Size(Parsed_String, Feature, Attribute, Size) { \
	if ((strlen(Parsed_String) + 1) > Size) { \
		SC_ERR("%s: %s: value string '%s' is greater than %d characters", \
			Feature, Attribute, Parsed_String, Size); \
		return -1; \
	} \
}

#define Validate_Item_Size(Items_Qty, Feature, Attribute, Size_Limit) { \
	if (Items_Qty > Size_Limit) { \
		SC_ERR("%s: %s: number of values '%d' is greater than %d items", \
			Feature, Attribute, Items_Qty, Size_Limit); \
		return -1; \
	} \
}

int
Parse_JSON(const char *Board_Name, Plat_Devs_t *Dev_Parse) {
	int Parse_Result;
	int Token_Size = JSMN_TOKENS_SIZE;
	FILE *FP;
	char *Json_File;
	char Board_File[SYSCMD_MAX];
	char Board_Path[LSTRLEN_MAX];
	long Char_Len;
	jsmn_parser Parser;
	jsmntok_t Tokens[Token_Size];
	char Value[LSTRLEN_MAX];
	int Found = 0;

	jsmn_init(&Parser);

	/*
	 * The default location of JSON file can be overridden by defining
	 * 'Board_Path' parameter in CONFIGFILE.
	 */
	if (Check_Config_File("Board_Path", Value, &Found) != 0) {
		return -1;
	}

	(void) strcpy(Board_Path, ((Found) ? Value : BOARD_PATH));
	SC_INFO("Board Path: %s", Board_Path);

	snprintf(Board_File, SYSCMD_MAX, "%s%s.json", Board_Path, Board_Name);
	if (strstr(Board_File, "VMK180") != NULL) {
		snprintf(Board_File, SYSCMD_MAX, "%s%s.json", Board_Path, "VCK190");
	}

	SC_INFO("Board File: %s", Board_File);

	FP = fopen(Board_File, "r");
	if (FP == NULL) {
		SC_ERR("failed to open %s: %m", Board_File);
		return -1;
	}

	if (fseek(FP, 0, SEEK_END) != 0) {
		SC_ERR("error finding end of JSON file.");
		return -1;
	}

	Char_Len = ftell(FP);
	rewind(FP);

	Json_File = (char *)malloc(Char_Len * sizeof(char));
	if (Char_Len != fread(Json_File, sizeof(char), Char_Len, FP)) {
		SC_ERR("failed to read file %s: %m", Board_File);
		return -1;
	}

	Parse_Result = jsmn_parse(&Parser, Json_File, Char_Len, Tokens, Token_Size);
	if (Parse_Result < 1) {
		SC_ERR("failed to parse JSON - error code: %d", Parse_Result);

		if (Parse_Result == 0 || Tokens[0].type != JSMN_OBJECT) {
			SC_ERR("object expected!");
		} else if (Parse_Result == -1) {
			SC_ERR("insufficient JSMN tokens provided");
		} else if (Parse_Result == -2) {
			SC_ERR("invalid character inside json string");
		} else if (Parse_Result == -3) {
			SC_ERR("string is not a complete JSON packet, more bytes expected");
		}

		return -1;
	}

	for (int i = 0; i < Parse_Result; i++) {
		if (jsoneq(Json_File, &Tokens[i], "FEATURE") == 0) {
			if (Parse_Feature(Json_File, Tokens, &i,
					  &Dev_Parse->FeatureList) != 0) {
				return -1;
			}
		} else if (jsoneq(Json_File, &Tokens[i], "BOOTMODES") == 0) {
			if (Parse_BootMode(Json_File, Tokens, &i,
					   &Dev_Parse->BootModes) != 0) {
				return -1;
			}
		} else if (jsoneq(Json_File, &Tokens[i], "CLOCK") == 0) {
			if (Parse_Clock(Json_File, Tokens, &i,
					&Dev_Parse->Clocks) != 0) {
				return -1;
			}
		} else if (jsoneq(Json_File, &Tokens[i], "INA226") == 0) {
			if (Parse_INA226(Json_File, Tokens, &i,
					 &Dev_Parse->INA226s) != 0) {
				return -1;
			}
		} else if (jsoneq(Json_File, &Tokens[i], "POWER DOMAIN") == 0) {
			if (Parse_PowerDomain(Json_File, Tokens, &i,
					      &Dev_Parse->Power_Domains,
					      Dev_Parse->INA226s) != 0) {
				return -1;
			}
		} else if (jsoneq(Json_File, &Tokens[i], "VOLTAGE") == 0) {
			if (Parse_Voltage(Json_File, Tokens, &i,
					  &Dev_Parse->Voltages) != 0) {
				return -1;
			}
		} else if (jsoneq(Json_File, &Tokens[i], "Temperature") == 0) {
			if (Parse_Temperature(Json_File, Tokens, &i,
					  &Dev_Parse->Temperature) != 0) {
				return -1;
			}
		} else if (jsoneq(Json_File, &Tokens[i], "DIMM") == 0) {
			if (Parse_DIMM(Json_File, Tokens, &i, &Dev_Parse->DIMMs) != 0) {
				return -1;
			}
		} else if (jsoneq(Json_File, &Tokens[i], "GPIO") == 0) {
			if (Parse_GPIO(Json_File, Tokens, &i, &Dev_Parse->GPIOs) != 0) {
				return -1;
			}
		} else if (jsoneq(Json_File, &Tokens[i], "GPIO_Group") == 0) {
			if (Parse_GPIO_Group(Json_File, Tokens, &i,
					     &Dev_Parse->GPIO_Groups) != 0) {
				return -1;
			}
		} else if (jsoneq(Json_File, &Tokens[i], "IO Exp") == 0) {
			if (Parse_IO_EXP(Json_File, Tokens, &i, &Dev_Parse->IO_Exp) != 0) {
				return -1;
			}
		} else if (jsoneq(Json_File, &Tokens[i], "Daughter Card") == 0) {
			if (Parse_DaughterCard(Json_File, Tokens, &i,
					       &Dev_Parse->Daughter_Card) != 0) {
				return -1;
			}
		} else if (jsoneq(Json_File, &Tokens[i], "SFPs") == 0) {
			if (Parse_SFP(Json_File, Tokens, &i, &Dev_Parse->SFPs) != 0) {
				return -1;
			}
		} else if (jsoneq(Json_File, &Tokens[i], "FMCs") == 0) {
			if (Parse_FMC(Json_File, Tokens, &i, &Dev_Parse->FMCs) != 0) {
				return -1;
			}
		} else if (jsoneq(Json_File, &Tokens[i], "WORKAROUND") == 0) {
			if (Parse_Workaround(Json_File, Tokens, &i,
					     &Dev_Parse->Workarounds) != 0) {
				return -1;
			}
		} else if (jsoneq(Json_File, &Tokens[i], "BITs") == 0) {
			if (Parse_BIT(Json_File, Tokens, &i, &Dev_Parse->BITs) != 0) {
				return -1;
			}
		} else if (jsoneq(Json_File, &Tokens[i], "Constraints") == 0) {
			if (Parse_Constraint(Json_File, Tokens, &i,
					     &Dev_Parse->Constraints) != 0) {
				return -1;
			}
		}
	}

	return 0;
}

int
jsoneq(const char *json, jsmntok_t *tok, const char *s) {
	if ((tok->type == JSMN_STRING) && ((int)strlen(s) == (tok->end - tok->start))
	    && (strncmp(json + tok->start, s, tok->end - tok->start) == 0)) {
		return 0;
	}

	return -1;
}

int
Parse_Feature(const char *Json_File, jsmntok_t *Tokens, int *Index,
	      FeatureList_t **Features)
{
	char *Value_Str;
	int Item = 0;

	SC_INFO("********************* FEATURES *********************");
	*Features = (FeatureList_t *)malloc(sizeof(FeatureList_t));

	*Index += 2;
	Check_Attribute("List", "FEATURE");
	(*Features)->Numbers = Tokens[*Index].size;
	SC_INFO("Number of Features: %i\n", (*Features)->Numbers);
	SC_INFO("Features:");
	char **Feature_List = (char **)malloc((*Features)->Numbers * sizeof(char *));
	while (Item < (*Features)->Numbers) {
		(*Index)++;
		Value_Str = strndup(Json_File + Tokens[*Index].start,
				    Tokens[*Index].end - Tokens[*Index].start);
		Feature_List[Item] = (char *)malloc(strlen(Value_Str) + 1);
		Validate_Str_Size(Value_Str, "FEATURE", "List", STRLEN_MAX);
		strncpy(Feature_List[Item], Value_Str, strlen(Value_Str) + 1);
		SC_INFO("  %s  ", Feature_List[Item]);

		Item++;
	}
	(*Features)->Feature = Feature_List;

	return 0;
}

int
Parse_BootMode(const char *Json_File, jsmntok_t *Tokens, int *Index,
	       BootModes_t **Boots)
{
	char *Value_Str;
	int Boot_Items = 0;

	SC_INFO("********************* BOOTMODES *********************");
	*Boots = (BootModes_t *)malloc(sizeof(BootModes_t));

	*Index += 2;
	Check_Attribute("Mode_Lines", "BOOTMODES");
	int Mode_Lines_Qty = Tokens[*Index].size;
	(*Index)++;
	SC_INFO("Mode Lines:");
	char **Boot_Mode_Lines = (char **)malloc(Mode_Lines_Qty * sizeof(char *));
	for (int i = 0; i < Mode_Lines_Qty; i++) {
		Value_Str = strndup(Json_File + Tokens[*Index + i].start,
				    Tokens[*Index + i].end - Tokens[*Index + i].start);
		Validate_Str_Size(Value_Str, "BOOTMODES", "Mode_Lines", SYSCMD_MAX);
		Boot_Mode_Lines[i] = (char *)malloc(strlen(Value_Str) + 1);
		strncpy(Boot_Mode_Lines[i], Value_Str, strlen(Value_Str) + 1);
		SC_INFO("%s", Boot_Mode_Lines[i]);
	}
	(*Boots)->Mode_Lines = Boot_Mode_Lines;

	*Index += Mode_Lines_Qty;
	Check_Attribute("Modes", "BOOTMODES");
	(*Boots)->Numbers = Tokens[*Index].size;
	Validate_Item_Size((*Boots)->Numbers, "BOOTMODES", "Modes", ITEMS_MAX);
	(*Index)--;
	SC_INFO("Modes:");
	while (Boot_Items < (*Boots)->Numbers) {
		*Index += 2;
		Value_Str = strndup(Json_File + Tokens[*Index].start,
				    Tokens[*Index].end - Tokens[*Index].start);
		Validate_Str_Size(Value_Str, "BOOTMODES", "Modes", STRLEN_MAX);
		(*Boots)->BootMode[Boot_Items].Name = (char *)malloc(strlen(Value_Str) + 1);
		strncpy((*Boots)->BootMode[Boot_Items].Name, Value_Str, strlen(Value_Str) + 1);
		SC_INFO("Name: %s", (*Boots)->BootMode[Boot_Items].Name);
		Value_Str = strndup(Json_File + Tokens[*Index + 1].start,
				    Tokens[*Index + 1].end - Tokens[*Index + 1].start);
		(*Boots)->BootMode[Boot_Items].Value = (int)strtol(Value_Str, NULL, 0);
		SC_INFO("Value: %i\n", (*Boots)->BootMode[Boot_Items].Value);

		Boot_Items++;
	}

	return 0;
}

int
Parse_Clock(const char *Json_File, jsmntok_t *Tokens, int *Index, Clocks_t **CLKs)
{
	char *Value_Str;
	int Clk_Items = 0;
	IDT_8A34001_Data_t *IDT_8A34001_Data;

	SC_INFO("********************* CLOCK *********************");
	*CLKs = (Clocks_t *)malloc(sizeof(Clocks_t));

	(*Index)++;
	(*CLKs)->Numbers = Tokens[*Index].size;
	Validate_Item_Size((*CLKs)->Numbers, "CLOCK", "CLOCK", ITEMS_MAX);
	SC_INFO("Number of Clocks: %i", (*CLKs)->Numbers);
	while (Clk_Items < (*CLKs)->Numbers) {
		*Index += 3;
		Check_Attribute("Name", "CLOCK");
		Value_Str = strndup(Json_File + Tokens[*Index].start,
				    Tokens[*Index].end - Tokens[*Index].start);
		Validate_Str_Size(Value_Str, "CLOCK", "Name", STRLEN_MAX);
		(*CLKs)->Clock[Clk_Items].Name = (char *)malloc(strlen(Value_Str) + 1);
		strncpy((*CLKs)->Clock[Clk_Items].Name, Value_Str, strlen(Value_Str) + 1);
		SC_INFO("Name: %s", (*CLKs)->Clock[Clk_Items].Name);

		(*Index)++;
		Check_Attribute("Type", "CLOCK");
		Value_Str = strndup(Json_File + Tokens[*Index].start,
		                    Tokens[*Index].end - Tokens[*Index].start);
		if (strcmp(Value_Str, "Si570") == 0) {
			(*CLKs)->Clock[Clk_Items].Type = Si570;
		} else if (strcmp(Value_Str, "IDT_8A34001") == 0) {
			(*CLKs)->Clock[Clk_Items].Type = IDT_8A34001;
		}

		SC_INFO("Type: %s", Value_Str);
		if ((*CLKs)->Clock[Clk_Items].Type != IDT_8A34001) {
			(*Index)++;
			Check_Attribute("Sysfs_Path", "CLOCK");
			Value_Str = strndup(Json_File + Tokens[*Index].start,
			                    Tokens[*Index].end - Tokens[*Index].start);
			Validate_Str_Size(Value_Str, "CLOCK", "Sysfs_Path", SYSCMD_MAX);
			(*CLKs)->Clock[Clk_Items].Sysfs_Path = (char *)malloc(strlen(Value_Str) + 1);
			strncpy((*CLKs)->Clock[Clk_Items].Sysfs_Path, Value_Str, strlen(Value_Str) + 1);
			SC_INFO("Sysfs Path: %s", (*CLKs)->Clock[Clk_Items].Sysfs_Path);

			(*Index)++;
			Check_Attribute("Default_Freq", "CLOCK");
			Value_Str = strndup(Json_File + Tokens[*Index].start,
					    Tokens[*Index].end - Tokens[*Index].start);
			(*CLKs)->Clock[Clk_Items].Default_Freq = atof(Value_Str);
			SC_INFO("Default Freq: %f", (*CLKs)->Clock[Clk_Items].Default_Freq);

			(*Index)++;
			Check_Attribute("Upper_Freq", "CLOCK");
			Value_Str = strndup(Json_File + Tokens[*Index].start,
					    Tokens[*Index].end - Tokens[*Index].start);
			(*CLKs)->Clock[Clk_Items].Upper_Freq = atof(Value_Str);
			SC_INFO("Upper Freq: %f", (*CLKs)->Clock[Clk_Items].Upper_Freq);

			(*Index)++;
			Check_Attribute("Lower_Freq", "CLOCK");
			Value_Str = strndup(Json_File + Tokens[*Index].start,
					    Tokens[*Index].end - Tokens[*Index].start);
			(*CLKs)->Clock[Clk_Items].Lower_Freq = atof(Value_Str);
			SC_INFO("Lower Freq: %f", (*CLKs)->Clock[Clk_Items].Lower_Freq);
		} else {	// (Type == IDT_8A34001)
			(*Index)++;
			Check_Attribute("Display_Label", "CLOCK");
			IDT_8A34001_Data =
				(IDT_8A34001_Data_t *)malloc(sizeof(IDT_8A34001_Data_t));
			int Count = Tokens[*Index].size;
			(*Index)++;
			char **Display_Labels = (char **)malloc(Count * sizeof(char *));
			for (int i = 0; i < Count; i++) {
				Value_Str = strndup(Json_File + Tokens[*Index + i].start,
						    Tokens[*Index + i].end -
						    Tokens[*Index + i].start);
				Display_Labels[i] = (char *)malloc(strlen(Value_Str) + 1);
				Validate_Str_Size(Value_Str, "CLOCK", "Display_Label", SYSCMD_MAX);
				strncpy(Display_Labels[i], Value_Str, strlen(Value_Str) + 1);
				SC_INFO("%s", Display_Labels[i]);
			}
			IDT_8A34001_Data->Display_Label = Display_Labels;

			*Index += Count;
			Check_Attribute("Internal_Label", "CLOCK");
			(*Index)++;
			char **Internal_Labels = (char **)malloc(Count * sizeof(char *));
			for (int i = 0; i < Count; i++) {
				Value_Str = strndup(Json_File + Tokens[*Index + i].start,
						    Tokens[*Index + i].end -
						    Tokens[*Index + i].start);
				Internal_Labels[i] = (char *)malloc(strlen(Value_Str) + 1);
				Validate_Str_Size(Value_Str, "CLOCK", "Internal_Label", SYSCMD_MAX);
				strncpy(Internal_Labels[i], Value_Str, strlen(Value_Str) + 1);
				SC_INFO("%s", Internal_Labels[i]);
			}
			IDT_8A34001_Data->Internal_Label = Internal_Labels;

			*Index += (Count - 1);
			IDT_8A34001_Data->Number_Label = Count;
			IDT_8A34001_Data->Chip_Reset = Reset_IDT_8A34001;
			(*CLKs)->Clock[Clk_Items].Type_Data = (void *)IDT_8A34001_Data;
		}

		(*Index)++;
		Check_Attribute("I2C_Bus", "CLOCK");
		Value_Str = strndup(Json_File + Tokens[*Index].start,
				    Tokens[*Index].end - Tokens[*Index].start);
		Validate_Str_Size(Value_Str, "CLOCK", "I2C_Bus", STRLEN_MAX);
		(*CLKs)->Clock[Clk_Items].I2C_Bus = (char *)malloc(strlen(Value_Str) + 1);
		strncpy((*CLKs)->Clock[Clk_Items].I2C_Bus, Value_Str, strlen(Value_Str) + 1);
		SC_INFO("I2C Bus: %s", (*CLKs)->Clock[Clk_Items].I2C_Bus);

		(*Index)++;
		Check_Attribute("I2C_Address", "CLOCK");
		Value_Str = strndup(Json_File + Tokens[*Index].start,
				    Tokens[*Index].end - Tokens[*Index].start);
		SC_INFO("I2C Addr - Hex: %s", Value_Str);
		(*CLKs)->Clock[Clk_Items].I2C_Address = (int)strtol(Value_Str, NULL, 0);

		Clk_Items++;
	}

	return 0;
}

int
Parse_INA226(const char *Json_File, jsmntok_t *Tokens, int *Index, INA226s_t **INAs)
{
	char *Value_Str;
	int INA226_Items = 0;

	SC_INFO("********************* INA226 *********************");
	*INAs = (INA226s_t *)malloc(sizeof(INA226s_t));

	(*Index)++;
	(*INAs)->Numbers = Tokens[*Index].size;
	Validate_Item_Size((*INAs)->Numbers, "INA226", "INA226", ITEMS_MAX);
	SC_INFO("Number of INA226s: %i", (*INAs)->Numbers);
	while (INA226_Items < (*INAs)->Numbers) {
		*Index += 3;
		Check_Attribute("Name", "INA226");
		Value_Str = strndup(Json_File + Tokens[*Index].start,
				    Tokens[*Index].end - Tokens[*Index].start);
		Validate_Str_Size(Value_Str, "INA226", "Name", STRLEN_MAX);
		(*INAs)->INA226[INA226_Items].Name = (char *)malloc(strlen(Value_Str) + 1);
		strncpy((*INAs)->INA226[INA226_Items].Name, Value_Str, strlen(Value_Str) + 1);
		SC_INFO("Name: %s", (*INAs)->INA226[INA226_Items].Name);

		(*Index)++;
		Check_Attribute("I2C_Bus", "INA226");
		Value_Str = strndup(Json_File + Tokens[*Index].start,
				    Tokens[*Index].end - Tokens[*Index].start);
		Validate_Str_Size(Value_Str, "INA226", "I2C_Bus", STRLEN_MAX);
		(*INAs)->INA226[INA226_Items].I2C_Bus = (char *)malloc(strlen(Value_Str) + 1);
		strncpy((*INAs)->INA226[INA226_Items].I2C_Bus, Value_Str, strlen(Value_Str) + 1);
		SC_INFO("I2C Bus: %s", (*INAs)->INA226[INA226_Items].I2C_Bus);

		(*Index)++;
		Check_Attribute("I2C_Address", "INA226");
		Value_Str = strndup(Json_File + Tokens[*Index].start,
				    Tokens[*Index].end - Tokens[*Index].start);
		SC_INFO("I2C Addr - Hex: %s", Value_Str);
		(*INAs)->INA226[INA226_Items].I2C_Address = (int)strtol(Value_Str, NULL, 0);

		(*Index)++;
		Check_Attribute("Shunt_Resistor", "INA226");
		Value_Str = strndup(Json_File + Tokens[*Index].start,
				    Tokens[*Index].end - Tokens[*Index].start);
		(*INAs)->INA226[INA226_Items].Shunt_Resistor = atoi(Value_Str);
		SC_INFO("Shunt Resistor: %i", (*INAs)->INA226[INA226_Items].Shunt_Resistor);

		(*Index)++;
		Check_Attribute("Maximum_Current", "INA226");
		Value_Str = strndup(Json_File + Tokens[*Index].start,
				    Tokens[*Index].end - Tokens[*Index].start);
		(*INAs)->INA226[INA226_Items].Maximum_Current = atoi(Value_Str);
		SC_INFO("Max Current: %i", (*INAs)->INA226[INA226_Items].Maximum_Current);

		(*Index)++;
		Check_Attribute("Phase_Multiplier", "INA226");
		Value_Str = strndup(Json_File + Tokens[*Index].start,
				    Tokens[*Index].end - Tokens[*Index].start);
		(*INAs)->INA226[INA226_Items].Phase_Multiplier = atoi(Value_Str);
		SC_INFO("Phase_Multiplier: %i\n",
		        (*INAs)->INA226[INA226_Items].Phase_Multiplier);

		INA226_Items++;
	}

	return 0;
}

int
Parse_PowerDomain(const char *Json_File, jsmntok_t *Tokens, int *Index,
		  Power_Domains_t **PowerDoms, INA226s_t *INAs)
{
	char *Value_Str;
	int PwrDom_Items = 0;

	SC_INFO("******************* POWER DOMAIN *******************");
	*PowerDoms = (Power_Domains_t *)malloc(sizeof(Power_Domains_t));

	(*Index)++;
	(*PowerDoms)->Numbers = Tokens[*Index].size;
	Validate_Item_Size((*PowerDoms)->Numbers, "POWER DOMAIN", "POWER DOMAIN", ITEMS_MAX);
	SC_INFO("Number of Power Domains: %i", (*PowerDoms)->Numbers);
	while (PwrDom_Items < (*PowerDoms)->Numbers) {
		*Index += 3;
		Check_Attribute("Name", "POWER DOMAIN");
		Value_Str = strndup(Json_File + Tokens[*Index].start,
				    Tokens[*Index].end - Tokens[*Index].start);
		Validate_Str_Size(Value_Str, "POWER DOMAIN", "Name", STRLEN_MAX);
		(*PowerDoms)->Power_Domain[PwrDom_Items].Name = (char *)malloc(strlen(Value_Str) + 1);
		strncpy((*PowerDoms)->Power_Domain[PwrDom_Items].Name, Value_Str, strlen(Value_Str) + 1);
		SC_INFO("\nName: %s", (*PowerDoms)->Power_Domain[PwrDom_Items].Name);

		(*Index)++;
		Check_Attribute("Rails", "POWER DOMAIN");
		(*PowerDoms)->Power_Domain[PwrDom_Items].Numbers = Tokens[*Index].size;
		Validate_Item_Size((*PowerDoms)->Power_Domain[PwrDom_Items].Numbers,
                            "POWER DOMAIN", "Rails", ITEMS_MAX);
		SC_INFO("No. of Rails: %i", (*PowerDoms)->Power_Domain[PwrDom_Items].Numbers);
		SC_INFO("Rails: ");
		for (int i = 0; i < (*PowerDoms)->Power_Domain[PwrDom_Items].Numbers; i++) {
			(*Index)++;
			Value_Str = strndup(Json_File + Tokens[*Index].start,
					    Tokens[*Index].end - Tokens[*Index].start);
			SC_INFO("  %s ", Value_Str);

			for (int j = INAs->Numbers - 1; j >= 0; j--) {
				if (strstr(Value_Str, INAs->INA226[j].Name) != NULL) {
					(*PowerDoms)->Power_Domain[PwrDom_Items].Rails[i] = j;
					break;
				}
			}
		}

		PwrDom_Items++;
	}

	return 0;
}

int
Parse_Voltage(const char *Json_File, jsmntok_t *Tokens, int *Index,
	      Voltages_t **VCCs)
{
	char *Value_Str;
	int Voltage_Items = 0;

	SC_INFO("********************* VOLTAGES *********************");
	*VCCs = (Voltages_t *)calloc(1, sizeof(Voltages_t));

	(*Index)++;
	(*VCCs)->Numbers = Tokens[*Index].size;
	Validate_Item_Size((*VCCs)->Numbers, "VOLTAGE", "VOLTAGE", ITEMS_MAX);
	SC_INFO("Number of Voltage Rails: %i", (*VCCs)->Numbers);
	while (Voltage_Items < (*VCCs)->Numbers) {
		*Index += 3;
		Check_Attribute("Name", "VOLTAGE");
		Value_Str = strndup(Json_File + Tokens[*Index].start,
				    Tokens[*Index].end - Tokens[*Index].start);
		Validate_Str_Size(Value_Str, "VOLTAGE", "Name", STRLEN_MAX);
		(*VCCs)->Voltage[Voltage_Items].Name = (char *)malloc(strlen(Value_Str) + 1);
		strncpy((*VCCs)->Voltage[Voltage_Items].Name, Value_Str, strlen(Value_Str) + 1);
		SC_INFO("Name: %s", (*VCCs)->Voltage[Voltage_Items].Name);

		(*Index)++;
		Check_Attribute("Part_Name", "VOLTAGE");
		Value_Str = strndup(Json_File + Tokens[*Index].start,
				    Tokens[*Index].end - Tokens[*Index].start);
		Validate_Str_Size(Value_Str, "VOLTAGE", "Part_Name", STRLEN_MAX);
		(*VCCs)->Voltage[Voltage_Items].Part_Name = (char *)malloc(strlen(Value_Str) + 1);
		strncpy((*VCCs)->Voltage[Voltage_Items].Part_Name, Value_Str, strlen(Value_Str) + 1);
		SC_INFO("Part Name: %s", (*VCCs)->Voltage[Voltage_Items].Part_Name);

		(*Index)++;
		Check_Attribute("Maximum_Volt", "VOLTAGE");
		Value_Str = strndup(Json_File + Tokens[*Index].start,
				    Tokens[*Index].end - Tokens[*Index].start);
		(*VCCs)->Voltage[Voltage_Items].Maximum_Volt = atof(Value_Str);
		SC_INFO("\nMax Volt: %f", (*VCCs)->Voltage[Voltage_Items].Maximum_Volt);

		(*Index)++;
		Check_Attribute("Typical_Volt", "VOLTAGE");
		Value_Str = strndup(Json_File + Tokens[*Index].start,
				    Tokens[*Index].end - Tokens[*Index].start);
		(*VCCs)->Voltage[Voltage_Items].Typical_Volt = atof(Value_Str);
		SC_INFO("Typ Volt: %f", (*VCCs)->Voltage[Voltage_Items].Typical_Volt);

		(*Index)++;
		Check_Attribute("Minimum_Volt", "VOLTAGE");
		Value_Str = strndup(Json_File + Tokens[*Index].start,
				    Tokens[*Index].end - Tokens[*Index].start);
		(*VCCs)->Voltage[Voltage_Items].Minimum_Volt = atof(Value_Str);
		SC_INFO("Min Volt: %f", (*VCCs)->Voltage[Voltage_Items].Minimum_Volt);

		(*Index)++;
		Check_Attribute("I2C_Bus", "VOLTAGE");
		Value_Str = strndup(Json_File + Tokens[*Index].start,
				    Tokens[*Index].end - Tokens[*Index].start);
		Validate_Str_Size(Value_Str, "VOLTAGE", "I2C_Bus", STRLEN_MAX);
		(*VCCs)->Voltage[Voltage_Items].I2C_Bus = (char *)malloc(strlen(Value_Str) + 1);
		strncpy((*VCCs)->Voltage[Voltage_Items].I2C_Bus, Value_Str, strlen(Value_Str) + 1);
		SC_INFO("I2C Bus: %s", (*VCCs)->Voltage[Voltage_Items].I2C_Bus);

		(*Index)++;
		Check_Attribute("I2C_Address", "VOLTAGE");
		Value_Str = strndup(Json_File + Tokens[*Index].start,
				    Tokens[*Index].end - Tokens[*Index].start);
		SC_INFO("I2C Addr - Hex: %s", Value_Str);
		(*VCCs)->Voltage[Voltage_Items].I2C_Address = (int)strtol(Value_Str, NULL, 0);

		(*Index)++;
		Check_Attribute("PMBus_VOUT_MODE", "VOLTAGE");
		Value_Str = strndup(Json_File + Tokens[*Index].start,
				    Tokens[*Index].end - Tokens[*Index].start);
		(*VCCs)->Voltage[Voltage_Items].PMBus_VOUT_MODE = atoi(Value_Str);
		SC_INFO("PMBus_VOUT_MODE: %i\n", (*VCCs)->Voltage[Voltage_Items].PMBus_VOUT_MODE);

		(*Index)++;
		Check_Attribute("Page_Select", "VOLTAGE");
		Value_Str = strndup(Json_File + Tokens[*Index].start,
				    Tokens[*Index].end - Tokens[*Index].start);
		(*VCCs)->Voltage[Voltage_Items].Page_Select = atoi(Value_Str);
		SC_INFO("Page Select: %i\n", (*VCCs)->Voltage[Voltage_Items].Page_Select);

		(*Index)++;
		Value_Str = strndup(Json_File + Tokens[*Index].start,
				    Tokens[*Index].end - Tokens[*Index].start);
		if (strcmp(Value_Str, "Voltage_Multiplier") == 0) {
			(*Index)++;
			Value_Str = strndup(Json_File + Tokens[*Index].start,
					    Tokens[*Index].end - Tokens[*Index].start);
			(*VCCs)->Voltage[Voltage_Items].Voltage_Multiplier = atoi(Value_Str);
			SC_INFO("Voltage Multiplier: %i\n",
				(*VCCs)->Voltage[Voltage_Items].Voltage_Multiplier);
		} else {
			(*Index)--;
		}

		Voltage_Items++;
	}

	return 0;
}

int
Parse_Temperature(const char *Json_File, jsmntok_t *Tokens, int *Index,
		  Temperature_t **Temperature)
{
	char *Value_Str;

	SC_INFO("********************* Temperature *********************");
	*Temperature = (Temperature_t *)malloc(sizeof(Temperature_t));

	*Index += 2;
	Check_Attribute("Name", "Temperature");
	Value_Str = strndup(Json_File + Tokens[*Index].start,
			    Tokens[*Index].end - Tokens[*Index].start);
	Validate_Str_Size(Value_Str, "Temperature", "Name", STRLEN_MAX);
	(*Temperature)->Name = (char *)malloc(strlen(Value_Str) + 1);
	strncpy((*Temperature)->Name, Value_Str, strlen(Value_Str) + 1);
	SC_INFO("Name: %s", (*Temperature)->Name);

	(*Index)++;
	Check_Attribute("Sensor", "Temperature");
	Value_Str = strndup(Json_File + Tokens[*Index].start,
			    Tokens[*Index].end - Tokens[*Index].start);
	Validate_Str_Size(Value_Str, "Temperature", "Sensor", STRLEN_MAX);
	(*Temperature)->Sensor = (char *)malloc(strlen(Value_Str) + 1);
	strncpy((*Temperature)->Sensor, Value_Str, strlen(Value_Str) + 1);
	SC_INFO("Sensor: %s", (*Temperature)->Sensor);

	return 0;
}

int
Parse_DIMM(const char *Json_File, jsmntok_t *Tokens, int *Index, DIMMs_t **DIMMs)
{
	char *Value_Str;
	int DIMM_Items = 0;

	SC_INFO("********************* DIMM *********************");
	*DIMMs = (DIMMs_t *)malloc(sizeof(DIMMs_t));

	(*Index)++;
	(*DIMMs)->Numbers = Tokens[*Index].size;
	Validate_Item_Size((*DIMMs)->Numbers, "DIMM", "DIMM", ITEMS_MAX);
	SC_INFO("Number of DIMM: %i", (*DIMMs)->Numbers);
	while (DIMM_Items < (*DIMMs)->Numbers) {
		*Index += 3;
		Check_Attribute("Name", "DIMM");
		Value_Str = strndup(Json_File + Tokens[*Index].start,
				    Tokens[*Index].end - Tokens[*Index].start);
		Validate_Str_Size(Value_Str, "DIMM", "Name", STRLEN_MAX);
		(*DIMMs)->DIMM[DIMM_Items].Name = (char *)malloc(strlen(Value_Str) + 1);
		strncpy((*DIMMs)->DIMM[DIMM_Items].Name, Value_Str, strlen(Value_Str) + 1);
		SC_INFO("Name: %s", (*DIMMs)->DIMM[DIMM_Items].Name);

		(*Index)++;
		Check_Attribute("I2C_Bus", "DIMM");
		Value_Str = strndup(Json_File + Tokens[*Index].start,
				    Tokens[*Index].end - Tokens[*Index].start);
		Validate_Str_Size(Value_Str, "DIMM", "I2C_Bus", STRLEN_MAX);
		(*DIMMs)->DIMM[DIMM_Items].I2C_Bus = (char *)malloc(strlen(Value_Str) + 1);
		strncpy((*DIMMs)->DIMM[DIMM_Items].I2C_Bus, Value_Str, strlen(Value_Str) + 1);
		SC_INFO("I2C_Bus: %s", (*DIMMs)->DIMM[DIMM_Items].I2C_Bus);

		(*Index)++;
		Check_Attribute("I2C_Address_SPD", "DIMM");
		Value_Str = strndup(Json_File + Tokens[*Index].start,
				    Tokens[*Index].end - Tokens[*Index].start);
		(*DIMMs)->DIMM[DIMM_Items].I2C_Address_SPD = (int)strtol(Value_Str, NULL, 0);
		SC_INFO("I2C_Address_SPD: %u", (*DIMMs)->DIMM[DIMM_Items].I2C_Address_SPD);

		(*Index)++;
		Check_Attribute("I2C_Address_Thermal", "DIMM");
		Value_Str = strndup(Json_File + Tokens[*Index].start,
				    Tokens[*Index].end - Tokens[*Index].start);
		(*DIMMs)->DIMM[DIMM_Items].I2C_Address_Thermal = (int)strtol(Value_Str, NULL, 0);
		SC_INFO("I2C_Address_Thermal: %u", (*DIMMs)->DIMM[DIMM_Items].I2C_Address_Thermal);

		DIMM_Items++;
	}

	return 0;
}

int
Parse_GPIO(const char *Json_File, jsmntok_t *Tokens, int *Index, GPIOs_t **GPIOs)
{
	char *Value_Str;
	int Items = 0;

	SC_INFO("********************* GPIOS *********************");
	*GPIOs = (GPIOs_t *)malloc(sizeof(GPIOs_t));

	(*Index)++;
	(*GPIOs)->Numbers = Tokens[*Index].size;
	Validate_Item_Size((*GPIOs)->Numbers, "GPIO", "GPIO", LITEMS_MAX);
	SC_INFO("Number of GPIO: %i", (*GPIOs)->Numbers);
	while (Items < (*GPIOs)->Numbers) {
		(*Index)++;
		Value_Str = strndup(Json_File + Tokens[*Index].start,
				    Tokens[*Index].end - Tokens[*Index].start);
		Validate_Str_Size(Value_Str, "GPIO", "Internal_Name", STRLEN_MAX);
		(*GPIOs)->GPIO[Items].Internal_Name = Value_Str;
		SC_INFO("Internal Name: %s", (*GPIOs)->GPIO[Items].Internal_Name);

		(*Index)++;
		Value_Str = strndup(Json_File + Tokens[*Index].start,
				    Tokens[*Index].end - Tokens[*Index].start);
		Validate_Str_Size(Value_Str, "GPIO", "Display_Name", STRLEN_MAX);
		(*GPIOs)->GPIO[Items].Display_Name = Value_Str;
		SC_INFO("Display Name: %s", (*GPIOs)->GPIO[Items].Display_Name);

		Items++;
	}

	return 0;
}

int
Parse_GPIO_Group(const char *Json_File, jsmntok_t *Tokens, int *Index,
		 GPIO_Groups_t **GPIO_Groups)
{
	char *Value_Str;
	int Group_Items = 0;
	int Line_Items;

	SC_INFO("********************* GPIO Groups *********************");
	*GPIO_Groups = (GPIO_Groups_t *)malloc(sizeof(GPIO_Groups_t));

	(*Index)++;
	(*GPIO_Groups)->Numbers = Tokens[*Index].size;
	Validate_Item_Size((*GPIO_Groups)->Numbers, "GPIO_Group", "GPIO_Group",
			   ITEMS_MAX);
	SC_INFO("Number of GPIO groups: %i", (*GPIO_Groups)->Numbers);
	while (Group_Items < (*GPIO_Groups)->Numbers) {
		*Index += 3;
		Check_Attribute("Name", "GPIO_Group");
		Value_Str = strndup(Json_File + Tokens[*Index].start,
				    Tokens[*Index].end - Tokens[*Index].start);
		Validate_Str_Size(Value_Str, "GPIO_Group", "Name", STRLEN_MAX);
		(*GPIO_Groups)->GPIO_Group[Group_Items].Name = Value_Str;
		SC_INFO("GPIO Group: %s", (*GPIO_Groups)->GPIO_Group[Group_Items].Name);

		(*Index)++;
		Check_Attribute("GPIO_Lines", "GPIO_Group");
		Line_Items = Tokens[*Index].size;
		SC_INFO("Number of GPIO Lines: %d", Line_Items);
		(*Index)++;
		SC_INFO("GPIO Lines:");
		char **GPIO_Lines = (char **)malloc(Line_Items * sizeof(char *));
		for (int i = 0; i < Line_Items; i++) {
			Value_Str = strndup(Json_File + Tokens[*Index + i].start,
					    Tokens[*Index + i].end - Tokens[*Index + i].start);
			Validate_Str_Size(Value_Str, "GPIO_Group", "GPIO_Lines", SYSCMD_MAX);
			GPIO_Lines[i] = Value_Str;
			SC_INFO("%s", GPIO_Lines[i]);
		}

		(*GPIO_Groups)->GPIO_Group[Group_Items].Numbers = Line_Items;
		(*GPIO_Groups)->GPIO_Group[Group_Items].GPIO_Lines = GPIO_Lines;
		*Index += (Line_Items - 1);
		Group_Items++;
	}

	return 0;
}

int
Parse_IO_EXP(const char *Json_File, jsmntok_t *Tokens, int *Index, IO_Exp_t **IEs)
{
	char *Value_Str;
	int Label = 0;
	int Direcs = 0;

	SC_INFO("********************* IO EXP *********************");
	*IEs = (IO_Exp_t *)malloc(sizeof(IO_Exp_t));

	*Index += 2;
	Check_Attribute("Name", "IO Exp");
	Value_Str = strndup(Json_File + Tokens[*Index].start,
			    Tokens[*Index].end - Tokens[*Index].start);
	Validate_Str_Size(Value_Str, "IO Exp", "Name", STRLEN_MAX);
	(*IEs)->Name = (char *)malloc(strlen(Value_Str) + 1);
	strncpy((*IEs)->Name, Value_Str, strlen(Value_Str) + 1);
	SC_INFO("Name: %s\n", (*IEs)->Name);

	(*Index)++;
	Check_Attribute("Labels", "IO Exp");
	(*IEs)->Numbers = Tokens[*Index].size;
	Validate_Item_Size((*IEs)->Numbers, "IO Exp", "IO Exp", ITEMS_MAX);
	char **IE_Labels = (char **)malloc((*IEs)->Numbers * sizeof(char *));
	SC_INFO("Number of IO Exps: %i", (*IEs)->Numbers);
	SC_INFO("Labels -");
	while (Label < (*IEs)->Numbers) {
		(*Index)++;
		Value_Str = strndup(Json_File + Tokens[*Index].start,
				    Tokens[*Index].end - Tokens[*Index].start);
		IE_Labels[Label] = (char *)malloc(strlen(Value_Str) + 1);
		Validate_Str_Size(Value_Str, "IO Exp", "Labels", STRLEN_MAX);
		strncpy(IE_Labels[Label], Value_Str, strlen(Value_Str) + 1);
		SC_INFO("\t%s", IE_Labels[Label]);

		Label++;
	}
	(*IEs)->Labels = IE_Labels;

	(*Index)++;
	Check_Attribute("Directions", "IO Exp");
	SC_INFO("Directions -");
	while (Direcs < (*IEs)->Numbers) {
		(*Index)++;
		Value_Str = strndup(Json_File + Tokens[*Index].start,
				    Tokens[*Index].end - Tokens[*Index].start);
		(*IEs)->Directions[Direcs] = atoi(Value_Str);
		SC_INFO(" %i ", (*IEs)->Directions[Direcs]);

		Direcs++;
	}

	(*Index)++;
	Check_Attribute("I2C_Bus", "IO Exp");
	Value_Str = strndup(Json_File + Tokens[*Index].start,
			    Tokens[*Index].end - Tokens[*Index].start);
	Validate_Str_Size(Value_Str, "IO Exp", "I2C_Bus", STRLEN_MAX);
	(*IEs)->I2C_Bus = (char *)malloc(strlen(Value_Str) + 1);
	strncpy((*IEs)->I2C_Bus, Value_Str, strlen(Value_Str) + 1);
	SC_INFO("I2C Bus: %s", (*IEs)->I2C_Bus);

	(*Index)++;
	Check_Attribute("I2C_Address", "IO Exp");
	Value_Str = strndup(Json_File + Tokens[*Index].start,
			    Tokens[*Index].end - Tokens[*Index].start);
	SC_INFO("I2C Addr: %s", Value_Str);
	(*IEs)->I2C_Address = (int)strtol(Value_Str, NULL, 0);

	return 0;
}

int
Parse_DaughterCard(const char *Json_File, jsmntok_t *Tokens, int *Index,
                   Daughter_Card_t **DCs)
{
	char *Value_Str;

	SC_INFO("*************** Daughter Card ****************");
	*DCs = (Daughter_Card_t *)malloc(sizeof(Daughter_Card_t));

	*Index += 2;
	Check_Attribute("Name", "Daughter Card");
	Value_Str = strndup(Json_File + Tokens[*Index].start,
			    Tokens[*Index].end - Tokens[*Index].start);
	Validate_Str_Size(Value_Str, "Daughter Card", "Name", STRLEN_MAX);
	(*DCs)->Name = (char *)malloc(strlen(Value_Str) + 1);
	strncpy((*DCs)->Name, Value_Str, strlen(Value_Str) + 1);
	SC_INFO("Name: %s", (*DCs)->Name);

	(*Index)++;
	Check_Attribute("I2C_Bus", "Daughter Card");
	Value_Str = strndup(Json_File + Tokens[*Index].start,
			    Tokens[*Index].end - Tokens[*Index].start);
	Validate_Str_Size(Value_Str, "Daughter Card", "I2C_Bus", STRLEN_MAX);
	(*DCs)->I2C_Bus = (char *)malloc(strlen(Value_Str) + 1);
	strncpy((*DCs)->I2C_Bus, Value_Str, strlen(Value_Str) + 1);
	SC_INFO("I2C Bus: %s", (*DCs)->I2C_Bus);

	(*Index)++;
	Check_Attribute("I2C_Address", "Daughter Card");
	Value_Str = strndup(Json_File + Tokens[*Index].start,
			    Tokens[*Index].end - Tokens[*Index].start);
	SC_INFO("I2C Addr - Hex: %s", Value_Str);
	(*DCs)->I2C_Address = (int)strtol(Value_Str, NULL, 0);

	return 0;
}

int
Parse_SFP(const char *Json_File, jsmntok_t *Tokens, int *Index, SFPs_t **SFPs)
{
	char *Value_Str;
	int Item = 0;

	SC_INFO("******************** SFPs ********************");
	*SFPs = (SFPs_t *)malloc(sizeof(SFPs_t));

	(*Index)++;
	(*SFPs)->Numbers = Tokens[*Index].size;
	Validate_Item_Size((*SFPs)->Numbers, "SFPs", "SFPs", ITEMS_MAX);
	SC_INFO("Number of SFPs: %i", (*SFPs)->Numbers);
	while (Item < (*SFPs)->Numbers) {
		*Index += 3;
		Check_Attribute("Name", "SFPs");
		Value_Str = strndup(Json_File + Tokens[*Index].start,
				    Tokens[*Index].end - Tokens[*Index].start);
		Validate_Str_Size(Value_Str, "SFPs", "Name", STRLEN_MAX);
		(*SFPs)->SFP[Item].Name = Value_Str;
		SC_INFO("Name: %s", (*SFPs)->SFP[Item].Name);

		(*Index)++;
		Check_Attribute("Type", "SFPs");
		Value_Str = strndup(Json_File + Tokens[*Index].start,
				    Tokens[*Index].end - Tokens[*Index].start);
		SC_INFO("Type: %s", Value_Str);
		if (strcmp(Value_Str, "sfp") == 0) {
			(*SFPs)->SFP[Item].Type = sfp;
		} else if (strcmp(Value_Str, "qsfp") == 0) {
			(*SFPs)->SFP[Item].Type = qsfp;
		} else if (strcmp(Value_Str, "qsfpdd") == 0) {
			(*SFPs)->SFP[Item].Type = qsfpdd;
		} else if (strcmp(Value_Str, "osfp") == 0) {
			(*SFPs)->SFP[Item].Type = osfp;
		}

		(*Index)++;
		Check_Attribute("I2C_Bus", "SFPs");
		Value_Str = strndup(Json_File + Tokens[*Index].start,
				    Tokens[*Index].end - Tokens[*Index].start);
		Validate_Str_Size(Value_Str, "SFPs", "I2C_Bus", STRLEN_MAX);
		(*SFPs)->SFP[Item].I2C_Bus = Value_Str;
		SC_INFO("I2C Bus: %s", (*SFPs)->SFP[Item].I2C_Bus);

		(*Index)++;
		Check_Attribute("I2C_Address", "SFPs");
		Value_Str = strndup(Json_File + Tokens[*Index].start,
				    Tokens[*Index].end - Tokens[*Index].start);
		SC_INFO("I2C Addr: %s", Value_Str);
		(*SFPs)->SFP[Item].I2C_Address = (int)strtol(Value_Str, NULL, 0);

		Item++;
	}

	return 0;
}

int
Parse_FMC(const char *Json_File, jsmntok_t *Tokens, int *Index, FMCs_t **FMCs)
{
	char *Value_Str;
	int Item = 0;
	int Sub_Item;

	SC_INFO("******************** FMCs ********************");
	*FMCs = (FMCs_t *)malloc(sizeof(FMCs_t));

	(*Index)++;
	(*FMCs)->Numbers = Tokens[*Index].size;
	Validate_Item_Size((*FMCs)->Numbers, "FMC", "FMC", ITEMS_MAX);
	SC_INFO("Number of FMCs: %i", (*FMCs)->Numbers);
	while (Item < (*FMCs)->Numbers) {
		*Index += 3;
		Check_Attribute("Name", "FMC");
		Value_Str = strndup(Json_File + Tokens[*Index].start,
				    Tokens[*Index].end - Tokens[*Index].start);
		Validate_Str_Size(Value_Str, "FMC", "Name", STRLEN_MAX);
		(*FMCs)->FMC[Item].Name = (char *)malloc(strlen(Value_Str) + 1);
		strncpy((*FMCs)->FMC[Item].Name, Value_Str, strlen(Value_Str) + 1);
		SC_INFO("Name: %s", (*FMCs)->FMC[Item].Name);

		(*Index)++;
		Check_Attribute("I2C_Bus", "FMC");
		Value_Str = strndup(Json_File + Tokens[*Index].start,
				    Tokens[*Index].end - Tokens[*Index].start);
		Validate_Str_Size(Value_Str, "FMC", "I2C_Bus", STRLEN_MAX);
		(*FMCs)->FMC[Item].I2C_Bus = (char *)malloc(strlen(Value_Str) + 1);
		strncpy((*FMCs)->FMC[Item].I2C_Bus, Value_Str, strlen(Value_Str) + 1);
		SC_INFO("I2C Bus: %s", (*FMCs)->FMC[Item].I2C_Bus);

		(*Index)++;
		Check_Attribute("I2C_Address", "FMC");
		Value_Str = strndup(Json_File + Tokens[*Index].start,
				    Tokens[*Index].end - Tokens[*Index].start);
		SC_INFO("I2C Addr: %s", Value_Str);
		(*FMCs)->FMC[Item].I2C_Address = (int)strtol(Value_Str, NULL, 0);

		(*Index)++;
		Check_Attribute("Presence_Labels", "FMC");
		(*FMCs)->FMC[Item].Label_Numbers = Tokens[*Index].size;
		SC_INFO("Number of Presence Labels: %i\n", (*FMCs)->FMC[Item].Label_Numbers);
		SC_INFO("Presence Labels:");
		char **Presence_Labels = (char **)malloc((*FMCs)->FMC[Item].Label_Numbers *
							sizeof(char *));
		Sub_Item = 0;
		while (Sub_Item < (*FMCs)->FMC[Item].Label_Numbers) {
			(*Index)++;
			Value_Str = strndup(Json_File + Tokens[*Index].start,
					    Tokens[*Index].end - Tokens[*Index].start);
			Presence_Labels[Sub_Item] = (char *)malloc(strlen(Value_Str) + 1);
			Validate_Str_Size(Value_Str, "FMC", "Presence_Labels", STRLEN_MAX);
			strncpy(Presence_Labels[Sub_Item], Value_Str, strlen(Value_Str) + 1);
			SC_INFO("  %s  ", Presence_Labels[Sub_Item]);
			Sub_Item++;
		}

		(*FMCs)->FMC[Item].Presence_Labels = Presence_Labels;

		(*Index)++;
		Check_Attribute("Supported_Volts", "FMC");
		(*FMCs)->FMC[Item].Volt_Numbers = Tokens[*Index].size;
		SC_INFO("Number of Supported Voltages: %i\n", (*FMCs)->FMC[Item].Volt_Numbers);
		SC_INFO("Supported Voltages:");
		float *Supported_Volts = (float *)malloc((*FMCs)->FMC[Item].Volt_Numbers *
							 sizeof(float));
		Sub_Item = 0;
		while (Sub_Item < (*FMCs)->FMC[Item].Volt_Numbers) {
			(*Index)++;
			Value_Str = strndup(Json_File + Tokens[*Index].start,
					    Tokens[*Index].end - Tokens[*Index].start);
			Supported_Volts[Sub_Item] = atof(Value_Str);
			SC_INFO("  %f  ", Supported_Volts[Sub_Item]);
			Sub_Item++;
		}

		(*FMCs)->FMC[Item].Supported_Volts = Supported_Volts;

		(*Index)++;
		Check_Attribute("Voltage_Regulator", "FMC");
		Value_Str = strndup(Json_File + Tokens[*Index].start,
				    Tokens[*Index].end - Tokens[*Index].start);
		Validate_Str_Size(Value_Str, "FMC", "Voltage_Regulator", STRLEN_MAX);
		(*FMCs)->FMC[Item].Voltage_Regulator = (char *)malloc(strlen(Value_Str) + 1);
		strncpy((*FMCs)->FMC[Item].Voltage_Regulator, Value_Str, strlen(Value_Str) + 1);
		SC_INFO("Voltage_Regulator: %s", (*FMCs)->FMC[Item].Voltage_Regulator);

		(*Index)++;
		Check_Attribute("Default_Volt", "FMC");
		Value_Str = strndup(Json_File + Tokens[*Index].start,
				    Tokens[*Index].end - Tokens[*Index].start);
		(*FMCs)->FMC[Item].Default_Volt = atof(Value_Str);
		SC_INFO("Default Voltage: %f", (*FMCs)->FMC[Item].Default_Volt);

		Item++;
	}

	return 0;
}

int
Parse_Workaround(const char *Json_File, jsmntok_t *Tokens, int *Index,
                 Workarounds_t **WAs)
{
	char *Value_Str;
	int Item = 0;

	SC_INFO("***************** WORKAROUNDS **************\n");
	*WAs = (Workarounds_t *)malloc(sizeof(Workarounds_t));

	(*Index)++;
	(*WAs)->Numbers = Tokens[*Index].size;
	Validate_Item_Size((*WAs)->Numbers, "WORKAROUND", "WORKAROUND", ITEMS_MAX);
	SC_INFO("Number of Workarounds: %i", (*WAs)->Numbers);
	while (Item < (*WAs)->Numbers) {
		*Index += 3;
		Check_Attribute("Name", "WORKAROUND");
		Value_Str = strndup(Json_File + Tokens[*Index].start,
				    Tokens[*Index].end - Tokens[*Index].start);
		Validate_Str_Size(Value_Str, "WORKAROUND", "Name", STRLEN_MAX);
		(*WAs)->Workaround[Item].Name = (char *)malloc(strlen(Value_Str) + 1);
		strncpy((*WAs)->Workaround[Item].Name, Value_Str, strlen(Value_Str) + 1);
		SC_INFO("Name: %s", (*WAs)->Workaround[Item].Name);

		(*Index)++;
		Check_Attribute("Arg_Needed", "WORKAROUND");
		Value_Str = strndup(Json_File + Tokens[*Index].start,
				    Tokens[*Index].end - Tokens[*Index].start);
		(*WAs)->Workaround[Item].Arg_Needed = atoi(Value_Str);
		SC_INFO("Args Needed: %i", (*WAs)->Workaround[Item].Arg_Needed);

		(*Index)++;
		Check_Attribute("Plat_Workaround_Op", "WORKAROUND");
		Value_Str = strndup(Json_File + Tokens[*Index].start,
				    Tokens[*Index].end - Tokens[*Index].start);
		if (strcmp(Value_Str, "VCK190_ES1_Vccaux_Workaround") == 0) {
			(*WAs)->Workaround[Item].Plat_Workaround_Op =
				VCK190_ES1_Vccaux_Workaround;
			SC_INFO("Plat_Workaround_Op: %s", Value_Str);
		}

		Item++;
	}

	return 0;
}

int
Parse_BIT(const char *Json_File, jsmntok_t *Tokens, int *Index, BITs_t **BITs)
{
	char *Value_Str;
	char *Occurence;
	int Item = 0;
	int Level = 0;
	int i;
	int j;
	BIT_t *Temp;

	SC_INFO("********************* BITs *****************");
	*BITs = (BITs_t *)malloc(sizeof(BITs_t));

	(*Index)++;
	(*BITs)->Numbers = Tokens[*Index].size;
	Validate_Item_Size((*BITs)->Numbers, "BITs", "BITs", ITEMS_MAX);
	SC_INFO("Number of BITs: %i", (*BITs)->Numbers);
	while (Item < (*BITs)->Numbers) {
		*Index += 3;
		Check_Attribute("Name", "BITs");
		Value_Str = strndup(Json_File + Tokens[*Index].start,
				    Tokens[*Index].end - Tokens[*Index].start);
		Temp = &(*BITs)->BIT[Item];
		Validate_Str_Size(Value_Str, "BITs", "Name", STRLEN_MAX);
		Temp->Name = (char *)malloc(strlen(Value_Str) + 1);
		strncpy(Temp->Name, Value_Str, strlen(Value_Str) + 1);
		SC_INFO("Name: %s", Temp->Name);

		(*Index)++;
		Check_Attribute("Manual", "BITs");
		Value_Str = strndup(Json_File + Tokens[*Index].start,
				    Tokens[*Index].end - Tokens[*Index].start);
		Temp->Manual = atoi(Value_Str);
		SC_INFO("Manual: %i", Temp->Manual);

		(*Index)++;
		Check_Attribute("BIT Levels", "BITs");
		Temp->Levels = Tokens[*Index].size;
		Validate_Item_Size(Temp->Levels, "BITs", "Levels", LEVELS_MAX);

		SC_INFO("Levels: %i", Temp->Levels);
		Level = 0;
		while (Level < Temp->Levels) {
			*Index += 3;
			Check_Attribute("Plat_BIT_Op", "BITs");
			Value_Str = strndup(Json_File + Tokens[*Index].start,
					    Tokens[*Index].end - Tokens[*Index].start);
			SC_INFO("Plat BIT Op: %s", Value_Str);
			if (strcmp(Value_Str, "XSDB_BIT") == 0) {
				Temp->Level[Level].Plat_BIT_Op = XSDB_BIT;
				(*Index)++;
				Check_Attribute("TCL_File", "BITs");
				Value_Str = strndup(Json_File + Tokens[*Index].start,
						    Tokens[*Index].end - Tokens[*Index].start);
				Validate_Str_Size(Value_Str, "BITs", "TCL_File", SYSCMD_MAX);
				Temp->Level[Level].TCL_File = (char *)malloc(strlen(Value_Str) + 1);
				strncpy(Temp->Level[Level].TCL_File, Value_Str, strlen(Value_Str) + 1);
				SC_INFO("TCL File: %s", Temp->Level[Level].TCL_File);
			} else {
				if (strcmp(Value_Str, "Clocks_Check") == 0) {
					Temp->Level[Level].Plat_BIT_Op = Clocks_Check;
				} else if (strcmp(Value_Str, "EBM_EEPROM_Check") == 0) {
					Temp->Level[Level].Plat_BIT_Op = EBM_EEPROM_Check;
				} else if (strcmp(Value_Str, "DIMM_EEPROM_Check") == 0) {
					Temp->Level[Level].Plat_BIT_Op = DIMM_EEPROM_Check;
				} else if (strcmp(Value_Str, "Voltages_Check") == 0) {
					Temp->Level[Level].Plat_BIT_Op = Voltages_Check;
				} else if (strcmp(Value_Str, "DDRMC_1_Test") == 0) {
					Temp->Level[Level].Plat_BIT_Op = DDRMC_1_Test;
				} else if (strcmp(Value_Str, "DDRMC_2_Test") == 0) {
					Temp->Level[Level].Plat_BIT_Op = DDRMC_2_Test;
				} else if (strcmp(Value_Str, "DDRMC_3_Test") == 0) {
					Temp->Level[Level].Plat_BIT_Op = DDRMC_3_Test;
				} else if (strcmp(Value_Str, "DDRMC_4_Test") == 0) {
					Temp->Level[Level].Plat_BIT_Op = DDRMC_4_Test;
				} else if (strcmp(Value_Str, "Display_Instruction") == 0) {
					Temp->Level[Level].Plat_BIT_Op = Display_Instruction;
				} else if (strcmp(Value_Str, "Assert_Reset") == 0) {
					Temp->Level[Level].Plat_BIT_Op = Assert_Reset;
				} else {
					SC_ERR("Unknown Platform BIT Operation: %s", Value_Str);
					return -1;
				}
			}

			if (Temp->Manual == 1) {
				(*Index)++;
				Check_Attribute("Instruction", "BITs");
				Value_Str = strndup(Json_File + Tokens[*Index].start,
						    Tokens[*Index].end - Tokens[*Index].start);
				Occurence = strstr(Value_Str, "\\");
				while (Occurence != NULL) {
					if (Occurence[1] == 'n') {
						Occurence[0] = '\n';
						Occurence[1] = '|';
					} else if (Occurence[1] == 't') {
						Occurence[0] = '\t';
						Occurence[1] = '|';
					}

					Occurence = strstr(Value_Str, "\\");
				}

				i = 1;
				j = i + 1;
				while (Value_Str[i] != '\0') {
					if (Value_Str[j] == '|') {
						j++;
					}
					Value_Str[i] = Value_Str[j];
					i++;
					j++;
				}

				Validate_Str_Size(Value_Str, "BITs", "Instruction", SYSCMD_MAX);
				Temp->Level[Level].Instruction = (char *)malloc(strlen(Value_Str) + 1);
				strncpy(Temp->Level[Level].Instruction, Value_Str, strlen(Value_Str) + 1);
				SC_INFO("Instruction: %s", Temp->Level[Level].Instruction);
			}

			Level++;
		}

		Item++;
	}

	return 0;
}

int
Parse_Constraint(const char *Json_File, jsmntok_t *Tokens, int *Index, Constraints_t **Constraints)
{
	char *Value_Str;
	int Item = 0;
	int Sub_Item;
	Constraint_Phases_t *Pre_Phases_Data;

	SC_INFO("******************* Constraints ******************");
	*Constraints = (Constraints_t *)malloc(sizeof(Constraints_t));

	(*Index)++;
	(*Constraints)->Numbers = Tokens[*Index].size;
	Validate_Item_Size((*Constraints)->Numbers, "Constraints", "Constraints", ITEMS_MAX);
	SC_INFO("Number of Constraints: %i", (*Constraints)->Numbers);
	while (Item < (*Constraints)->Numbers) {
		*Index += 3;
		Check_Attribute("Type", "Constraints");
		Value_Str = strndup(Json_File + Tokens[*Index].start,
				    Tokens[*Index].end - Tokens[*Index].start);
		Validate_Str_Size(Value_Str, "Constraints", "Type", STRLEN_MAX);
		SC_INFO("Type: %s", Value_Str);
		(*Constraints)->Constraint[Item].Type = Value_Str;
		if ((strcmp(Value_Str, "Complete") != 0) &&
		    (strcmp(Value_Str, "Terminate") != 0)) {
			SC_ERR("invalid \'Type\' for Constraints");
			return -1;
		}

		(*Index)++;
		Check_Attribute("Command", "Constraints");
		Value_Str = strndup(Json_File + Tokens[*Index].start,
				    Tokens[*Index].end - Tokens[*Index].start);
		Validate_Str_Size(Value_Str, "Constraints", "Command", STRLEN_MAX);
		(*Constraints)->Constraint[Item].Command = Value_Str;
		SC_INFO("Command: %s", (*Constraints)->Constraint[Item].Command);

		(*Index)++;
		Value_Str = strndup(Json_File + Tokens[*Index].start,
				    Tokens[*Index].end - Tokens[*Index].start);
		Validate_Str_Size(Value_Str, "Constraints", "Next", STRLEN_MAX);
		if (strcmp(Value_Str, "Target") == 0) {
			(*Index)++;
			Value_Str = strndup(Json_File + Tokens[*Index].start,
					    Tokens[*Index].end - Tokens[*Index].start);
			Validate_Str_Size(Value_Str, "Constraints", "Target", STRLEN_MAX);
			(*Constraints)->Constraint[Item].Target = Value_Str;
			SC_INFO("Target: %s", (*Constraints)->Constraint[Item].Target);

			(*Index)++;
			Value_Str = strndup(Json_File + Tokens[*Index].start,
					    Tokens[*Index].end - Tokens[*Index].start);
			Validate_Str_Size(Value_Str, "Constraints", "Next", STRLEN_MAX);
			(*Constraints)->Constraint[Item].Value = NULL;
			if (strcmp(Value_Str, "Value") == 0) {
				(*Index)++;
				Value_Str = strndup(Json_File + Tokens[*Index].start,
						    Tokens[*Index].end - Tokens[*Index].start);
				Validate_Str_Size(Value_Str, "Constraints", "Value", STRLEN_MAX);
				(*Constraints)->Constraint[Item].Value = Value_Str;
				SC_INFO("Value: %s", (*Constraints)->Constraint[Item].Value);
				(*Index)++;
			}
		}

		Check_Attribute("Pre_Phases", "Constraints");
		Pre_Phases_Data = (Constraint_Phases_t *)malloc(sizeof(Constraint_Phases_t));
		Pre_Phases_Data->Numbers = Tokens[*Index].size;
		SC_INFO("Number of Pre_Phases: %i", Pre_Phases_Data->Numbers);
		Sub_Item = 0;
		while (Sub_Item < Pre_Phases_Data->Numbers) {
			*Index += 3;
			Check_Attribute("Type", "Pre_Phases");
			Value_Str = strndup(Json_File + Tokens[*Index].start,
					    Tokens[*Index].end - Tokens[*Index].start);
			Validate_Str_Size(Value_Str, "Pre_Phases", "Type", STRLEN_MAX);
			SC_INFO("Pre_Phases Type: %s", Value_Str);
			Pre_Phases_Data->Phase[Sub_Item].Type = Value_Str;
			if ((strcmp(Value_Str, "External") != 0) &&
			    (strcmp(Value_Str, "Internal") != 0)) {
				SC_ERR("invalid \'Type\' for Pre_Phases");
				return -1;
			}

			(*Index)++;
			Check_Attribute("Command", "Pre_Phases");
			Value_Str = strndup(Json_File + Tokens[*Index].start,
					    Tokens[*Index].end - Tokens[*Index].start);
			Validate_Str_Size(Value_Str, "Pre_Phases", "Command", STRLEN_MAX);
			Pre_Phases_Data->Phase[Sub_Item].Command = Value_Str;
			SC_INFO("Pre_Phases Command: %s", Pre_Phases_Data->Phase[Sub_Item].Command);

			(*Index)++;
			Value_Str = strndup(Json_File + Tokens[*Index].start,
					    Tokens[*Index].end - Tokens[*Index].start);
			Validate_Str_Size(Value_Str, "Pre_Phases", "Next", STRLEN_MAX);
			Pre_Phases_Data->Phase[Sub_Item].Args = NULL;
			if (strcmp(Value_Str, "Args") == 0) {
				(*Index)++;
				Value_Str = strndup(Json_File + Tokens[*Index].start,
						    Tokens[*Index].end - Tokens[*Index].start);
				Validate_Str_Size(Value_Str, "Pre_Phases", "Args", STRLEN_MAX);
				Pre_Phases_Data->Phase[Sub_Item].Args = Value_Str;
				SC_INFO("Pre_Phases Args: %s", Pre_Phases_Data->Phase[Sub_Item].Args);
			} else {
				(*Index)--;
			}

			Sub_Item++;
		}

		(*Constraints)->Constraint[Item].Pre_Phases = Pre_Phases_Data;
		Item++;
	}

	return 0;
}
