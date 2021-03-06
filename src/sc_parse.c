/*
 * Copyright (c) 2021 - 2022 Xilinx, Inc.  All rights reserved.
 *
 * SPDX-License-Identifier: MIT
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "jsmn.h"
#include "sc_app.h"

#define JSMN_TOKENS_SIZE 2048

extern int VCK190_ES1_Vccaux_Workaround(void *);
extern int Clocks_Check(void *, void *);
extern int XSDB_BIT(void *, void *);
extern int EBM_EEPROM_Check(void *, void *);
extern int DIMM_EEPROM_Check(void *, void *);
extern int Voltages_Check(void *, void *);
extern int Display_Instruction(void *, void *);
extern int Assert_Reset(void *, void *);
extern int Reset_IDT_8A34001(void);

int jsoneq(const char *, jsmntok_t *, const char *);
int Parse_Feature(const char *, jsmntok_t *, int *, FeatureList_t **);
int Parse_BootMode(const char *, jsmntok_t *, int *, BootModes_t **);
int Parse_Clock(const char *, jsmntok_t *, int *, Clocks_t **);
int Parse_INA226(const char *, jsmntok_t *, int *, INA226s_t **);
int Parse_PowerDomain(const char *, jsmntok_t *, int *, Power_Domains_t **,
                      INA226s_t *);
int Parse_Voltage(const char *, jsmntok_t *, int *, Voltages_t **);
int Parse_DIMM(const char *, jsmntok_t *, int *, DIMM_t **);
int Parse_GPIO(const char *, jsmntok_t *, int *, GPIOs_t **);
int Parse_IO_EXP(const char *, jsmntok_t *, int *, IO_Exp_t **);
int Parse_DaughterCard(const char *, jsmntok_t *, int *, Daughter_Card_t **);
int Parse_SFP(const char *, jsmntok_t *, int *, SFPs_t **);
int Parse_QSFP(const char *, jsmntok_t *, int *, QSFPs_t **);
int Parse_FMC(const char *, jsmntok_t *, int *, FMCs_t **);
int Parse_Workaround(const char *, jsmntok_t *, int *, Workarounds_t **);
int Parse_BIT(const char *, jsmntok_t *, int *, BITs_t **);

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

int
Parse_JSON(const char *Board_Name, Plat_Devs_t *Dev_Parse) {
	int Parse_Result;
	int Token_Size = JSMN_TOKENS_SIZE;
	FILE *FP;
	char *Json_File;
	char Board_File[STRLEN_MAX];
	long Char_Len;
	jsmn_parser Parser;
	jsmntok_t Tokens[Token_Size];

	jsmn_init(&Parser);

	snprintf(Board_File, STRLEN_MAX, "%s%s.json", BOARD_PATH, Board_Name);
	if (strstr(Board_File, "VMK180") != NULL) {
		snprintf(Board_File, STRLEN_MAX, "%s%s.json", BOARD_PATH, "VCK190");
	}

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
		} else if (jsoneq(Json_File, &Tokens[i], "DIMM") == 0) {
			if (Parse_DIMM(Json_File, Tokens, &i, &Dev_Parse->DIMM) != 0) {
				return -1;
			}
		} else if (jsoneq(Json_File, &Tokens[i], "GPIO") == 0) {
			if (Parse_GPIO(Json_File, Tokens, &i, &Dev_Parse->GPIOs) != 0) {
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
		} else if (jsoneq(Json_File, &Tokens[i], "QSFPs") == 0) {
			if (Parse_QSFP(Json_File, Tokens, &i, &Dev_Parse->QSFPs) != 0) {
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
	while (Item < (*Features)->Numbers) {
		(*Index)++;
		Value_Str = strndup(Json_File + Tokens[*Index].start,
				    Tokens[*Index].end - Tokens[*Index].start);
		(*Features)->Feature[Item] = (char *)calloc(strlen(Value_Str), sizeof(char));
		Validate_Str_Size(Value_Str, "FEATURE", "List", STRLEN_MAX);
		(*Features)->Feature[Item] = Value_Str;
		SC_INFO("  %s  ", (*Features)->Feature[Item]);

		Item++;
	}

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
	for (int i = 0; i < Mode_Lines_Qty; i++) {
		Value_Str = strndup(Json_File + Tokens[*Index + i].start,
				    Tokens[*Index + i].end - Tokens[*Index + i].start);
		Validate_Str_Size(Value_Str, "BOOTMODES", "Mode_Lines", SYSCMD_MAX);
		strncpy((*Boots)->Mode_Lines[i], Value_Str, strlen(Value_Str) + 1);
		SC_INFO("%s", (*Boots)->Mode_Lines[i]);
	}

	*Index += Mode_Lines_Qty;
	Check_Attribute("Modes", "BOOTMODES");
	(*Boots)->Numbers = Tokens[*Index].size;
	(*Index)--;
	SC_INFO("Modes:");
	while (Boot_Items < (*Boots)->Numbers) {
		*Index += 2;
		Value_Str = strndup(Json_File + Tokens[*Index].start,
				    Tokens[*Index].end - Tokens[*Index].start);
		Validate_Str_Size(Value_Str, "BOOTMODES", "Modes", STRLEN_MAX);
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
	char *Label;
	IDT_8A34001_Data_t *IDT_8A34001_Data;

	SC_INFO("********************* CLOCK *********************");
	*CLKs = (Clocks_t *)malloc(sizeof(Clocks_t));

	(*Index)++;
	(*CLKs)->Numbers = Tokens[*Index].size;
	SC_INFO("Number of Clocks: %i", (*CLKs)->Numbers);
	while (Clk_Items < (*CLKs)->Numbers) {
		*Index += 3;
		Check_Attribute("Name", "CLOCK");
		Value_Str = strndup(Json_File + Tokens[*Index].start,
				    Tokens[*Index].end - Tokens[*Index].start);
		Validate_Str_Size(Value_Str, "CLOCK", "Name", STRLEN_MAX);
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
		if ((*CLKs)->Clock[Clk_Items].Type == Si570) {
			(*Index)++;
			Check_Attribute("Sysfs_Path", "CLOCK");
			Value_Str = strndup(Json_File + Tokens[*Index].start,
			                    Tokens[*Index].end - Tokens[*Index].start);
			Validate_Str_Size(Value_Str, "CLOCK", "Sysfs_Path", SYSCMD_MAX);
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
		} else if ((*CLKs)->Clock[Clk_Items].Type == IDT_8A34001) {
			(*Index)++;
			Check_Attribute("Display_Label", "CLOCK");
			IDT_8A34001_Data =
				(IDT_8A34001_Data_t *)malloc(sizeof(IDT_8A34001_Data_t));
			int Count = Tokens[*Index].size;
			(*Index)++;
			for (int i = 0; i < Count; i++) {
				Value_Str = strndup(Json_File + Tokens[*Index + i].start,
						    Tokens[*Index + i].end -
						    Tokens[*Index + i].start);
				Label = (char *)malloc(strlen(Value_Str) + 1);
				Validate_Str_Size(Value_Str, "CLOCK", "Display_Label", SYSCMD_MAX);
				strncpy(Label, Value_Str, strlen(Value_Str) + 1);
				IDT_8A34001_Data->Display_Label[i] = Label;
				SC_INFO("%s", IDT_8A34001_Data->Display_Label[i]);
			}

			*Index += Count;
			Check_Attribute("Internal_Label", "CLOCK");
			(*Index)++;
			for (int i = 0; i < Count; i++) {
				Value_Str = strndup(Json_File + Tokens[*Index + i].start,
						    Tokens[*Index + i].end -
						    Tokens[*Index + i].start);
				Label = (char *)malloc(strlen(Value_Str) + 1);
				Validate_Str_Size(Value_Str, "CLOCK", "Internal_Label", SYSCMD_MAX);
				strncpy(Label, Value_Str, strlen(Value_Str) + 1);
				IDT_8A34001_Data->Internal_Label[i] = Label;
				SC_INFO("%s", IDT_8A34001_Data->Internal_Label[i]);
			}

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
	SC_INFO("Number of INA226s: %i", (*INAs)->Numbers);
	while (INA226_Items < (*INAs)->Numbers) {
		*Index += 3;
		Check_Attribute("Name", "INA226");
		Value_Str = strndup(Json_File + Tokens[*Index].start,
				    Tokens[*Index].end - Tokens[*Index].start);
		Validate_Str_Size(Value_Str, "INA226", "Name", STRLEN_MAX);
		strncpy((*INAs)->INA226[INA226_Items].Name, Value_Str, strlen(Value_Str) + 1);
		SC_INFO("Name: %s", (*INAs)->INA226[INA226_Items].Name);

		(*Index)++;
		Check_Attribute("I2C_Bus", "INA226");
		Value_Str = strndup(Json_File + Tokens[*Index].start,
				    Tokens[*Index].end - Tokens[*Index].start);
		Validate_Str_Size(Value_Str, "INA226", "I2C_Bus", STRLEN_MAX);
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
	SC_INFO("Number of Power Domains: %i", (*PowerDoms)->Numbers);
	while (PwrDom_Items < (*PowerDoms)->Numbers) {
		*Index += 3;
		Check_Attribute("Name", "POWER DOMAIN");
		Value_Str = strndup(Json_File + Tokens[*Index].start,
				    Tokens[*Index].end - Tokens[*Index].start);
		Validate_Str_Size(Value_Str, "POWER DOMAIN", "Name", STRLEN_MAX);
		strncpy((*PowerDoms)->Power_Domain[PwrDom_Items].Name, Value_Str, strlen(Value_Str) + 1);
		SC_INFO("\nName: %s", (*PowerDoms)->Power_Domain[PwrDom_Items].Name);

		(*Index)++;
		Check_Attribute("Rails", "POWER DOMAIN");
		(*PowerDoms)->Power_Domain[PwrDom_Items].Numbers = Tokens[*Index].size;
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
	*VCCs = (Voltages_t *)malloc(sizeof(Voltages_t));

	(*Index)++;
	(*VCCs)->Numbers = Tokens[*Index].size;
	SC_INFO("Number of Voltage Rails: %i", (*VCCs)->Numbers);
	while (Voltage_Items < (*VCCs)->Numbers) {
		*Index += 3;
		Check_Attribute("Name", "VOLTAGE");
		Value_Str = strndup(Json_File + Tokens[*Index].start,
				    Tokens[*Index].end - Tokens[*Index].start);
		Validate_Str_Size(Value_Str, "VOLTAGE", "Name", STRLEN_MAX);
		strncpy((*VCCs)->Voltage[Voltage_Items].Name, Value_Str, strlen(Value_Str) + 1);
		SC_INFO("Name: %s", (*VCCs)->Voltage[Voltage_Items].Name);

		(*Index)++;
		Check_Attribute("Part_Name", "VOLTAGE");
		Value_Str = strndup(Json_File + Tokens[*Index].start,
				    Tokens[*Index].end - Tokens[*Index].start);
		Validate_Str_Size(Value_Str, "VOLTAGE", "Part_Name", STRLEN_MAX);
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
		strncpy((*VCCs)->Voltage[Voltage_Items].I2C_Bus, Value_Str, strlen(Value_Str) + 1);
		SC_INFO("I2C Bus: %s", (*VCCs)->Voltage[Voltage_Items].I2C_Bus);

		(*Index)++;
		Check_Attribute("I2C_Address", "VOLTAGE");
		Value_Str = strndup(Json_File + Tokens[*Index].start,
				    Tokens[*Index].end - Tokens[*Index].start);
		SC_INFO("I2C Addr - Hex: %s", Value_Str);
		(*VCCs)->Voltage[Voltage_Items].I2C_Address = (int)strtol(Value_Str, NULL, 0);

		(*Index)++;
		Check_Attribute("Page_Select", "VOLTAGE");
		Value_Str = strndup(Json_File + Tokens[*Index].start,
				    Tokens[*Index].end - Tokens[*Index].start);
		(*VCCs)->Voltage[Voltage_Items].Page_Select = atoi(Value_Str);
		SC_INFO("Page Select: %i\n", (*VCCs)->Voltage[Voltage_Items].Page_Select);

		Voltage_Items++;
	}

	return 0;
}

int
Parse_DIMM(const char *Json_File, jsmntok_t *Tokens, int *Index, DIMM_t **DIMMs)
{
	char *Value_Str;

	SC_INFO("********************* DIMM *********************");
	*DIMMs = (DIMM_t *)malloc(sizeof(DIMM_t));

	*Index += 2;
	Check_Attribute("Name", "DIMM");
	Value_Str = strndup(Json_File + Tokens[*Index].start,
			    Tokens[*Index].end - Tokens[*Index].start);
	Validate_Str_Size(Value_Str, "DIMM", "Name", STRLEN_MAX);
	strncpy((*DIMMs)->Name, Value_Str, strlen(Value_Str) + 1);
	SC_INFO("Name: %s", (*DIMMs)->Name);

	(*Index)++;
	Check_Attribute("I2C_Bus", "DIMM");
	Value_Str = strndup(Json_File + Tokens[*Index].start,
			    Tokens[*Index].end - Tokens[*Index].start);
	Validate_Str_Size(Value_Str, "DIMM", "I2C_Bus", STRLEN_MAX);
	strncpy((*DIMMs)->I2C_Bus, Value_Str, strlen(Value_Str) + 1);
	SC_INFO("I2C Bus: %s", (*DIMMs)->I2C_Bus);

	SC_INFO("Spd Info -");
	(*Index)++;
	Check_Attribute("Spd", "DIMM");
	(*Index)++;
	Check_Attribute("Bus_addr", "Spd");
	Value_Str = strndup(Json_File + Tokens[*Index].start,
			    Tokens[*Index].end - Tokens[*Index].start);
	(*DIMMs)->Spd.Bus_addr = (int)strtol(Value_Str, NULL, 0);
	SC_INFO("Bus Addr: %u", (*DIMMs)->Spd.Bus_addr);

	(*Index)++;
	Check_Attribute("Reg_addr", "Spd");
	Value_Str = strndup(Json_File + Tokens[*Index].start,
			    Tokens[*Index].end - Tokens[*Index].start);
	(*DIMMs)->Spd.Reg_addr = atoi(Value_Str);
	SC_INFO("Reg Addr: %u", (*DIMMs)->Spd.Reg_addr);

	(*Index)++;
	Check_Attribute("Read_len", "Spd");
	Value_Str = strndup(Json_File + Tokens[*Index].start,
			    Tokens[*Index].end - Tokens[*Index].start);
	(*DIMMs)->Spd.Read_len = atoi(Value_Str);
	SC_INFO("Read len: %u", (*DIMMs)->Spd.Read_len);

	SC_INFO("Term Info -");
	(*Index)++;
	Check_Attribute("Therm", "DIMM");
	(*Index)++;
	Check_Attribute("Bus_addr", "Therm");
	Value_Str = strndup(Json_File + Tokens[*Index].start,
			    Tokens[*Index].end - Tokens[*Index].start);
	(*DIMMs)->Therm.Bus_addr = (int)strtol(Value_Str, NULL, 0);
	SC_INFO("Bus Addr: %u", (*DIMMs)->Therm.Bus_addr);

	(*Index)++;
	Check_Attribute("Reg_addr", "Therm");
	Value_Str = strndup(Json_File + Tokens[*Index].start,
			    Tokens[*Index].end - Tokens[*Index].start);
	(*DIMMs)->Therm.Reg_addr = atoi(Value_Str);
	SC_INFO("Reg Addr: %u", (*DIMMs)->Therm.Reg_addr);

	(*Index)++;
	Check_Attribute("Read_len", "Therm");
	Value_Str = strndup(Json_File + Tokens[*Index].start,
			    Tokens[*Index].end - Tokens[*Index].start);
	(*DIMMs)->Therm.Read_len = atoi(Value_Str);
	SC_INFO("Read len: %u", (*DIMMs)->Therm.Read_len);

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
	SC_INFO("Number of GPIO: %i", (*GPIOs)->Numbers);
	while (Items < (*GPIOs)->Numbers) {
		*Index += 3;
		Check_Attribute("Line", "GPIO");
		Value_Str = strndup(Json_File + Tokens[*Index].start,
				    Tokens[*Index].end - Tokens[*Index].start);
		(*GPIOs)->GPIO[Items].Line = atoi(Value_Str);
		SC_INFO("GPIO_%i\nLine: %i", Items, (*GPIOs)->GPIO[Items].Line);

		(*Index)++;
		Check_Attribute("Display_Name", "GPIO");
		Value_Str = strndup(Json_File + Tokens[*Index].start,
				    Tokens[*Index].end - Tokens[*Index].start);
		(*GPIOs)->GPIO[Items].Display_Name = Value_Str;
		SC_INFO("Display Name: %s", (*GPIOs)->GPIO[Items].Display_Name);

		(*Index)++;
		Check_Attribute("Internal_Name", "GPIO");
		Value_Str = strndup(Json_File + Tokens[*Index].start,
				    Tokens[*Index].end - Tokens[*Index].start);
		(*GPIOs)->GPIO[Items].Internal_Name = Value_Str;
		SC_INFO("Internal Name: %s", (*GPIOs)->GPIO[Items].Internal_Name);

		Items++;
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
	strncpy((*IEs)->Name, Value_Str, strlen(Value_Str) + 1);
	SC_INFO("Name: %s\n", (*IEs)->Name);

	(*Index)++;
	Check_Attribute("Labels", "IO Exp");
	(*IEs)->Numbers = Tokens[*Index].size;
	SC_INFO("Number of IO Exps: %i", (*IEs)->Numbers);
	SC_INFO("Labels -");
	while (Label < (*IEs)->Numbers) {
		(*Index)++;
		Value_Str = strndup(Json_File + Tokens[*Index].start,
				    Tokens[*Index].end - Tokens[*Index].start);
		Validate_Str_Size(Value_Str, "IO Exp", "Labels", STRLEN_MAX);
		strncpy((*IEs)->Labels[Label], Value_Str, strlen(Value_Str) + 1);
		SC_INFO("\t%s", (*IEs)->Labels[Label]);

		Label++;
	}

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
	strncpy((*DCs)->Name, Value_Str, strlen(Value_Str) + 1);
	SC_INFO("Name: %s", (*DCs)->Name);

	(*Index)++;
	Check_Attribute("I2C_Bus", "Daughter Card");
	Value_Str = strndup(Json_File + Tokens[*Index].start,
			    Tokens[*Index].end - Tokens[*Index].start);
	Validate_Str_Size(Value_Str, "Daughter Card", "I2C_Bus", STRLEN_MAX);
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
	SC_INFO("Number of SFPs: %i", (*SFPs)->Numbers);
	while (Item < (*SFPs)->Numbers) {
		*Index += 3;
		Check_Attribute("Name", "SFP");
		Value_Str = strndup(Json_File + Tokens[*Index].start,
				    Tokens[*Index].end - Tokens[*Index].start);
		Validate_Str_Size(Value_Str, "SFP", "Name", STRLEN_MAX);
		strncpy((*SFPs)->SFP[Item].Name, Value_Str, strlen(Value_Str) + 1);
		SC_INFO("Name: %s", (*SFPs)->SFP[Item].Name);

		(*Index)++;
		Check_Attribute("I2C_Bus", "SFP");
		Value_Str = strndup(Json_File + Tokens[*Index].start,
				    Tokens[*Index].end - Tokens[*Index].start);
		Validate_Str_Size(Value_Str, "SFP", "I2C_Bus", STRLEN_MAX);
		strncpy((*SFPs)->SFP[Item].I2C_Bus, Value_Str, strlen(Value_Str) + 1);
		SC_INFO("I2C Bus: %s", (*SFPs)->SFP[Item].I2C_Bus);

		(*Index)++;
		Check_Attribute("I2C_Address", "SFP");
		Value_Str = strndup(Json_File + Tokens[*Index].start,
				    Tokens[*Index].end - Tokens[*Index].start);
		SC_INFO("I2C Addr: %s", Value_Str);
		(*SFPs)->SFP[Item].I2C_Address = (int)strtol(Value_Str, NULL, 0);

		Item++;
	}

	return 0;
}

int
Parse_QSFP(const char *Json_File, jsmntok_t *Tokens, int *Index, QSFPs_t **QSFPs)
{
	char *Value_Str;
	int Item = 0;

	SC_INFO("******************** QSFPs ********************");
	*QSFPs = (QSFPs_t *)malloc(sizeof(QSFPs_t));

	(*Index)++;
	(*QSFPs)->Numbers = Tokens[*Index].size;
	SC_INFO("Number of QSFPs: %i", (*QSFPs)->Numbers);
	while (Item < (*QSFPs)->Numbers) {
		*Index += 3;
		Check_Attribute("Name", "QSFP");
		Value_Str = strndup(Json_File + Tokens[*Index].start,
				    Tokens[*Index].end - Tokens[*Index].start);
		Validate_Str_Size(Value_Str, "QSFP", "Name", STRLEN_MAX);
		strncpy((*QSFPs)->QSFP[Item].Name, Value_Str, strlen(Value_Str) + 1);
		SC_INFO("Name: %s", (*QSFPs)->QSFP[Item].Name);

		(*Index)++;
		Check_Attribute("Type", "QSFP");
		Value_Str = strndup(Json_File + Tokens[*Index].start,
				    Tokens[*Index].end - Tokens[*Index].start);
		SC_INFO("Type: %s", Value_Str);
		if (strcmp(Value_Str, "qsfp") == 0) {
			(*QSFPs)->QSFP[Item].Type = qsfp;
		} else if (strcmp(Value_Str, "qsfpdd") == 0) {
			(*QSFPs)->QSFP[Item].Type = qsfpdd;
		}

		(*Index)++;
		Check_Attribute("I2C_Bus", "QSFP");
		Value_Str = strndup(Json_File + Tokens[*Index].start,
				    Tokens[*Index].end - Tokens[*Index].start);
		Validate_Str_Size(Value_Str, "QSFP", "I2C_Bus", STRLEN_MAX);
		strncpy((*QSFPs)->QSFP[Item].I2C_Bus, Value_Str, strlen(Value_Str) + 1);
		SC_INFO("I2C Bus: %s", (*QSFPs)->QSFP[Item].I2C_Bus);

		(*Index)++;
		Check_Attribute("I2C_Address", "QSFP");
		Value_Str = strndup(Json_File + Tokens[*Index].start,
				    Tokens[*Index].end - Tokens[*Index].start);
		SC_INFO("I2C Addr: %s", Value_Str);
		(*QSFPs)->QSFP[Item].I2C_Address = (int)strtol(Value_Str, NULL, 0);

		Item++;
	}

	return 0;
}

int
Parse_FMC(const char *Json_File, jsmntok_t *Tokens, int *Index, FMCs_t **FMCs)
{
	char *Value_Str;
	int Item = 0;

	SC_INFO("******************** FMCs ********************");
	*FMCs = (FMCs_t *)malloc(sizeof(FMCs_t));

	(*Index)++;
	(*FMCs)->Numbers = Tokens[*Index].size;
	SC_INFO("Number of FMCs: %i", (*FMCs)->Numbers);
	while (Item < (*FMCs)->Numbers) {
		*Index += 3;
		Check_Attribute("Name", "FMC");
		Value_Str = strndup(Json_File + Tokens[*Index].start,
				    Tokens[*Index].end - Tokens[*Index].start);
		Validate_Str_Size(Value_Str, "FMC", "Name", STRLEN_MAX);
		strncpy((*FMCs)->FMC[Item].Name, Value_Str, strlen(Value_Str) + 1);
		SC_INFO("Name: %s", (*FMCs)->FMC[Item].Name);

		(*Index)++;
		Check_Attribute("I2C_Bus", "FMC");
		Value_Str = strndup(Json_File + Tokens[*Index].start,
				    Tokens[*Index].end - Tokens[*Index].start);
		Validate_Str_Size(Value_Str, "FMC", "I2C_Bus", STRLEN_MAX);
		strncpy((*FMCs)->FMC[Item].I2C_Bus, Value_Str, strlen(Value_Str) + 1);
		SC_INFO("I2C Bus: %s", (*FMCs)->FMC[Item].I2C_Bus);

		(*Index)++;
		Check_Attribute("I2C_Address", "FMC");
		Value_Str = strndup(Json_File + Tokens[*Index].start,
				    Tokens[*Index].end - Tokens[*Index].start);
		SC_INFO("I2C Addr: %s", Value_Str);
		(*FMCs)->FMC[Item].I2C_Address = (int)strtol(Value_Str, NULL, 0);

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
	SC_INFO("Number of Workarounds: %i", (*WAs)->Numbers);
	while (Item < (*WAs)->Numbers) {
		*Index += 3;
		Check_Attribute("Name", "WORKAROUND");
		Value_Str = strndup(Json_File + Tokens[*Index].start,
				    Tokens[*Index].end - Tokens[*Index].start);
		Validate_Str_Size(Value_Str, "WORKAROUND", "Name", STRLEN_MAX);
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
	SC_INFO("Number of BITs: %i", (*BITs)->Numbers);
	while (Item < (*BITs)->Numbers) {
		*Index += 3;
		Check_Attribute("Name", "BITs");
		Value_Str = strndup(Json_File + Tokens[*Index].start,
				    Tokens[*Index].end - Tokens[*Index].start);
		Temp = &(*BITs)->BIT[Item];
		Validate_Str_Size(Value_Str, "BITs", "Name", STRLEN_MAX);
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
				strncpy(Temp->Level[Level].Instruction, Value_Str, strlen(Value_Str) + 1);
				SC_INFO("Instruction: %s", Temp->Level[Level].Instruction);
			}

			Level++;
		}

		Item++;
	}

	return 0;
}
