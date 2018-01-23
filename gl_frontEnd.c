//
//  gl_frontEnd.c
//  GL threads
//
//  Created by Jean-Yves Hervé on 2017-12-04.
//  Copyright © 2017 Jean-Yves Hervé. All rights reserved.
//

 /*-------------------------------------------------------------------------+
 |	A graphic front end for displaying the output image.					|
 |																			|
 +-------------------------------------------------------------------------*/

#include <string.h>
#include <stdlib.h>
#include <stdio.h>
#include <math.h>
#include <time.h>
//
#include "fileIO.h"
#include "gl_frontEnd.h"

const extern int MAX_NUM_MESSAGES;
const extern int MAX_LENGTH_MESSAGE;

extern int numLiveThreads;
extern ImageStruct imageOut;

//---------------------------------------------------------------------------
//  Private functions' prototypes
//---------------------------------------------------------------------------

void myResize(int w, int h);
void displayTextualInfo(const char* infoStr, int x, int y, int isLarge);
void myImagePaneMouse(int b, int s, int x, int y);
void myStatePaneMouse(int button, int state, int x, int y);
void myMouse(int b, int s, int x, int y);
void myTimeCB(int d);

//---------------------------------------------------------------------------
//  Interface constants
//---------------------------------------------------------------------------


#define SMALL_DISPLAY_FONT    GLUT_BITMAP_HELVETICA_10
#define MEDIUM_DISPLAY_FONT   GLUT_BITMAP_HELVETICA_12
#define LARGE_DISPLAY_FONT    GLUT_BITMAP_HELVETICA_18
const int SMALL_FONT_HEIGHT = 12;
const int LARGE_FONT_HEIGHT = 18;
const int TEXT_PADDING = 0;
const float kTextColor[4] = {1.f, 1.f, 1.f, 1.f};

const int   INIT_WIN_X = 100,
            INIT_WIN_Y = 40;
const int   msecs = 20;

//	I hate the fact that the gcc compiler on Ubuntu doesn't let
//	me cascade the definition of my constants.  This goes agaisnt
//	so many pronciples of good programming.  No wonder so many
//	aps on Linux are crap.
const int IMAGE_PANE_WIDTH = 900;
const int IMAGE_PANE_HEIGHT = 600;
const int STATE_PANE_WIDTH = 300;
const int STATE_PANE_HEIGHT = 600;
const int H_PADDING = 0;
const int WINDOW_WIDTH = 1200;
const int WINDOW_HEIGHT = 600;


//---------------------------------------------------------------------------
//  File-level global variables
//---------------------------------------------------------------------------

void (*imageDisplayFunc)(void);
void (*stateDisplayFunc)(void);
void (*keyboardFunc)(unsigned char c, int x, int y);
void (*mouseFunc)(int b, int s, int x, int y);

//	We use a window split into two panes/subwindows.  The subwindows
//	will be accessed by an index.
const int	IMAGE_PANE = 0,
			STATE_PANE = 1;
int	gMainWindow,
	gSubwindow[2];


//---------------------------------------------------------------------------
//	Drawing functions
//---------------------------------------------------------------------------



