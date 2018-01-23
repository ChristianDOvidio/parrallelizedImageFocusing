#ifndef	FILE_IO_H
#define	FILE_IO_H

typedef enum imageFileType
{
		kUnknownType = -1,
		kTGA_COLOR,				//	24-bit color image
		kTGA_GRAY,
		kPPM,					//	24-bit color image
		kPGM					//	8-bit gray-level image
} ImageFileType;

typedef	struct ImageInfoStruct {
									unsigned char* raster;
									//unsigned char** raster2D;
									int	nbCols;
									int nbRows;
									int bytesPerPixel;
									int bytesPerRow;
} ImageStruct;


#endif
