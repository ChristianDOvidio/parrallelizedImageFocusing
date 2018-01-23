#include <stdio.h>
#include <stdlib.h>
#include <time.h>
#include <dirent.h>
#include <string.h>
#include <pthread.h>
#include <inttypes.h>
//
#include "gl_frontEnd.h"
#include "fileIO_TGA.h"

//==================================================================================
//	Function prototypes
//==================================================================================
void displayImagePane(void);
void displayStatePane(void);

void myKeyboard(unsigned char c, int x, int y);
void initializeApplication(void);
void *imageCorrect(void *argument);
ImageStruct readImage(char *filePath);

typedef struct threadInfo
{
	pthread_t threadID;
	int index;
	int rowStart;
	int rowEnd;
	int colStart;
	int colEnd;
	unsigned char *raster;
	ImageStruct *outputImage;
} threadInfo;

//==================================================================================
//	Application-level global variables
//==================================================================================

//	Don't touch. These are defined in the front end source code
extern const int IMAGE_PANE, STATE_PANE;
extern int gMainWindow, gSubwindow[2];
extern const int IMAGE_PANE_WIDTH, IMAGE_PANE_HEIGHT;

//	Don't rename any of these variables/constants
//--------------------------------------------------
int numLiveThreads = 0; //	the number of live focusing threads

//	An array of C-string where you can store things you want displayed in the spate pane
//	that you want the state pane to display (for debugging purposes?)
//	Dont change the dimensions as this may break the front end
//	I preallocate the max number of messages at the max message
//	length.  This goes against some of my own principles about
//	good programming practice, but I do that so that you can
//	change the number of messages and their content "on the fly,"
//	at any point during the execution of your program, whithout
//	having to worry about allocation and resizing.
const int MAX_NUM_MESSAGES = 3;
const int MAX_LENGTH_MESSAGE = 32;
char **message;
int numMessages;
time_t launchTime;

//	This is the image that you should be writing into.  In this
//	handout, I simply read one of the input images into it.
//	You should not rename this variable unless you plan to mess
//	with the display code.
ImageStruct imageOut;
float scaleX, scaleY;

//------------------------------------------------------------------
//	The variables defined here are for you to modify and add to
//------------------------------------------------------------------

int rectFind(int x, int y);

#define IN_PATH "/input/path/"
#define OUT_PATH "/output/path/"

#define desiredColumns 2
#define desiredRows 2
#define windowSize 13 //window should be 11 or 13

ImageStruct *imageReadArray;								//array to hold images to process
pthread_mutex_t myDataLock[(desiredRows * desiredColumns)]; //mutex to lock data
struct timespec startingTime, endingTime;					//used to time runtime in uS
char inputImageList[60][128];								//array of image names, max of 60 files, 128 character filename
char charArray[100];										//string to use for testing
DIR *directory;												//directory variable to open IN_PATH
struct dirent *dir;											//dir varaible to deal with files in code
int nameLength = 0;											//used for filename parsing
int inputImageCount = 0;									//number of images to be looked at
int numThreads;												//number of threads to create
int y = 0;													//simple iterator variable

//==================================================================================
//	These are the functions that tie the computation with the rendering.
//	Some parts are "don't touch."  Other parts need your intervention
//	to make sure that access to critical section is properly synchronized
//==================================================================================

//	I can't see any reason why you may need/want to change this
//	function
void displayImagePane(void)
{
	//==============================================
	//	This is OpenGL/glut magic.  Don't touch
	//==============================================
	glutSetWindow(gSubwindow[IMAGE_PANE]);
	glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
	glMatrixMode(GL_MODELVIEW);
	glLoadIdentity();
	glPixelZoom(scaleX, scaleY);

	//==============================================
	//	This is OpenGL/glut magic.  Don't touch
	//==============================================
	glDrawPixels(imageOut.nbCols, imageOut.nbRows,
				 GL_RGBA,
				 GL_UNSIGNED_BYTE,
				 imageOut.raster);
	glutSwapBuffers();
	glutSetWindow(gMainWindow);
}