void displayTextualInfo(const char* infoStr, int xPos, int yPos, int fontSize)
{
    //-----------------------------------------------
    //  0.  get current material properties
    //-----------------------------------------------
    float oldAmb[4], oldDif[4], oldSpec[4], oldShiny;
    glGetMaterialfv(GL_FRONT, GL_AMBIENT, oldAmb);
    glGetMaterialfv(GL_FRONT, GL_DIFFUSE, oldDif);
    glGetMaterialfv(GL_FRONT, GL_SPECULAR, oldSpec);
    glGetMaterialfv(GL_FRONT, GL_SHININESS, &oldShiny);

    glPushMatrix();

    //-----------------------------------------------
    //  1.  Build the string to display <-- parameter
    //-----------------------------------------------
    int infoLn = (int) strlen(infoStr);

    //-----------------------------------------------
    //  2.  Determine the string's length (in pixels)
    //-----------------------------------------------
    int textWidth = 0;
	switch(fontSize)
	{
		case 0:
			for (int k=0; k<infoLn; k++)
			{
				textWidth += glutBitmapWidth(SMALL_DISPLAY_FONT, infoStr[k]);
			}
			break;
		
		case 1:
			for (int k=0; k<infoLn; k++)
			{
				textWidth += glutBitmapWidth(MEDIUM_DISPLAY_FONT, infoStr[k]);
			}
			break;
		
		case 2:
			for (int k=0; k<infoLn; k++)
			{
				textWidth += glutBitmapWidth(LARGE_DISPLAY_FONT, infoStr[k]);
			}
			break;
			
		default:
			break;
	}
		
	//  add a few pixels of padding
    textWidth += 2*TEXT_PADDING;
	
    //-----------------------------------------------
    //  4.  Draw the string
    //-----------------------------------------------    
    glColor4fv(kTextColor);
    int x = xPos;
	switch(fontSize)
	{
		case 0:
			for (int k=0; k<infoLn; k++)
			{
				glRasterPos2i(x, yPos);
				glutBitmapCharacter(SMALL_DISPLAY_FONT, infoStr[k]);
				x += glutBitmapWidth(SMALL_DISPLAY_FONT, infoStr[k]);
			}
			break;
		
		case 1:
			for (int k=0; k<infoLn; k++)
			{
				glRasterPos2i(x, yPos);
				glutBitmapCharacter(MEDIUM_DISPLAY_FONT, infoStr[k]);
				x += glutBitmapWidth(MEDIUM_DISPLAY_FONT, infoStr[k]);
			}
			break;
		
		case 2:
			for (int k=0; k<infoLn; k++)
			{
				glRasterPos2i(x, yPos);
				glutBitmapCharacter(LARGE_DISPLAY_FONT, infoStr[k]);
				x += glutBitmapWidth(LARGE_DISPLAY_FONT, infoStr[k]);
			}
			break;
			
		default:
			break;
	}


    //-----------------------------------------------
    //  5.  Restore old material properties
    //-----------------------------------------------
	glMaterialfv(GL_FRONT, GL_AMBIENT, oldAmb);
	glMaterialfv(GL_FRONT, GL_DIFFUSE, oldDif);
	glMaterialfv(GL_FRONT, GL_SPECULAR, oldSpec);
	glMaterialf(GL_FRONT, GL_SHININESS, oldShiny);  
    
    //-----------------------------------------------
    //  6.  Restore reference frame
    //-----------------------------------------------
    glPopMatrix();
}

void drawState(int numMessages, char** message)
{
	//	I compute once the dimensions for all the rendering of my state info
	//	One other place to rant about that desperately lame gcc compiler.  It's
	//	positively disgusting that the code below is rejected.
	int LEFT_MARGIN = STATE_PANE_WIDTH / 12;
	int V_PAD = STATE_PANE_HEIGHT / 12;

	for (int k=0; k<numMessages; k++)
	{
		displayTextualInfo(message[k], LEFT_MARGIN, 3*STATE_PANE_HEIGHT/4 - k*V_PAD, 1);
	}

	//	display info about number of live threads
	char infoStr[256];
	sprintf(infoStr, "Live Threads: %d", numLiveThreads);
	displayTextualInfo(infoStr, LEFT_MARGIN, 7*STATE_PANE_HEIGHT/8, 2);
}


//	This callback function is called when the window is resized
//	(generally by the user of the application).
//	It is also called when the window is created, why I placed there the
//	code to set up the virtual camera for this application.
//
void myResize(int w, int h)
{
	if ((w != WINDOW_WIDTH) || (h != WINDOW_HEIGHT))
	{
		glutReshapeWindow(WINDOW_WIDTH, WINDOW_HEIGHT);
	}
	else
	{
		glutPostRedisplay();
	}
}


void myDisplay(void)
{
    glutSetWindow(gMainWindow);

    glMatrixMode(GL_MODELVIEW);
    glClear(GL_COLOR_BUFFER_BIT);
    glutSwapBuffers();

	imageDisplayFunc();
	stateDisplayFunc();
	
    glutSetWindow(gMainWindow);	
}


