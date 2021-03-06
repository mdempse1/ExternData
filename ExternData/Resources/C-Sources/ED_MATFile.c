/* ED_MATFile.c - MAT functions
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

#include <string.h>
#include <stdio.h>
#if defined(_MSC_VER)
#define strdup _strdup
#endif
#include "ModelicaUtilities.h"
#include "ModelicaIO.c"
#include "../Include/ED_MATFile.h"

typedef struct {
	char* fileName;
	int verbose;
} MATFile;

void* ED_createMAT(const char* fileName, int verbose)
{
	MATFile* mat = (MATFile*)malloc(sizeof(MATFile));
	if (mat == NULL) {
		ModelicaError("Memory allocation error\n");
		return NULL;
	}
	mat->fileName = strdup(fileName);
	if (mat->fileName == NULL) {
		free(mat);
		ModelicaError("Memory allocation error\n");
		return NULL;
	}
	mat->verbose = verbose;

	return mat;
}

void ED_destroyMAT(void* _mat)
{
	MATFile* mat = (MATFile*)_mat;
	if (mat != NULL) {
		if (mat->fileName != NULL) {
			free(mat->fileName);
		}
		free(mat);
	}
}

void ED_getDoubleArray2DFromMAT(void* _mat, const char* varName, double* a, size_t m, size_t n)
{
	MATFile* mat = (MATFile*)_mat;
	if (mat != NULL) {
		ModelicaIO_readRealMatrix(mat->fileName, varName, a, m, n, mat->verbose);
	}
}

void ED_getStringArray1DFromMAT(void* _mat, const char* varName, const char* string[], size_t m)
{
	MATFile* mat = (MATFile*)_mat;
	if (mat != NULL) {
		MatIO matio = {NULL, NULL, NULL};

		if (mat->verbose == 1) {
			/* Print info message, that matrix / file is loading */
			ModelicaFormatMessage("... loading \"%s\" from \"%s\"\n", varName, mat->fileName);
		}

		readMatIO(mat->fileName, varName, &matio);
		if (NULL != matio.matvar) {
			matvar_t* matvar = matio.matvar;
			size_t nRow, nCol, i;

			/* Check if matvar is of character class */
			if (matvar->class_type != MAT_C_CHAR) {
				Mat_VarFree(matio.matvarRoot);
				(void)Mat_Close(matio.mat);
				ModelicaFormatError("2D array \"%s\" has not the required "
					"character class.\n", varName);
				return;
			}

			nRow = matvar->dims[0];
			nCol = matvar->dims[1];

			/* Check number of rows */
			if (m > nRow) {
				Mat_VarFree(matio.matvarRoot);
				(void)Mat_Close(matio.mat);
				ModelicaFormatError(
					"Cannot read %lu rows of array \"%s(%lu,%lu)\" "
					"from file \"%s\"\n", (unsigned long)m, varName,
					(unsigned long)nRow, (unsigned long)nCol, mat->fileName);
				return;
			}

			(void)Mat_VarReadDataAll(matio.mat, matvar);

			for (i = 0; i < m; i++) {
				char* str = ModelicaAllocateStringWithErrorReturn(nCol);
				if (NULL != str) {
					size_t j;
					/* Array is stored column-wise -> need to transpose */
					for (j = 0; j < nCol; j++) {
						str[j] = ((char*)matvar->data)[i + j*nRow];
					}
					str[nCol] = '\0';
					string[i] = str;
				}
				else {
					Mat_VarFree(matio.matvarRoot);
					(void)Mat_Close(matio.mat);
					ModelicaError("Memory allocation error\n");
					return;
				}
			}

			Mat_VarFree(matio.matvarRoot);
			(void)Mat_Close(matio.mat);
		}
	}
}
