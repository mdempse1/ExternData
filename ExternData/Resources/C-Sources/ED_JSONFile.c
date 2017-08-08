/* ED_JSONFile.c - JSON functions
 *
 * Copyright (C) 2015-2017, tbeu
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are met:
 *
 * 1. Redistributions of source code must retain the above copyright notice,
 *    this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS" AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
 * DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR
 * SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER
 * CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY,
 * OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#if defined(__gnu_linux__)
#define _GNU_SOURCE 1
#endif

#include <string.h>
#if defined(_MSC_VER)
#define strdup _strdup
#endif
#include "ED_locale.h"
#include "bsjson.h"
#include "ModelicaUtilities.h"
#include "../Include/ED_JSONFile.h"

/* The standard way to detect posix is to check _POSIX_VERSION,
 * which is defined in <unistd.h>
 */
#if defined(__unix__) || defined(__linux__) || defined(__APPLE_CC__)
#include <unistd.h>
#endif
#if !defined(_POSIX_) && defined(_POSIX_VERSION)
#define _POSIX_ 1
#endif

/* Use re-entrant string tokenize function if available */
#if defined(_POSIX_)
#elif defined(_MSC_VER) && _MSC_VER >= 1400
#define strtok_r(str, delim, saveptr) strtok_s((str), (delim), (saveptr))
#else
#define strtok_r(str, delim, saveptr) strtok((str), (delim))
#endif

typedef struct {
	char* fileName;
	JsonNodeRef root;
	ED_LOCALE_TYPE loc;
} JSONFile;

void* ED_createJSON(const char* fileName, int verbose)
{
	JsonParser jsonParser;
	JSONFile* json = (JSONFile*)malloc(sizeof(JSONFile));
	if (json == NULL) {
		ModelicaError("Memory allocation error\n");
		return NULL;
	}
	json->fileName = strdup(fileName);
	if (json->fileName == NULL) {
		free(json);
		ModelicaError("Memory allocation error\n");
		return NULL;
	}

	if (verbose == 1) {
		/* Print info message, that file is loading */
		ModelicaFormatMessage("... loading \"%s\"\n", fileName);
	}

	json->root = JsonParser_parseFile(&jsonParser, fileName);
	if (json->root == NULL) {
		free(json->fileName);
		free(json);
		if (JsonParser_getErrorLineSet(&jsonParser) != 0) {
			ModelicaFormatError("Error \"%s\" in line %lu: Cannot parse file \"%s\"\n",
				JsonParser_getErrorString(&jsonParser), JsonParser_getErrorLine(&jsonParser), fileName);
		}
		else {
			ModelicaFormatError("Cannot read \"%s\": %s\n", fileName, JsonParser_getErrorString(&jsonParser));
		}
		return NULL;
	}
	json->loc = ED_INIT_LOCALE;
	return json;
}

void ED_destroyJSON(void* _json)
{
	JSONFile* json = (JSONFile*)_json;
	if (json != NULL) {
		if (json->fileName != NULL) {
			free(json->fileName);
		}
		JsonNode_deleteTree(json->root);
		ED_FREE_LOCALE(json->loc);
		free(json);
	}
}

static char* findValue(JsonNodeRef* root, const char* varName, const char* fileName)
{
	char* token = NULL;
	char* buf = strdup(varName);
	if (buf != NULL) {
		char* key = NULL;
		char* nextToken = NULL;
		token = strtok_r(buf, ".", &nextToken);
		while (token != NULL) {
			JsonNodeRef iter = JsonNode_findChild(*root, token, JSON_OBJ);
			if (NULL != iter) {
				*root = iter;
				token = strtok_r(NULL, ".", &nextToken);
			}
			else {
				key = token;
				token = strtok_r(NULL, ".", &nextToken);
				break;
			}
		}
		if (NULL != key && NULL != *root && NULL == token) {
			token = JsonNode_getPairValue(*root, key);
			free(buf);
			if (NULL == token) {
				ModelicaFormatMessage("Cannot read element \"%s\" from file \"%s\"\n",
					varName, fileName);
				*root = NULL;
			}
		}
		else {
			free(buf);
			ModelicaFormatMessage("Cannot read element \"%s\" from file \"%s\"\n",
				varName, fileName);
			*root = NULL;
			token = NULL;
		}
	}
	else {
		ModelicaError("Memory allocation error\n");
	}
	return token;
}

double ED_getDoubleFromJSON(void* _json, const char* varName, int* exist)
{
	double ret = 0.;
	JSONFile* json = (JSONFile*)_json;
	*exist = 1;
	if (json != NULL) {
		JsonNodeRef root = json->root;
		char* token = findValue(&root, varName, json->fileName);
		if (token != NULL) {
			if (ED_strtod(token, json->loc, &ret)) {
				ModelicaFormatError("Cannot read double value \"%s\" from file \"%s\"\n",
					token, json->fileName);
			}
		}
		else if (NULL != root) {
			ModelicaFormatMessage("Cannot read double value from file \"%s\"\n",
				json->fileName);
			*exist = 0;
		}
		else {
			*exist = 0;
		}
	}
	return ret;
}

const char* ED_getStringFromJSON(void* _json, const char* varName, int* exist)
{
	JSONFile* json = (JSONFile*)_json;
	*exist = 1;
	if (json != NULL) {
		JsonNodeRef root = json->root;
		char* token = findValue(&root, varName, json->fileName);
		if (token != NULL) {
			char* ret = ModelicaAllocateString(strlen(token));
			strcpy(ret, token);
			return (const char*)ret;
		}
		else if (NULL != root) {
			ModelicaFormatMessage("Cannot read value from file \"%s\"\n",
				json->fileName);
			*exist = 0;
		}
		else {
			*exist = 0;
		}
	}
	return "";
}

int ED_getIntFromJSON(void* _json, const char* varName, int* exist)
{
	long ret = 0;
	JSONFile* json = (JSONFile*)_json;
	*exist = 1;
	if (json != NULL) {
		JsonNodeRef root = json->root;
		char* token = findValue(&root, varName, json->fileName);
		if (token != NULL) {
			if (ED_strtol(token, json->loc, &ret)) {
				ModelicaFormatError("Cannot read int value \"%s\" from file \"%s\"\n",
					token, json->fileName);
			}
		}
		else if (NULL != root) {
			ModelicaFormatMessage("Cannot read int value from file \"%s\"\n",
				json->fileName);
			*exist = 0;
		}
		else {
			*exist = 0;
		}
	}
	return (int)ret;
}
