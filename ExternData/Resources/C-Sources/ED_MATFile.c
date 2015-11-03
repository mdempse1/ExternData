/* ED_MATFile.c - MAT functions
 *
 * Copyright (C) 2015, tbeu
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
#include "ModelicaMatIO.h"
#include "../Include/ED_MATFile.h"

typedef struct {
	char* fileName;
} MATFile;

void* ED_createMAT(const char* fileName)
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

static void transpose(double* table, size_t nRow, size_t nCol)
{
	/* Reference:

	   Cycle-based in-place array transposition
	   (http://en.wikipedia.org/wiki/In-place_matrix_transposition#Non-square_matrices:_Following_the_cycles)
	 */

	size_t i;
	for (i = 1; i < nRow*nCol - 1; i++) {
		size_t x = nRow*(i % nCol) + i/nCol; /* predecessor of i in the cycle */
		/* Continue if cycle is of length one or predecessor already was visited */
		if (x <= i) {
			continue;
		}
		/* Continue if cycle already was visited */
		while (x > i) {
			x = nRow*(x % nCol) + x/nCol;
		}
		if (x < i) {
			continue;
		}
		{
			double tmp = table[i];
			size_t s = i; /* start index in the cycle */
			x = nRow*(i % nCol) + i/nCol; /* predecessor of i in the cycle */
			while (x != i) {
				table[s] = table[x];
				s = x;
				x = nRow*(x % nCol) + x/nCol;
			}
			table[s] = tmp;
		}
	}
}

void ED_getDimDoubleArray2DFromMAT(void* _mat, const char* varName, int* dim)
{
	MATFile* edmat = (MATFile*)_mat;
	if (edmat != NULL) {
		mat_t* matfp;
		matvar_t* matvar;

		matfp = Mat_Open(edmat->fileName, (int)MAT_ACC_RDONLY);
		if (matfp == NULL) {
			dim[0] = 0;
			dim[1] = 0;
			ModelicaFormatError("Not possible to open file \"%s\": "
				"No such file or directory\n", edmat->fileName);
			return;
		}

		matvar = Mat_VarReadInfo(matfp, varName);
		if (matvar == NULL) {
			dim[0] = 0;
			dim[1] = 0;
			(void)Mat_Close(matfp);
			ModelicaFormatError(
				"Variable \"%s\" not found on file \"%s\".\n", varName,
				edmat->fileName);
			return;
		}

		/* Check if array is a matrix */
		if (matvar->rank != 2) {
			dim[0] = 0;
			dim[1] = 0;
			Mat_VarFree(matvar);
			(void)Mat_Close(matfp);
			ModelicaFormatError(
				"Array \"%s\" has not the required rank 2.\n", varName);
			return;
		}

		dim[0] = (int)matvar->dims[0];
		dim[1] = (int)matvar->dims[1];

		Mat_VarFree(matvar);
		(void)Mat_Close(matfp);
	}
}