void displayStatePane(void)
{
	//==============================================
	//	This is OpenGL/glut magic.  Don't touch
	//==============================================
	glutSetWindow(gSubwindow[STATE_PANE]);
	glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
	glMatrixMode(GL_MODELVIEW);
	glLoadIdentity();

	//--------------------------------------------------------
	//	stuff to replace or remove.
	//--------------------------------------------------------
	//	Here I hard-code a few messages that I want to see displayed
	//	in my state pane.  The number of live focusing threads will
	//	always get displayed.  No need to pass a message about it.
	time_t currentTime = time(NULL);
	numMessages = MAX_NUM_MESSAGES;
	sprintf(message[0], "System time: %ld", currentTime);
	sprintf(message[1], "Time since launch: %ld", currentTime - launchTime);
	//calculate runtime in millisecs
	uint64_t delta_us = (endingTime.tv_sec - startingTime.tv_sec) * 1000000 + (endingTime.tv_nsec - startingTime.tv_nsec) / 1000;
	sprintf(message[2], "Runtime: %" PRIu64 "ms\n", delta_us);

	//---------------------------------------------------------
	//	This is the call that makes OpenGL render information
	//	about the state of the simulation.
	//	You may have to synchronize this call if you run into
	//	problems, but really the OpenGL display is a hack for
	//	you to get a peek into what's happening.
	//---------------------------------------------------------
	drawState(numMessages, message);

	//==============================================
	//	This is OpenGL/glut magic.  Don't touch
	//==============================================
	glutSwapBuffers();
	glutSetWindow(gMainWindow);
}

//	This callback function is called when a keyboard event occurs
//	You can change things here if you want to have keyboard input
//
void myKeyboard(unsigned char c, int x, int y)
{
	int ok = 0;

	switch (c)
	{
	//	'esc' to quit
	case 27:
		exit(0);
		break;

	default:
		ok = 1;
		break;
	}
	if (!ok)
	{
		//	do something?
	}

	//==============================================
	//	This is OpenGL/glut magic.  Don't touch
	//==============================================
	glutSetWindow(gMainWindow);
	glutPostRedisplay();
}

//	If you have a modified/enhanced version of the image reading/writing
// 	code, then feel free to use it to replace the one included, but you
//	shouldn't *have* to change this.
ImageStruct readImage(char *filePath)
{
	int nbRows, nbCols;
	ImageFileType imgType;

	unsigned char *myData = readTGA(filePath, &nbRows, &nbCols, &imgType);

	ImageStruct img;
	if (myData != NULL)
	{
		img.raster = myData;
		img.nbCols = nbCols;
		img.nbRows = nbRows;
		if (imgType == kTGA_COLOR)
			//	all my color images are stored on 4 bytes:  RGBA
			img.bytesPerPixel = 4;
		else
			img.bytesPerPixel = 1;

		img.bytesPerRow = img.bytesPerPixel * nbCols;
	}
	else
	{
		printf("Reading of image file failed.\n");
		exit(-1);
	}

	return img;
}

