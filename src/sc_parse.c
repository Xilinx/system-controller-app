/*
 * Copyright (c) 2021 Xilinx, Inc.  All rights reserved.
 *
 * SPDX-License-Identifier: MIT
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "jsmn.h"
#include "sc_app.h"

#define JSMN_TOKENS_SIZE 2048

char Board_File[STRLEN_MAX];

int
Parse_JSON(const char *Board_Name, Plat_Devs_t *Dev_Parse) {
	int Parse_Result;
	int Token_Size = JSMN_TOKENS_SIZE;
	FILE *FP;
	char *Json_File;
	long Char_Len;
	jsmn_parser Parser;
	jsmntok_t Tokens[Token_Size];

	jsmn_init(&Parser);

	sprintf(Board_File, "%s.json", Board_Name);

	if (strcmp(Board_File, "VMK180.json") == 0) {
		strcpy(Board_File, "VCK190.json");
	}

	FP = fopen(Board_File, "r");

	if (FP == NULL) {
		SC_ERR("Error opening file");
		return -1;
	}

	fseek(FP, 0, SEEK_END);
	Char_Len = ftell(FP);
	rewind(FP);

	Json_File = (char *)malloc(Char_Len * sizeof(char));
	if (Char_Len != fread(Json_File, sizeof(char), Char_Len, FP)) {
		SC_ERR("Error reading file.");
        return -1;
	}

	Parse_Result = jsmn_parse(&Parser, Json_File, strlen(Json_File), Tokens, Token_Size);

	if (Parse_Result < 1) {
		SC_ERR("Failed to parse JSON - Error Code: %d", Parse_Result);

		if (Parse_Result == 0 || Tokens[0].type != JSMN_OBJECT) {
			SC_ERR("Object expected!");
		} else if (Parse_Result == -1) {
			SC_ERR("Insufficient JSMN tokens provided.");
		} else if (Parse_Result == -2) {
			SC_ERR("Invalid character inside json string.");
		} else if (Parse_Result == -3) {
			SC_ERR("String is not a complete JSON packet, more bytes expected.");
		}

		return -1;
	}

    return 0;
}
