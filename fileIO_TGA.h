#ifndef	FILE_IO_TGA_H
#define	FILE_IO_TGA_H

#include "fileIO.h"

unsigned char* readTGA(const char* fileName, int* nbCols, int* nbRows, ImageFileType* theType);
int writeTGA(char* fileName, unsigned char* theData, int nbRows, int nbCols);

#endif