//------------------------------------------------------------------------
//	You shouldn't have to change anything in the main function.
//------------------------------------------------------------------------
int main(int argc, char **argv)
{
	//	Even though we extracted the relevant information from the argument
	//	list, I still need to pass argc and argv to the front-end init
	//	function because that function passes them to glutInit, the required call
	//	to the initialization of the glut library.
	initializeFrontEnd(argc, argv, displayImagePane, displayStatePane, myKeyboard);

	//	Now we can do application-level initialization
	initializeApplication();

	pthread_t focusThread[numThreads];
	threadInfo threadInfo[numThreads]; //create an array of threads

	int width = imageReadArray[0].nbCols / desiredColumns; //calculate width of thread window
	int height = imageReadArray[0].nbRows / desiredRows;   //calculate height of thread window
	y = 0;												   //used to assign therad ID's

	// //initialize array of the mutex locks.  Here, I use the default initialization
	// //(the mutex is unlocked, so the first thread to ask for access will
	// //gain it.)
	for (int k = 0; k < numThreads; k++)
	{
		//printf("Initializing datalock%d\n", k);
		pthread_mutex_init(myDataLock + k, NULL);
	}

	for (int k = 0; k < desiredRows; ++k)
	{
		for (int j = 0; j < desiredColumns; ++j)
		{ //iterate over all sub-rectangles of image
			threadInfo[y].index = y;
			threadInfo[y].rowStart = k * height;
			threadInfo[y].rowEnd = (k + 1) * height;
			if (threadInfo[y].rowEnd > imageReadArray[0].nbRows)
				threadInfo[y].rowEnd = imageReadArray[0].nbRows;

			threadInfo[y].colStart = j * width;
			threadInfo[y].colEnd = (j + 1) * width;
			if (threadInfo[y].colEnd > imageReadArray[0].nbCols)
				threadInfo[y].colEnd = imageReadArray[0].nbCols;

			threadInfo[y].raster = imageOut.raster;
			threadInfo[y].outputImage = &imageOut;
			pthread_create(focusThread + y, NULL, imageCorrect, threadInfo + y);
			//printf("Thread %d created.\n", y);
			y++;
		}
	}

	//	Now, wait for the threads to terminate
	for (int y = 0; y < numThreads; y++)
	{
		//	I need a type long enough to store a pointer value, even if I am
		//	going to ignore the return value, in this case.
		char *val;
		pthread_join(focusThread[y], (void **)&val);
		//printf("Thread %d joined.\n", y);
	}

	clock_gettime(CLOCK_MONOTONIC_RAW, &endingTime); //stop the timer here

	//==============================================
	//	This is OpenGL/glut magic.  Don't touch
	//==============================================
	//	Now we enter the main loop of the program and to a large extend
	//	"lose control" over its execution.  The callback functions that
	//	we set up earlier will be called when the corresponding event
	//	occurs
	glutMainLoop();

	//	Free allocated resource before leaving (not absolutely needed, but
	//	just nicer.  Also, if you crash there, you know something is wrong
	//	in your code.
	for (int k = 0; k < MAX_NUM_MESSAGES; k++)
		free(message[k]);
	free(message);

	//	This will probably never be executed (the exit point will be in one of the
	//	call back functions).
	return 0;
}

//==================================================================================
//	This is a part that you have to edit and add to, for example to
//	load a complete stack of images and initialize the output image
//	(right now it is initialized simply by reading an image into it.
//==================================================================================

void initializeApplication(void)
{
	//	I preallocate the max number of messages at the max message
	//	length.  This goes against some of my own principles about
	//	good programming practice, but I do that so that you can
	//	change the number of messages and their content "on the fly,"
	//	at any point during the execution of your program, whithout
	//	having to worry about allocation and resizing.
	message = (char **)malloc(MAX_NUM_MESSAGES * sizeof(char *));
	for (int k = 0; k < MAX_NUM_MESSAGES; k++)
		message[k] = (char *)malloc((MAX_LENGTH_MESSAGE + 1) * sizeof(char));

	directory = opendir(IN_PATH); //opens folder defined as input path
	if (directory)				  //creates array of filenames
	{
		while ((dir = readdir(directory)) != NULL) //keep reading until no more files in directory
		{
			nameLength = strlen(dir->d_name);
			if (strncmp(dir->d_name + nameLength - 4, ".tga", 4) == 0) //only look at .tga files
			{
				inputImageCount++; //increment count of all files
			}
		}
		closedir(directory);
	}
	imageReadArray = (ImageStruct *)calloc(inputImageCount, sizeof(ImageStruct));

	inputImageCount = 0;
	directory = opendir(IN_PATH); //opens folder defined as input path
	if (directory)				  //creates array of filenames
	{
		while ((dir = readdir(directory)) != NULL) //keep reading until no more files in directory
		{
			nameLength = strlen(dir->d_name);
			if (strncmp(dir->d_name + nameLength - 4, ".tga", 4) == 0) //only look at .tga files
			{
				strcpy(inputImageList[inputImageCount], dir->d_name); //add to array of all filenames
				strcpy(charArray, "");
				strcat(charArray, IN_PATH);
				strcat(charArray, inputImageList[inputImageCount]);
				imageReadArray[inputImageCount] = readImage(charArray); //add raed file to array
				inputImageCount++;										//increment count of all files
			}
		}
		closedir(directory);
	}

	//jyh: Allocate the output image.
	imageOut.nbRows = imageReadArray[0].nbRows;
	imageOut.nbCols = imageReadArray[0].nbCols;
	imageOut.bytesPerPixel = imageReadArray[0].bytesPerPixel;
	imageOut.bytesPerRow = imageReadArray[0].bytesPerRow;
	imageOut.raster = (unsigned char *)calloc(imageOut.nbRows * imageOut.bytesPerRow, sizeof(unsigned char));

	if (desiredColumns == 0)
		numThreads = desiredRows;
	else if (desiredRows == 0)
		numThreads = desiredColumns;
	else
		numThreads = desiredColumns * desiredRows;

	scaleX = (1.f * IMAGE_PANE_WIDTH) / imageOut.nbCols;
	scaleY = (1.f * IMAGE_PANE_HEIGHT) / imageOut.nbRows;
	launchTime = time(NULL);
	clock_gettime(CLOCK_MONOTONIC_RAW, &startingTime);
}