//	This function is called when a mouse event occurs in the state pane
void myStatePaneMouse(int button, int state, int x, int y)
{
	switch (button)
	{
		case GLUT_LEFT_BUTTON:
			if (state == GLUT_DOWN)
			{
				//	do something
			}
			else if (state == GLUT_UP)
			{
				//	exit(0);
			}
			break;
			
		default:
			break;
	}

	glutSetWindow(gMainWindow);
	glutPostRedisplay();
}


//	This function is called when a mouse event occurs just in the tiny
//	space between the two subwindows.
//
void myMouse(int button, int state, int x, int y)
{
	glutSetWindow(gMainWindow);
	glutPostRedisplay();
}

//	This function is called when a mouse event occurs in the grid pane
//
void myImagePaneMouse(int button, int state, int x, int y)
{
	switch (button)
	{
		case GLUT_LEFT_BUTTON:
			if (state == GLUT_DOWN)
			{
				//	do something
			}
			else if (state == GLUT_UP)
			{
				//	exit(0);
			}
			break;
			
		default:
			break;
	}

	glutSetWindow(gMainWindow);
	glutPostRedisplay();
}


void myTimeCB(int d)
{
	//	Nothing to do but display the current output image
    myDisplay();

	glutTimerFunc(msecs, myTimeCB, 0);
}


void initializeFrontEnd(int argc, char** argv, void (*imageDisplayCB)(void),
						void (*stateDisplayCB)(void),
						void (*keyboardFunc)(unsigned char c, int x, int y))
{
	//	Initialize glut and create a new window
	glutInit(&argc, argv);
	glutInitDisplayMode(GLUT_DOUBLE | GLUT_RGBA);


	glutInitWindowSize(WINDOW_WIDTH, WINDOW_HEIGHT);
	glutInitWindowPosition(INIT_WIN_X, INIT_WIN_Y);
	gMainWindow = glutCreateWindow("Colorful Trails -- CSC 412 - Spring 2017");
	glClearColor(0.2f, 0.2f, 0.2f, 1.f);
	
	//	set up the callbacks for the main window
	glutDisplayFunc(myDisplay);
	glutReshapeFunc(myResize);
	glutMouseFunc(myMouse);
	glutTimerFunc(msecs, myTimeCB, 0);

	imageDisplayFunc = imageDisplayCB;
	stateDisplayFunc = stateDisplayCB;
	
	//	create the two panes as glut subwindows
	gSubwindow[IMAGE_PANE] = glutCreateSubWindow(gMainWindow,
												0, 0,							//	origin
												IMAGE_PANE_WIDTH, IMAGE_PANE_HEIGHT);
    glViewport(0, 0, IMAGE_PANE_WIDTH, IMAGE_PANE_HEIGHT);
    glMatrixMode(GL_PROJECTION);
    glLoadIdentity();
	glOrtho(0.0f, IMAGE_PANE_WIDTH, 0.0f, IMAGE_PANE_HEIGHT, -1, 1);
	glClearColor(1.f, 0.f, 0.f, 1.f);
	glutKeyboardFunc(keyboardFunc);
	glutMouseFunc(myImagePaneMouse);
	glutDisplayFunc(imageDisplayCB);
	
	
	glutSetWindow(gMainWindow);
	gSubwindow[STATE_PANE] = glutCreateSubWindow(gMainWindow,
												IMAGE_PANE_WIDTH + H_PADDING, 0,	//	origin
												STATE_PANE_WIDTH, STATE_PANE_HEIGHT);
    glViewport(0, 0, STATE_PANE_WIDTH, STATE_PANE_WIDTH);
    glMatrixMode(GL_PROJECTION);
    glLoadIdentity();
	glOrtho(0.0f, STATE_PANE_WIDTH, 0.0f, STATE_PANE_HEIGHT, -1, 1);
	glClearColor(0.f, 0.f, 0.f, 1.f);
	glutKeyboardFunc(keyboardFunc);
	glutMouseFunc(myImagePaneMouse);
	glutDisplayFunc(stateDisplayCB);
}