void ED_getDoubleArray2DFromMAT(void* _mat, const char* varName, double* a, size_t m, size_t n)
{
	MATFile* edmat = (MATFile*)_mat;
	if (edmat != NULL) {
		mat_t* matfp;
		matvar_t* matvar;
		size_t nRow, nCol;
		int tableReadError = 0;

		matfp = Mat_Open(edmat->fileName, (int)MAT_ACC_RDONLY);
		if (matfp == NULL) {
			ModelicaFormatError("Not possible to open file \"%s\": "
				"No such file or directory\n", edmat->fileName);
			return;
		}

		matvar = Mat_VarReadInfo(matfp, varName);
		if (matvar == NULL) {
			(void)Mat_Close(matfp);
			ModelicaFormatError(
				"Variable \"%s\" not found on file \"%s\".\n", varName,
				edmat->fileName);
			return;
		}

		/* Check if array is a matrix */
		if (matvar->rank != 2) {
			Mat_VarFree(matvar);
			(void)Mat_Close(matfp);
			ModelicaFormatError(
				"Array \"%s\" has not the required rank 2.\n", varName);
			return;
		}

		/* Check if array is of double precision class (and thus non-sparse) */
		if (matvar->class_type != MAT_C_DOUBLE) {
			Mat_VarFree(matvar);
			(void)Mat_Close(matfp);
			ModelicaFormatError("2D array \"%s\" has not the required "
				"double precision class.\n", varName);
			return;
		}

		/* Check if array is purely real-valued */
		if (matvar->isComplex) {
			Mat_VarFree(matvar);
			(void)Mat_Close(matfp);
			ModelicaFormatError("2D array  \"%s\" must not be complex.\n",
				varName);
			return;
		}

		nRow = matvar->dims[0];
		nCol = matvar->dims[1];

		/* Check if number of rows matches */
		if (m != nRow) {
			Mat_VarFree(matvar);
			(void)Mat_Close(matfp);
			ModelicaFormatError(
				"Cannot read %lu rows of matrix \"%s(%lu,%lu)\" "
				"from file \"%s\"\n", (unsigned long)m, varName,
				(unsigned long)nRow, (unsigned long)nCol, edmat->fileName);
			return;
		}

		/* Check if number of columns matches */
		if (n != nCol) {
			Mat_VarFree(matvar);
			(void)Mat_Close(matfp);
			ModelicaFormatError(
				"Cannot read %lu columns of matrix \"%s(%lu,%lu)\" "
				"from file \"%s\"\n", (unsigned long)n, varName,
				(unsigned long)nRow, (unsigned long)nCol, edmat->fileName);
			return;
		}

		{
			int start[2] = {0, 0};
			int stride[2] = {1, 1};
			int edge[2];
			edge[0] = (int)nRow;
			edge[1] = (int)nCol;
			tableReadError = Mat_VarReadData(matfp, matvar, a, start, stride, edge);
		}

		Mat_VarFree(matvar);
		(void)Mat_Close(matfp);

		if (tableReadError == 0) {
			/* Array is stored column-wise -> need to transpose */
			transpose(a, nRow, nCol);
		}
		else {
			ModelicaFormatError(
				"Error when reading numeric data of matrix \"%s(%lu,%lu)\" "
				"from file \"%s\"\n", varName, (unsigned long)nRow,
				(unsigned long)nCol, edmat->fileName);
			return;
		}
	}
}

int ED_writeDoubleArray2DToMAT(void* _mat, const char* varName, double* a, size_t m, size_t n, int append)
{
	int status = 0;
	MATFile* mat = (MATFile*)_mat;
	if (mat != NULL) {
		mat_t* matfp;
		matvar_t* matvar;
		size_t dims[2];
		double* aT;

		matfp = append == 0 ?
			Mat_CreateVer(mat->fileName, NULL, MAT_FT_MAT4) :
			Mat_Open(mat->fileName, (int)MAT_ACC_RDWR | MAT_FT_MAT4);

		if (matfp == NULL) {
			ModelicaFormatError("Not possible to open file \"%s\"\n", mat->fileName);
			return 0;
		}

		if (append != 0) {
			(void)Mat_VarDelete(matfp, varName);
		}

		/* MAT file array is stored column-wise -> need to transpose */
		aT = (double*)malloc(m*n*sizeof(double));
		if (aT == NULL) {
			(void)Mat_Close(matfp);
			ModelicaError("Memory allocation error\n");
			return 0;
		}
		memcpy(aT, a, m*n*sizeof(double));
		transpose(aT, n, m);

		dims[0] = m;
		dims[1] = n;
		matvar = Mat_VarCreate(varName, MAT_C_DOUBLE, MAT_T_DOUBLE, 2, dims, aT, MAT_F_DONT_COPY_DATA);
		status = Mat_VarWrite(matfp, matvar, MAT_COMPRESSION_ZLIB);
		if (status != 0) {
			Mat_VarFree(matvar);
			(void)Mat_Close(matfp);
			free(aT);
			ModelicaFormatError("Cannot write variable \"%s\" to \"%s\"\n", varName, mat->fileName);
			return 0;
		}
		Mat_VarFree(matvar);
		(void)Mat_Close(matfp);
		free(aT);
		status = 1;
	}
	return status;
}