void *imageCorrect(void *argument)
{
	int imageStackCount = 0; //iterator for image stack
	int maxImageX = imageReadArray[0].nbRows;
	int maxImageY = imageReadArray[0].nbCols;
	int windowHalfSize = windowSize / 2;

	threadInfo *info = (threadInfo *)argument;

	//printf("Thread %d working on area %d to %d and %d to %d\n", info->index, info->rowStart, info->rowEnd, info->colStart, info->colEnd);
	for (int x = 0; x < 50000; x++)
	{
		int pixelX = random() % maxImageX; //random X pixel
		int pixelY = random() % maxImageY; //random Y pixel

		//form small rectangular window centered at the pixel
		int pixelLX = ((pixelX - windowHalfSize) < 0) ? 0 : (pixelX - windowHalfSize);
		//min X value of window
		int pixelRX = ((pixelX + windowHalfSize) > maxImageX) ? maxImageX : (pixelX + windowHalfSize);
		//max X value of rectangle
		int pixelBY = ((pixelY - windowHalfSize) < 0) ? 0 : (pixelY - windowHalfSize);
		//min Y value of window
		int pixelTY = ((pixelY + windowHalfSize) > maxImageY) ? maxImageY : (pixelY + windowHalfSize);
		//max Y value of rectangle
		//jhy: maxfocus and maxFocusImage should be reinitialized for each pixel.
		float maxFocus = 0;
		int maxFocusImage = 0;

		for (imageStackCount = 0; imageStackCount < inputImageCount; imageStackCount++)
		{ //look at all input files
			unsigned char *currentImage = imageReadArray[imageStackCount].raster;
			int minGreyValue = 255;
			int maxGreyValue = 0;
			for (int windowX = pixelLX; windowX < pixelRX; windowX++)
			{
				for (int windowY = pixelBY; windowY < pixelTY; windowY++)
				{ //get greyscale value for each pixel in window
					int redValue = currentImage[windowX * imageReadArray[0].bytesPerRow + 4 * windowY + 0];
					int greenValue = currentImage[windowX * imageReadArray[0].bytesPerRow + 4 * windowY + 1];
					int blueValue = currentImage[windowX * imageReadArray[0].bytesPerRow + 4 * windowY + 2];
					int greyValue = redValue + greenValue + blueValue;
					minGreyValue = (greyValue < minGreyValue) ? greyValue : minGreyValue;
					maxGreyValue = (greyValue > maxGreyValue) ? greyValue : maxGreyValue;
				}
			}
			//marginally improved focus scoring criterion
			float focusScore = (maxGreyValue - minGreyValue + 0.f) / maxGreyValue;
			if (focusScore > maxFocus)
			{ //find which image is most focused at window
				maxFocus = focusScore;
				maxFocusImage = imageStackCount;
			}
		}
		//set the new image's pixels to be the most in focused one
		unsigned char *bestImage = imageReadArray[maxFocusImage].raster;
		for (int windowX = pixelLX; windowX < pixelRX; windowX++)
		{
			for (int windowY = pixelBY; windowY < pixelTY; windowY++)
			{
				unsigned char redValue = info->raster[windowX * imageReadArray[0].bytesPerRow + 4 * windowY + 0];
				unsigned char greenValue = info->raster[windowX * imageReadArray[0].bytesPerRow + 4 * windowY + 1];
				unsigned char blueValue = info->raster[windowX * imageReadArray[0].bytesPerRow + 4 * windowY + 2];
				int largestMutex = rectFind(windowX, windowY);

				if ((redValue + greenValue + blueValue) == 0)
				{ //if pixel value at that location in the output image is 0
					//replace the value at that location by that in the most focus window
					redValue = bestImage[windowX * imageReadArray[0].bytesPerRow + 4 * windowY + 0];
					greenValue = bestImage[windowX * imageReadArray[0].bytesPerRow + 4 * windowY + 1];
					blueValue = bestImage[windowX * imageReadArray[0].bytesPerRow + 4 * windowY + 2];
					//enter CS
					//obtain all smaller mutex locks up to current lock
					for (int x = 0; x <= largestMutex; x++)
					{
						pthread_mutex_lock(myDataLock + x);
						//printf("Obtaining lock %d\n", x);
					}
					info->raster[windowX * imageReadArray[0].bytesPerRow + 4 * windowY + 0] = redValue;
					info->raster[windowX * imageReadArray[0].bytesPerRow + 4 * windowY + 1] = greenValue;
					info->raster[windowX * imageReadArray[0].bytesPerRow + 4 * windowY + 2] = blueValue;
					//release all obtained locks
					for (int x = largestMutex; x >= 0; x--)
					{
						pthread_mutex_unlock(myDataLock + x);
						//printf("Releasing lock %d\n", x);
					}
					//leave CS
				}
				else
				{ //for each color channel r, g, b, of the pixel
					unsigned char oldRedValue = info->raster[windowX * imageReadArray[0].bytesPerRow + 4 * windowY + 0];
					unsigned char oldGreenValue = info->raster[windowX * imageReadArray[0].bytesPerRow + 4 * windowY + 1];
					unsigned char oldBlueValue = info->raster[windowX * imageReadArray[0].bytesPerRow + 4 * windowY + 2];
					redValue = bestImage[windowX * imageReadArray[0].bytesPerRow + 4 * windowY + 0];
					greenValue = bestImage[windowX * imageReadArray[0].bytesPerRow + 4 * windowY + 1];
					blueValue = bestImage[windowX * imageReadArray[0].bytesPerRow + 4 * windowY + 2];
					//new color is .5*most in focus value + .5*old value in output image
					redValue = (int)(0.5 * redValue) + (0.5 * oldRedValue);
					greenValue = (int)(0.5 * greenValue) + (0.5 * oldGreenValue);
					blueValue = (int)(0.5 * blueValue) + (0.5 * oldBlueValue);
					//enter CS
					//obtain all smaller mutex locks up to current lock
					for (int x = 0; x <= largestMutex; x++)
					{
						pthread_mutex_lock(myDataLock + x);
						//printf("Obtaining lock %d\n", x);
					}
					info->raster[windowX * imageReadArray[0].bytesPerRow + 4 * windowY + 0] = redValue;
					info->raster[windowX * imageReadArray[0].bytesPerRow + 4 * windowY + 1] = greenValue;
					info->raster[windowX * imageReadArray[0].bytesPerRow + 4 * windowY + 2] = blueValue;
					//release all obtained locks
					for (int x = largestMutex; x >= 0; x--)
					{
						pthread_mutex_unlock(myDataLock + x);
						//printf("Releasing lock %d\n", x);
					}
					//leave CS
				}
			}
		}
	}

	//printf("Thread %d done\n", info->index);
	return (NULL);
}

//returns index of window that contains passed coordinates
//returned index coorelates with mutex lock index
int rectFind(int x, int y)
{
	int maxImageX = imageReadArray[0].nbRows;
	int maxImageY = imageReadArray[0].nbCols;
	int width = maxImageX / desiredColumns;		//calculate width of thread window
	int height = maxImageY / desiredRows;		//calculate height of thread window
	int rowStart, rowEnd, colStart, colEnd = 0; //bounds of thread window
	int z = 0;									//simple iterator variable

	for (int k = 0; k < desiredRows; ++k)
	{
		for (int j = 0; j < desiredColumns; ++j)
		{ //iterate over all sub-rectangles of image
			rowStart = j * width;
			rowEnd = (j + 1) * width;
			colStart = k * height;
			colEnd = (k + 1) * height;
			if (x >= rowStart && x <= rowEnd && y >= colStart && y <= colEnd)
			{
				//printf("%d,%d belongs in lock %d\n", x, y, z);
				return z;
			}
			z++;
		}
	}
	return z;
}
