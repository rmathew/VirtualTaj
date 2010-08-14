/* Copyright (c) 2001 Ranjit Mathew. All rights reserved.
 * Use of this source code is governed by the terms of the BSD licence
 * that can be found in the LICENCE file.
 */

/*
 * VTAJ.C: Displays the Virtual Taj Mahal to the user.
 * Can use both GLData as well as BSP Tree format models, the
 * former is the default.
 *
 * Command line options:
 *   -6: 640x480 resolution at desktop colour depth
 *   -8: 800x600 resolution at desktop colour depth (default)
 *  -10: 1024x768 resolution at desktop colour depth
 *   -f: fullscreen mode (default)
 *   -w: windowed mode
 * -gld: use GLData models (default)
 * -bsp: use BSP Tree models
 * 
 * Requires SDL 1.2.3 or better, SDL_image 1.0 or better
 * and OpenGL 1.1 or better, on an i386-compatible platform.
 * A decent 3D accelerator with good OpenGL drivers is 
 * highly recommended.
 *
 * Version: 1.0
 * Author: Ranjit Mathew (rmathew@gmail.com)
 *
 * Credits for the original demo:
 *
 *     Published by Intel India (http://www.intel.com/in/)
 *     DirectX programming by D Sanjit (dsanjit@yahoo.com)
 *     Taj Mahal models by VRR Technologies (http://www.vrrt.com/)
 * 
 * Started: 2001/09/08
 * Finished: 2001/12/16 (Version 1.0)
 */


/* Include headers */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <math.h>
#include <limits.h>
#include <float.h>

#ifndef M_PI
    # define M_PI		3.14159265358979323846F
#endif

/* We do not want SDL to automatically include "glext.h" */
#define NO_SDL_GLEXT

#include "SDL.h"
#include "SDL_opengl.h"
#include "SDL_image.h"

#include "gld.h"
#include "bsp.h"
#include "coldet.h"


/* Handy macro for checking OpenGL errors */
#ifdef VTAJ_DEBUG
    #define CHECK_GL_ERROR \
        { \
	    GLenum glErr; \
	    if( ( glErr = glGetError( )) != GL_NO_ERROR) \
	    { \
	        fprintf( stderr, \
		    "\nOpenGL ERROR around line %d: %s\n", \
		    __LINE__, \
		    gluErrorString( glErr)); \
	    } \
	}
#else
    #define CHECK_GL_ERROR
#endif


/* Literal constants */

#define FIELD_OF_VIEW 30.0F
#define NEAR_Z_CLIP 1.0F
#define FAR_Z_CLIP 6000.F

#define VIEWER_STRIDE +5.0F
#define VIEWER_UPDOWN_DELTA +5.0F
#define VIEWER_TURN_ANGLE ( ( 3.0F * M_PI) / 180.0F)

#define TAJ_INT_MIN_X -50.0F
#define TAJ_INT_MAX_X +50.0F

#define TAJ_INT_MIN_Z -290.0F
#define TAJ_INT_MAX_Z -160.0F

#define IMGS_FOLDER_PFX "textures/"
#define PROG_BAR_IMG "initwindow.jpg"

#define TAJ_EXT_GLD_MODEL "models/externals.gld"
#define TAJ_INT_GLD_MODEL "models/internals.gld"

#define TAJ_EXT_BSP_MODEL "models/externals.bsp"
#define TAJ_INT_BSP_MODEL "models/internals.bsp"

#define TAJ_EXT_COLDET_MODEL "models/cx_ext.gld"
#define TAJ_INT_COLDET_MODEL "models/cx_int.gld"


/* Global data */

static GLboolean useBSP = GL_FALSE;


/* Frames-per-second data */
static Uint32 currFPS;

/* The screen dimensions */
static int scrWidth = 800;
static int scrHeight = 600;
static GLboolean fullscreen = GL_TRUE;

/* Models to be shown */
static GLData *extGldModel = NULL;
static GLData *intGldModel = NULL;

static BSPTreeData *extBspModel = NULL;
static BSPTreeData *intBspModel = NULL;

static GLData *extColDetModel = NULL;
static GLData *intColDetModel = NULL;

/* Viewer information */
static GLfloat angleOfView;
static GLfloat vPos[3];
static GLdouble vNorm[3];
static GLdouble minVisCos = 0.0;

/* Texture data */
static GLuint progBarTexture;

static Uint16 numExtMaps;
static Uint16 numIntMaps;

static GLuint *extTextures;
static GLuint *intTextures;

static GLfloat *extTexPriorities;
static GLfloat *intTexPriorities;

/* Queued vertex and texture coordinate indices during each redraw */
static Uint32 *extNumVerts;
static GLushort **extVertIndices;

static Uint32 *intNumVerts;
static GLushort **intVertIndices;

/* Inside/outside flag */
static GLboolean insideTaj;

/* Switched pointers depending on viewer's position */
static GLData *currGldModel = NULL;
static BSPTreeData *currBspModel = NULL;
static GLData *currColDetModel = NULL;
static GLuint *currTextures;
static Uint32 *currNumVerts;
static GLushort **currVertIndices;


/* Local function prototypes */

static void ParseCmdLine( int argc, char *argv[]);
static void LoadModels( void);
static void InitGraphics( void);
static void InitQueues( void);
static void HandleEvents( void);
static void RenderFrame( void);
static void ShowProgressBar( unsigned int percentComplete);
static int LoadJPGTexture( const char *fileName, GLuint texObjId);
static void InitTextures( void);
static void DrawBSPTree( BSPTree *aTree);
static void FreeResources( void);

/**
 * The entry point to the program
 */
int main( int argc, char *argv[])
{
    /* Parse command-line arguments */
    ParseCmdLine( argc, argv);

    /* Load all the models */
    LoadModels( );

    /* Initialise viewing queues (vertex arrays, indices, etc.) */
    InitQueues( );

    /* Position the viewer - outside, and facing the Taj */
    vPos[0] = vPos[1] = 0.0F;
    vPos[2] = +330.0F;
    angleOfView = (270.0F * M_PI) / 180.0F;
    vNorm[0] = cos( angleOfView);
    vNorm[1] = 0.0;
    vNorm[2] = sin( angleOfView);

    
    /* Initialise SDL/OpenGL, load textures, etc. */
    InitGraphics( );


    /* Initialise model-related data */
    insideTaj = GL_FALSE;
    if( useBSP == GL_TRUE)
    {
        currBspModel = extBspModel;

    } /* End if */
    else
    {
        currGldModel = extGldModel;

    } /* End else */

    currTextures = extTextures;
    currColDetModel = extColDetModel;
    currNumVerts = extNumVerts;
    currVertIndices = extVertIndices;

    glVertexPointer( 
        3, GL_FLOAT, 0, 
	( ( useBSP == GL_TRUE) ? 
	    extBspModel->vertCoords : 
	    extGldModel->vertCoords
        )
    );
    CHECK_GL_ERROR;
    glTexCoordPointer( 
        2, GL_FLOAT, 0,
	( ( useBSP == GL_TRUE) ? 
	    extBspModel->texCoords : 
	    extGldModel->texCoords
        )
    );
    CHECK_GL_ERROR;

    glPrioritizeTextures( numExtMaps, extTextures, extTexPriorities);
    CHECK_GL_ERROR;


    /* Now show the models to the user and respond to his inputs */
    HandleEvents( );

    /* Done showing the Taj. Clean up resource usage */
    FreeResources( );

    return EXIT_SUCCESS;

} /* End function main */


/**
 * Parse the command line and set appropriate flags and variables.
 * This routine considers repeated specifications for the same class
 * (resolution, mode, etc.) as an error.
 */
void ParseCmdLine( int argc, char *argv[])
{
    GLboolean parseError = GL_FALSE;
    GLboolean resSelected = GL_FALSE;
    GLboolean scrModeSelected = GL_FALSE;
    GLboolean mdlFmtSelected = GL_FALSE;

    /* If there are command line arguments, parse them */
    if( argc > 1)
    {
        int i;

	for( i = 1; i < argc; i++)
	{
	    if( ( strcmp( "-6", argv[i]) == 0) && 
	        ( resSelected == GL_FALSE)
	    )
	    {
		resSelected = GL_TRUE;
		scrWidth = 640;
		scrHeight = 480;

	    } /* End if */
	    else if( ( strcmp( "-8", argv[i]) == 0) && 
	        ( resSelected == GL_FALSE)
	    )
	    {
		resSelected = GL_TRUE;
		scrWidth = 800;
		scrHeight = 600;

	    } /* End else-if */
	    else if( ( strcmp( "-10", argv[i]) == 0) && 
	        ( resSelected == GL_FALSE)
	    )
	    {
		resSelected = GL_TRUE;
		scrWidth = 1024;
		scrHeight = 768;

	    } /* End else-if */
	    else if( ( strcmp( "-w", argv[i]) == 0) && 
	        ( scrModeSelected == GL_FALSE)
	    )
	    {
		scrModeSelected = GL_TRUE;
		fullscreen = GL_FALSE;

	    } /* End else-if */
	    else if( ( strcmp( "-f", argv[i]) == 0) && 
	        ( scrModeSelected == GL_FALSE)
	    )
	    {
		scrModeSelected = GL_TRUE;
		fullscreen = GL_TRUE;

	    } /* End else-if */
	    else if( ( strcmp( "-gld", argv[i]) == 0) && 
	        ( mdlFmtSelected == GL_FALSE)
	    )
	    {
		mdlFmtSelected = GL_TRUE;
		useBSP = GL_FALSE;

	    } /* End else-if */
	    else if( ( strcmp( "-bsp", argv[i]) == 0) && 
	        ( mdlFmtSelected == GL_FALSE)
	    )
	    {
		mdlFmtSelected = GL_TRUE;
		useBSP = GL_TRUE;

	    } /* End else-if */
	    else
	    {
	        parseError = GL_TRUE;
		break;

	    } /* End else */

	} /* End for */

    } /* End if */


    if( parseError == GL_TRUE)
    {
        fprintf( stderr, "\nERROR: Invalid command line arguments!\n");
	fprintf( 
	    stderr, 
	    "\nUsage: %s {-6 or -8 or -10} {-w or -f} {-gld or -bsp}\n",
	    argv[0]
	);
	fprintf(
	    stderr,
	    "\t  -6: 640x480 resolution at desktop colour depth\n"
	);
	fprintf(
	    stderr,
	    "\t  -8: 800x600 resolution at desktop colour depth (default)\n"
	);
	fprintf(
	    stderr,
	    "\t -10: 1024x768 resolution at desktop colour depth\n"
	);
	fprintf(
	    stderr,
	    "\t  -w: windowed mode\n"
	);
	fprintf(
	    stderr,
	    "\t  -f: fullscreen mode (default)\n"
	);
	fprintf(
	    stderr,
	    "\t-gld: use GLData models (default)\n"
	);
	fprintf(
	    stderr,
	    "\t-bsp: use BSP Tree models\n"
	);

        exit( EXIT_FAILURE);

    } /* End if */

} /* End function ParseCmdLine */


/**
 * Load the GLData or BSP Tree models of the Taj Exterior and
 * Interior as needed, as well as the models used for collision 
 * detection.
 */
void LoadModels( void)
{
    FILE *extMdlFile, *intMdlFile;
    FILE *extColDetFile, *intColDetFile;

    /* Read in the Taj interior and exterior models */
    if( useBSP == GL_TRUE)
    {
        /* Use the BSP Tree version of the models */

	if( ( extMdlFile = fopen( TAJ_EXT_BSP_MODEL, "rb")) == NULL)
	{
	    fprintf( 
	        stderr,
	        "\nERROR: Could not read VirtualTaj Externals "
		"BSP model \"%s\"\n", TAJ_EXT_BSP_MODEL
	    );
	    perror( "Details");
	    exit( EXIT_FAILURE);

	} /* End if */
	else
	{
	    extBspModel = LoadBSPTreeData( extMdlFile);
	    fclose( extMdlFile);

	} /* End else */

	if( ( intMdlFile = fopen( TAJ_INT_BSP_MODEL, "rb")) == NULL)
	{
	    fprintf( 
	        stderr,
	        "\nERROR: Could not read VirtualTaj Internals "
		"BSP model \"%s\"\n", TAJ_INT_BSP_MODEL
	    );
	    perror( "Details");
	    exit( EXIT_FAILURE);

	} /* End if */
	else
	{
	    intBspModel = LoadBSPTreeData( intMdlFile);
	    fclose( intMdlFile);

	} /* End else */
        
    } /* End if */
    else
    {
        /* Use the simple GLData format of the models */

	if( ( extMdlFile = fopen( TAJ_EXT_GLD_MODEL, "rb")) == NULL)
	{
	    fprintf( 
	        stderr,
	        "\nERROR: Could not read VirtualTaj Externals "
		"GLD model \"%s\"\n", TAJ_EXT_GLD_MODEL
	    );
	    perror( "Details");
	    exit( EXIT_FAILURE);

	} /* End if */
	else
	{
	    extGldModel = LoadGLData( extMdlFile);
	    fclose( extMdlFile);

	} /* End else */

	if( ( intMdlFile = fopen( TAJ_INT_GLD_MODEL, "rb")) == NULL)
	{
	    fprintf( 
	        stderr,
	        "\nERROR: Could not read VirtualTaj Internals "
		"GLD model \"%s\"\n", TAJ_INT_GLD_MODEL
	    );
	    perror( "Details");
	    exit( EXIT_FAILURE);

	} /* End if */
	else
	{
	    intGldModel = LoadGLData( intMdlFile);
	    fclose( intMdlFile);

	} /* End else */

    } /* End else */

    
    /* Find out the number of texture maps used in each model */
    numExtMaps = ( useBSP == GL_TRUE) ?
        extBspModel->nMaps : extGldModel->nMaps;

    numIntMaps = ( useBSP == GL_TRUE) ?
        intBspModel->nMaps : intGldModel->nMaps;


    /* Read in the low polygon count models used for
     * collision detection.
     */
    
    extColDetFile = fopen( TAJ_EXT_COLDET_MODEL, "rb");
    if( extColDetFile == NULL)
    {
        fprintf( 
	    stderr, 
	    "Unable to read the Taj exterior collision-detection model (%s)\n",
	    TAJ_EXT_COLDET_MODEL
	);
	exit( EXIT_FAILURE);

    } /* End if */

    if( ( extColDetModel = LoadGLData( extColDetFile)) == NULL)
    {
        fprintf( 
	    stderr, 
	    "Unable to read the Taj exterior collision-detection model (%s)\n",
	    TAJ_EXT_COLDET_MODEL
	);
	exit( EXIT_FAILURE);

    } /* End if */

    fclose( extColDetFile);


    intColDetFile = fopen( TAJ_INT_COLDET_MODEL, "rb");
    if( intColDetFile == NULL)
    {
        fprintf( 
	    stderr, 
	    "Unable to read the Taj interior collision-detection model (%s)\n",
	    TAJ_INT_COLDET_MODEL
	);
	exit( EXIT_FAILURE);

    } /* End if */

    if( ( intColDetModel = LoadGLData( intColDetFile)) == NULL)
    {
        fprintf( 
	    stderr, 
	    "Unable to read the Taj interior collision-detection model (%s)\n",
	    TAJ_INT_COLDET_MODEL
	);
	exit( EXIT_FAILURE);

    } /* End if */

    fclose( intColDetFile);

} /* End function LoadModels */


/**
 * Initialises SDL/OpenGL according to the needs of the program and
 * the user.
 */
void InitGraphics( void)
{
    Uint32 sdlVidFlags;

    /* Initialize SDL for video output */
    if( SDL_Init( SDL_INIT_VIDEO) < 0) 
    {
	fprintf(stderr, "Unable to initialise SDL: %s\n", SDL_GetError());
	exit( EXIT_FAILURE);

    } /* End if */

    atexit( SDL_Quit);

    /* Use double buffering */
    SDL_GL_SetAttribute( SDL_GL_DOUBLEBUFFER, 1);

    /* Create an OpenGL screen */
    sdlVidFlags = 0U;
    sdlVidFlags |= SDL_OPENGL;
    if( fullscreen == GL_TRUE)
    {
	sdlVidFlags |= SDL_FULLSCREEN;

    } /* End if */

    if( SDL_SetVideoMode( scrWidth, scrHeight, 0, sdlVidFlags) == NULL ) 
    {
        fprintf( 
	    stderr, 
	    "Unable to create OpenGL screen: %s\n", 
            SDL_GetError( )
        );
        exit( EXIT_FAILURE);

    } /* End if */

    /* Set the title bar in environments that support it */
    SDL_WM_SetCaption( 
        "Virtual Taj Mahal Demo (by Ranjit Mathew)", NULL
    );

    /* Hide the mouse cursor, if any */
    SDL_ShowCursor( SDL_DISABLE);

    /* OpenGL initialisation */
    glViewport( 0, 0, (GLsizei )scrWidth, (GLsizei )scrHeight); 
    CHECK_GL_ERROR;

    glClearColor( 0.0F, 0.4F, 0.6F, 0.0F); 
    CHECK_GL_ERROR;

    glEnable( GL_DEPTH_TEST);
    CHECK_GL_ERROR;

    glClearDepth( +1.0F);
    CHECK_GL_ERROR;

    glDepthFunc( GL_LEQUAL);
    CHECK_GL_ERROR;

    if( useBSP == GL_TRUE)
    {
	glDisable( GL_CULL_FACE);
	CHECK_GL_ERROR;

    } /* End if */
    else 
    {
	glFrontFace( GL_CCW);
	CHECK_GL_ERROR;

	glCullFace( GL_BACK);
	CHECK_GL_ERROR;

	glEnable( GL_CULL_FACE);
	CHECK_GL_ERROR;

    } /* End else */

    glEnable( GL_TEXTURE_2D);
    CHECK_GL_ERROR;

    glTexEnvf( GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, GL_REPLACE);
    CHECK_GL_ERROR;

    glDisable( GL_ALPHA_TEST);
    CHECK_GL_ERROR;

    glAlphaFunc( GL_GREATER, 0.5F);
    CHECK_GL_ERROR;

    glShadeModel( GL_FLAT);
    CHECK_GL_ERROR;


    /* Initialise stuff */

    glMatrixMode( GL_PROJECTION);
    glLoadIdentity( );
    gluOrtho2D( 0.0, (GLdouble )scrWidth, 0.0, (GLdouble )scrHeight);
    glMatrixMode( GL_MODELVIEW);
    glLoadIdentity( );

    InitTextures( );

    /* Ready for prime time */

    glEnableClientState( GL_VERTEX_ARRAY);
    CHECK_GL_ERROR;

    glEnableClientState( GL_TEXTURE_COORD_ARRAY);
    CHECK_GL_ERROR;


    glMatrixMode( GL_PROJECTION);
    glLoadIdentity( );

    gluPerspective( 
        FIELD_OF_VIEW, (GLfloat )scrWidth/(GLfloat )scrHeight,
        NEAR_Z_CLIP, FAR_Z_CLIP
    );
    CHECK_GL_ERROR;

    glColor4f( 1.0F, 1.0F, 1.0F, 0.0F);

    glHint( GL_PERSPECTIVE_CORRECTION_HINT, GL_FASTEST);
    CHECK_GL_ERROR;

    glMatrixMode( GL_MODELVIEW);
    glLoadIdentity( );
    CHECK_GL_ERROR;

    gluLookAt( 
	vPos[0], vPos[1], vPos[2], 
	vPos[0] + ( 1.0F * cos( angleOfView)), 
	vPos[1], 
	vPos[2] + ( 1.0F * sin( angleOfView)), 
	0.0F, 1.0F, 0.0F
    );
    CHECK_GL_ERROR;

} /* End function InitGraphics */


/**
 * Initialises various queues - vertex arrays, etc.
 */
void InitQueues( void)
{
    Uint32 i, j;

    
    /* Create the drawing queues */
    extNumVerts = (Uint32 *)( malloc( numExtMaps * sizeof( Uint32)));
    intNumVerts = (Uint32 *)( malloc( numIntMaps * sizeof( Uint32)));
    extVertIndices = 
        (GLushort **)( malloc( numExtMaps * sizeof( GLushort *)));
    intVertIndices = 
        (GLushort **)( malloc( numIntMaps * sizeof( GLushort *)));
    
    if( ( extNumVerts == NULL) || ( intNumVerts == NULL) ||
        ( extVertIndices == NULL) || ( intVertIndices == NULL)
    )
    {
        fprintf( stderr, "\nFATAL ERROR: Out of Memory!\n");
	exit( EXIT_FAILURE);

    } /* End if */


    for( i = 0U; i < numExtMaps; i++)
    {
        Uint32 nTri;

        nTri = ( useBSP == GL_TRUE) ? 
	    extBspModel->mapTriNums[i] :
	    extGldModel->mapTriNums[i];

        extVertIndices[i] = (GLushort *)( 
	    malloc( 3 * nTri * sizeof( GLushort))
	);

	if( extVertIndices[i] == NULL)
	{
	    fprintf( stderr, "\nFATAL ERROR: Out of Memory!\n");
	    exit( EXIT_FAILURE);

	} /* End if */

    } /* End for */

    for( i = 0U; i < numIntMaps; i++)
    {
        Uint32 nTri;

        nTri = ( useBSP == GL_TRUE) ? 
	    intBspModel->mapTriNums[i] :
	    intGldModel->mapTriNums[i];

        intVertIndices[i] = (GLushort *)( 
	    malloc( 3 * nTri * sizeof( GLushort))
	);

	if( intVertIndices[i] == NULL)
	{
	    fprintf( stderr, "\nFATAL ERROR: Out of Memory!\n");
	    exit( EXIT_FAILURE);

	} /* End if */

    } /* End for */


    /* Calculate 'minVisCos' for the BSP Tree renderer.
     *
     * Taking the angle from the viewpoint to the corners of
     * the view frustum gives the angle of the 'view cone'.
     * This is a much simpler, though less accurate, way of
     * culling entire subtrees based on the relative position
     * and orientation of the viewer and the partition plane
     * of a BSP Tree, than frustum plane based culling.
     * 
     * The value being calculated here is the limit on the
     * cosine of the angle between the view direction and
     * the partition plane normal of a BSP Tree, for the 
     * back subtree of the BSP Tree to be completely culled.
     */
    if( useBSP == GL_TRUE)
    {
        GLdouble tanSqrTheta;
	GLdouble onePlusAspRatioSqr;
	GLdouble tmpTerm;

	tanSqrTheta = tan( ( FIELD_OF_VIEW / 2.0) * M_PI / 180.0);
	tanSqrTheta *= tanSqrTheta;

	onePlusAspRatioSqr = (GLdouble )scrWidth / (GLdouble )scrHeight;
	onePlusAspRatioSqr *= onePlusAspRatioSqr;
	onePlusAspRatioSqr += 1.0;

	tmpTerm = tanSqrTheta * onePlusAspRatioSqr;

	minVisCos = sqrt( tmpTerm / ( tmpTerm + 1.0));

    } /* End if */


    if( useBSP == GL_FALSE)
    {
        for( i = 0U; i < extGldModel->nMaps; i++)
	{
	    extNumVerts[i] = ( 3U * extGldModel->mapTriNums[i]);

	    for( j = 0U; j < extGldModel->mapTriNums[i]; j++)
	    {
		extVertIndices[i][3*j + 0] = extGldModel->triFaces[i][3*j + 0];
		extVertIndices[i][3*j + 1] = extGldModel->triFaces[i][3*j + 1];
		extVertIndices[i][3*j + 2] = extGldModel->triFaces[i][3*j + 2];

	    } /* End for */

	} /* End for */

        for( i = 0U; i < intGldModel->nMaps; i++)
	{
	    intNumVerts[i] = ( 3U * intGldModel->mapTriNums[i]);

	    for( j = 0U; j < intGldModel->mapTriNums[i]; j++)
	    {
		intVertIndices[i][3*j + 0] = intGldModel->triFaces[i][3*j + 0];
		intVertIndices[i][3*j + 1] = intGldModel->triFaces[i][3*j + 1];
		intVertIndices[i][3*j + 2] = intGldModel->triFaces[i][3*j + 2];

	    } /* End for */

	} /* End for */

    } /* End if */

} /* End function InitQueues */


/**
 * Handle user input and render updated frames.
 */
void HandleEvents( void)
{
    SDL_Event event;
    GLboolean done = GL_FALSE;


    /* Loop, drawing and checking events */
    SDL_EnableKeyRepeat( 
        SDL_DEFAULT_REPEAT_DELAY, 
	SDL_DEFAULT_REPEAT_INTERVAL
    );
    
    while( done == GL_FALSE)
    {
	RenderFrame( );

        while( SDL_PollEvent( &event) != 0) 
        {
	    GLfloat destPt[3], srcPt[3];
	    GLfloat movableDist = 0.0F;
	    GLboolean triedToMove = GL_FALSE;
	    GLboolean changedPosn = GL_FALSE;
	    GLboolean turnedAround = GL_FALSE;

	    destPt[0] = srcPt[0] = vPos[0];
	    destPt[1] = srcPt[1] = vPos[1];
	    destPt[2] = srcPt[2] = vPos[2];


            if( event.type == SDL_QUIT)
            {
                done = GL_TRUE;

            } /* End if */
	    else if( event.type == SDL_KEYDOWN)
	    {
	        switch( event.key.keysym.sym)
		{
		case SDLK_ESCAPE:
		    done = GL_TRUE;
		    break;

                case SDLK_UP:
		    destPt[0] += ( VIEWER_STRIDE * cos( angleOfView));
		    destPt[2] += ( VIEWER_STRIDE * sin( angleOfView));
		    triedToMove = GL_TRUE;
		    break;

                case SDLK_DOWN:
		    destPt[0] -= ( VIEWER_STRIDE * cos( angleOfView));
		    destPt[2] -= ( VIEWER_STRIDE * sin( angleOfView));
		    triedToMove = GL_TRUE;
		    break;

                case SDLK_RIGHT:
		    angleOfView += VIEWER_TURN_ANGLE;
		    if( angleOfView > ( 2.0F * M_PI))
		    {
		        angleOfView -= ( 2.0F * M_PI);

		    } /* End if */
		    turnedAround = GL_TRUE;
		    break;

                case SDLK_LEFT:
		    angleOfView -= VIEWER_TURN_ANGLE;
		    if( angleOfView < 0.0F)
		    {
		        angleOfView += ( 2.0F * M_PI);

		    } /* End if */
		    turnedAround = GL_TRUE;
		    break;

                case SDLK_PAGEUP:
		    destPt[1] += VIEWER_UPDOWN_DELTA;
		    triedToMove = GL_TRUE;
		    break;

                case SDLK_PAGEDOWN:
		    destPt[1] -= VIEWER_UPDOWN_DELTA;
		    triedToMove = GL_TRUE;
		    break;

                case SDLK_F1:
		    printf( "\n");
		    printf( "Current Engine Stats: \n");
		    printf( 
		        "\tEyePos: (%f, %f, %f)\n",
		        vPos[0], vPos[1], vPos[2]
		    );
		    printf( 
		        "\tLookAt: %.2f Degrees\n", 
		        ( angleOfView * 180.0 / M_PI)
		    );
		    printf( "\tFPS: %u\n", currFPS);
		    break;

                default:
		    break;

		} /* End switch */

	    } /* End else-if */


            if( triedToMove == GL_TRUE)
	    {
		if( hasCollision( 
			currColDetModel, srcPt, destPt, &movableDist
		    ) == GL_TRUE
		)
		{
		    /* Nothing to do */

		} /* End if */
		else
		{
		    /* No collision */
		    if( ( insideTaj == GL_TRUE) && 
		        ( ( destPt[0] < TAJ_INT_MIN_X) ||
			  ( destPt[0] > TAJ_INT_MAX_X) ||
			  ( destPt[2] < TAJ_INT_MIN_Z)
			)
		    )
		    {
		        /* Quirks of the model */

		    } /* End if */
		    else 
		    {
			vPos[0] = destPt[0];
			vPos[1] = destPt[1];
			vPos[2] = destPt[2];

			changedPosn = GL_TRUE;

		    } /* End else */

		} /* End else */

		/* Is he now inside the Taj? */
		if( ( vPos[0] > TAJ_INT_MIN_X) && 
		    ( vPos[0] < TAJ_INT_MAX_X) &&
		    ( vPos[2] > TAJ_INT_MIN_Z) &&
		    ( vPos[2] < TAJ_INT_MAX_Z)
		)
		{
		    /* We are now inside the Taj */
		    if( insideTaj == GL_FALSE)
		    {
		        /* We have just moved in */
			insideTaj = GL_TRUE;

			/* Adjust the viewer's position */
			/* (This is due to the quirks in the models) */

			vPos[1] = -15.0F;
			vPos[2] = TAJ_INT_MAX_Z - 20.0F;

                        /* Make other adjustments */
			if( useBSP == GL_TRUE)
			{
			    currBspModel = intBspModel;

			} /* End if */
			else
			{
			    currGldModel = intGldModel;

			} /* End else */

			currTextures = intTextures;
			currColDetModel = intColDetModel;
			currNumVerts = intNumVerts;
			currVertIndices = intVertIndices;

			glVertexPointer( 
			    3, GL_FLOAT, 0, 
			    ( ( useBSP == GL_TRUE) ? 
				intBspModel->vertCoords : 
				intGldModel->vertCoords
			    )
			);
			CHECK_GL_ERROR;
			glTexCoordPointer( 
			    2, GL_FLOAT, 0, 
			    ( ( useBSP == GL_TRUE) ? 
				intBspModel->texCoords : 
				intGldModel->texCoords
			    )
			);
			CHECK_GL_ERROR;

			glPrioritizeTextures( 
			    numIntMaps, intTextures, intTexPriorities
			);
			CHECK_GL_ERROR;

			glEnable( GL_ALPHA_TEST);
			CHECK_GL_ERROR;

		    } /* End if */

		} /* End if */
		else
		{
		    /* We are outside the Taj */
		    if( insideTaj == GL_TRUE)
		    {
		        /* We were inside the Taj earlier */
			insideTaj = GL_FALSE;

			if( useBSP == GL_TRUE)
			{
			    currBspModel = extBspModel;

			} /* End if */
			else
			{
			    currGldModel = extGldModel;

			} /* End else */

			currTextures = extTextures;
			currColDetModel = extColDetModel;
			currNumVerts = extNumVerts;
			currVertIndices = extVertIndices;

			glVertexPointer( 
			    3, GL_FLOAT, 0, 
			    ( ( useBSP == GL_TRUE) ? 
				extBspModel->vertCoords : 
				extGldModel->vertCoords
			    )
			);
			CHECK_GL_ERROR;
			glTexCoordPointer( 
			    2, GL_FLOAT, 0, 
			    ( ( useBSP == GL_TRUE) ? 
				extBspModel->texCoords : 
				extGldModel->texCoords
			    )
			);
			CHECK_GL_ERROR;

			glPrioritizeTextures( 
			    numExtMaps, extTextures, extTexPriorities
			);
			CHECK_GL_ERROR;

			glDisable( GL_ALPHA_TEST);
			CHECK_GL_ERROR;

                    } /* End if */

		} /* End else */

	    } /* End if */
	    else if( turnedAround == GL_TRUE)
	    {
	        /* Update view normal */
		vNorm[0] = cos( angleOfView);
		vNorm[2] = sin( angleOfView);
		
	    } /* End else-if */


	    /* Update the ModelView matrix if necessary */
	    if( ( changedPosn == GL_TRUE) || ( turnedAround == GL_TRUE))
	    {
		glMatrixMode( GL_MODELVIEW);
		glLoadIdentity( );

		gluLookAt( 
		    vPos[0], vPos[1], vPos[2], 
		    vPos[0] + ( 1.0F * cos( angleOfView)), 
		    vPos[1], 
		    vPos[2] + ( 1.0F * sin( angleOfView)), 
		    0.0F, 1.0F, 0.0F
		);
		CHECK_GL_ERROR;

	    } /* End if */

        } /* End while */

    } /* End while */

} /* End function HandleEvents */


/**
 * Render a frame according to the viewer position and orientation.
 */
void RenderFrame( void)
{
    register Uint32 i;
    Uint16 currNMaps;
    Uint32 startTime, endTime;

    startTime = SDL_GetTicks( );

    glClear( GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);


    if( useBSP == GL_TRUE)
    {
        currNMaps = currBspModel->nMaps;

	/* Clear display queue */
	memset( currNumVerts, 0, ( currNMaps * sizeof( Uint32)));

	/* Figure out which triangles to draw */
	DrawBSPTree( currBspModel->bspTree);

    } /* End if */
    else
    {
        currNMaps = currGldModel->nMaps;

    } /* End else */


    /* Now draw all the queued triangles */
    for( i = 0U; i < currNMaps; i++)
    {
        if( currNumVerts[i] > 0U)
	{
	    glBindTexture( GL_TEXTURE_2D, currTextures[i]);

	    glDrawElements( 
	        GL_TRIANGLES, 
		currNumVerts[i], 
		GL_UNSIGNED_SHORT, 
		currVertIndices[i]
	    );

	} /* End if */

    } /* End for */

    glFinish( );
    CHECK_GL_ERROR;

    /* Swap buffers to display, since we're double buffered */
    SDL_GL_SwapBuffers();

    /* Calculate FPS */
    endTime = SDL_GetTicks( );

    if( endTime > startTime)
    {
	currFPS = ( 1000U / ( endTime - startTime));

    } /* End if */

} /* End function RenderFrame */


/**
 * Initialises texture objects used by the models.
 */
void InitTextures( void)
{
    char texFileName[256];
    Uint32 loadedSoFar, totalTextures;
    Uint16 i;

    /* Load texture for the progress bar window */
    glGenTextures( 1, &progBarTexture);

    strcpy( texFileName, IMGS_FOLDER_PFX);
    strcat( texFileName, PROG_BAR_IMG);
    LoadJPGTexture( texFileName, progBarTexture);


    ShowProgressBar( 0U);
    
    loadedSoFar = 0U;
    totalTextures = (Uint32 )numExtMaps + (Uint32 )numIntMaps;


    /* Load textures for the Taj exterior */

    if( numExtMaps > 0U)
    {
	extTextures = 
	    (GLuint *)( malloc( sizeof( GLuint) * numExtMaps));
        extTexPriorities = 
	    (GLfloat *)( malloc( sizeof( GLfloat) * numExtMaps));
	if( ( extTextures == NULL) || ( extTexPriorities == NULL))
	{
	    fprintf( stderr, "\nFATAL ERROR: Out of Memory!\n");
	    exit( EXIT_FAILURE);

	} /* End if */

	glGenTextures( numExtMaps, extTextures);
	CHECK_GL_ERROR;

	for( i = 0U; i < numExtMaps; i++)
	{
	    /* Load the texture image */
	    strcpy( texFileName, IMGS_FOLDER_PFX);
	    strcat( texFileName, 
	        ( ( useBSP == GL_TRUE) ? 
		    extBspModel->mapNames[i] :
		    extGldModel->mapNames[i]
		)
	    );

	    LoadJPGTexture( texFileName, extTextures[i]);

            loadedSoFar++;
            if( ( loadedSoFar % 10U) == 0U)
	    {
		ShowProgressBar( ( loadedSoFar * 100U) / totalTextures);

	    } /* End if */


            /* Calculate the texture's relative abundance */
	    if( useBSP == GL_TRUE)
	    {
		extTexPriorities[i] = ( 
		    (GLfloat )( extBspModel->mapTriNums[i]) / 
		    (GLfloat )( extBspModel->numTri)
		);

	    } /* End if */
	    else
	    {
		extTexPriorities[i] = ( 
		    (GLfloat )( extGldModel->mapTriNums[i]) / 
		    (GLfloat )( extGldModel->numTri)
		);

	    } /* End else */

	} /* End for */

    } /* End if */


    /* Load textures for the Taj interior */

    if( numIntMaps > 0U)
    {
	intTextures = 
	    (GLuint *)( malloc( sizeof( GLuint) * numIntMaps));
        intTexPriorities = 
	    (GLfloat *)( malloc( sizeof( GLfloat) * numIntMaps));
	if( ( intTextures == NULL) || ( intTexPriorities == NULL))
	{
	    fprintf( stderr, "\nFATAL ERROR: Out of Memory!\n");
	    exit( EXIT_FAILURE);

	} /* End if */

	glGenTextures( numIntMaps, intTextures);
	CHECK_GL_ERROR;

	for( i = 0U; i < numIntMaps; i++)
	{
	    /* Load the texture image */
	    strcpy( texFileName, IMGS_FOLDER_PFX);
	    strcat( texFileName, 
	        ( ( useBSP == GL_TRUE) ? 
		    intBspModel->mapNames[i] :
		    intGldModel->mapNames[i]
		)
	    );

	    LoadJPGTexture( texFileName, intTextures[i]);

            loadedSoFar++;
            if( ( loadedSoFar % 10U) == 0U)
	    {
		ShowProgressBar( ( loadedSoFar * 100U) / totalTextures);

	    } /* End if */


            /* Calculate the texture's relative abundance */
	    if( useBSP == GL_TRUE)
	    {
		intTexPriorities[i] = ( 
		    (GLfloat )( intBspModel->mapTriNums[i]) / 
		    (GLfloat )( intBspModel->numTri)
		);

	    } /* End if */
	    else
	    {
		intTexPriorities[i] = ( 
		    (GLfloat )( intGldModel->mapTriNums[i]) / 
		    (GLfloat )( intGldModel->numTri)
		);

            } /* End else */

	} /* End for */

    } /* End if */

    ShowProgressBar( 100U);

    /* We no longer need the progress bar texture */
    glDeleteTextures( 1, &progBarTexture);

} /* End function InitTextures */


/**
 * Recursively draw a BSP Tree. Instead of actually drawing
 * the triangles of the tree, collects vertex indices of visible
 * triangles. Performs backface and view cone based culling.
 */
void DrawBSPTree( BSPTree *aTree)
{
    register Uint16 i;

    if( aTree != NULL)
    {
        PointType vpRel = ClassifyPoint( vPos, &( aTree->partPlane));
	GLdouble vpDotProd;

        vpDotProd = 
	    vNorm[0]*aTree->partPlane.A + vNorm[2]*aTree->partPlane.C;

        /* 'vNorm[1]' is always zero */

	if( 0 && ( vpRel == BELOW_PLANE) && ( vpDotProd < -minVisCos))
	{
	    /* The front sub-tree can not be seen */

	} /* End if */
	else if( aTree->front != NULL)
	{
	    DrawBSPTree( aTree->front);

	} /* End else-if */


	for( i = 0U; i < aTree->numTri; i++)
	{
	    register Uint32 tIndex;
	    register BSPTriFace *aTri;
	    GLfloat res[3], dotProd;
	    
	    aTri = ( aTree->triDefs + i);

	    if( insideTaj == GL_FALSE)
	    {
		/* Backface culling can be done only for the
		 * Taj exterior model.
		 */
		res[0] = 
		    currBspModel->vertCoords[3*aTri->vIndices[0] + 0];
		res[1] = 
		    currBspModel->vertCoords[3*aTri->vIndices[0] + 1];
		res[2] = 
		    currBspModel->vertCoords[3*aTri->vIndices[0] + 2];

		res[0] -= vPos[0];
		res[1] -= vPos[1];
		res[2] -= vPos[2];

		res[0] *= aTree->partPlane.A;
		res[1] *= aTree->partPlane.B;
		res[2] *= aTree->partPlane.C;

		dotProd = res[0] + res[1] + res[2];

		if( dotProd >= 0.0F)
		{
		    continue;

		} /* End if */

	    } /* End if */

	    tIndex = currNumVerts[aTri->texIndex];

	    currVertIndices[aTri->texIndex][tIndex++] = 
		aTri->vIndices[0];

	    currVertIndices[aTri->texIndex][tIndex++] = 
		aTri->vIndices[1];

	    currVertIndices[aTri->texIndex][tIndex++] = 
		aTri->vIndices[2];

	    currNumVerts[aTri->texIndex] = tIndex;

	} /* End for */


	if( ( vpRel == ABOVE_PLANE) && ( vpDotProd > minVisCos))
	{
	    /* The back sub-tree can not be seen */

	} /* End if */
	else if( aTree->back != NULL)
	{
	    DrawBSPTree( aTree->back);

	} /* End if */

    } /* End if */

} /* End function DrawBSPTree */


/**
 * A simple function to show a progress bar to the user while
 * the textures are being loaded.
 */
void ShowProgressBar( unsigned int percentComplete)
{
    /* Assume Ortho-2D projection, with coordinate system matching
     * screen resolution has been set up.
     *
     * The progress window is 256x128, and is to be centred.
     */

    int startX = ( scrWidth - 256) / 2;
    int startY = ( scrHeight - 128) / 2;

    glClear( GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
    CHECK_GL_ERROR;

    glBindTexture( GL_TEXTURE_2D, progBarTexture);
    CHECK_GL_ERROR;

    glBegin( GL_QUADS);

	glTexCoord2f( 0, 0.5);
	glVertex2f( startX, startY);

	glTexCoord2f( 0.99, 0.5);
	glVertex2f( startX + 255, startY);

	glTexCoord2f( 0.99, 0.0);
	glVertex2f( startX + 255, startY + 127);

	glTexCoord2f( 0, 0.0);
	glVertex2f( startX, startY + 127);

    glEnd( );
    CHECK_GL_ERROR;

    glDisable( GL_TEXTURE_2D);
    glColor3f( 0.0F, 0.0F, 0.7F);
    glBegin( GL_QUADS);
        glVertex2i( startX + 6, startY + 18);
        glVertex2i( startX + ( 244 * percentComplete)/100, startY + 18);
        glVertex2i( startX + ( 244 * percentComplete)/100, startY + 60);
        glVertex2i( startX + 6, startY + 60);
    glEnd( );
    glColor3f( 1.0F, 1.0F, 1.0F);
    glEnable( GL_TEXTURE_2D);

    /* Swap buffers to display, since we're double buffered */
    SDL_GL_SwapBuffers();

} /* End function ShowProgressBar */


/**
 * Loads a JPEG texture, creating an alpha channel - "sufficiently
 * black" pixels are considered transparent. This is used to
 * show the grills in the Taj interiors.
 */
int LoadJPGTexture( const char *fileName, GLuint texObjId)
{
    int retVal;
    Uint32 rmask, gmask, bmask, amask;
    SDL_Surface *image = NULL;
    SDL_Surface *bbImage = NULL;
  
    /* RGBA masks will have to depend on the endianness of the
     * the machine. (A fraud, since the rest of the code heavily
     * depends on it being a 32-bit, i386 compatible machine.)
     */
#if SDL_BYTEORDER == SDL_BIG_ENDIAN
    rmask = 0xFF000000;
    gmask = 0x00FF0000;
    bmask = 0x0000FF00;
    amask = 0x000000FF;
#else
    rmask = 0x000000FF;
    gmask = 0x0000FF00;
    bmask = 0x00FF0000;
    amask = 0xFF000000;
#endif

    image = IMG_Load( fileName);

    if( image != NULL)
    {
        bbImage = SDL_CreateRGBSurface( 
	    SDL_SWSURFACE, image->w, image->h, 32, 
	    rmask, gmask, bmask, amask
        );

	if( bbImage == NULL)
	{
	    fprintf( stderr, "ERROR: Could not create billboard image! (%s)\n",
	        SDL_GetError( )
	    );
	    fflush( stderr);

	} /* End if */
	else
	{
	    int i, totalPixels = ( image->w * image->h);
	    Uint8 *bbPixels = (Uint8 *)( bbImage->pixels);

	    for( i = 0; i < totalPixels; i++)
	    {
	        /* For the moment, assume 24-bit RGB in 
		 * little-endian form.
		 */
                
		bbPixels[4*i + 0] = ((Uint8 *)( image->pixels))[3*i + 0];
		bbPixels[4*i + 1] = ((Uint8 *)( image->pixels))[3*i + 1];
		bbPixels[4*i + 2] = ((Uint8 *)( image->pixels))[3*i + 2];

		if( ( bbPixels[4*i + 0] <= 5) &&
		    ( bbPixels[4*i + 1] <= 5) &&
		    ( bbPixels[4*i + 2] <= 5)
                )
		{
		    bbPixels[4*i + 3] = 0x00;

		} /* End if */
		else
		{
		    bbPixels[4*i + 3] = 0xff;

		} /* End else */

	    } /* End for */
	    
	} /* End else */

	SDL_FreeSurface( image);

	glBindTexture( GL_TEXTURE_2D, texObjId);
	CHECK_GL_ERROR;

	glTexParameteri( GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT);
	CHECK_GL_ERROR;

	glTexParameteri( GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT);
	CHECK_GL_ERROR;

	glTexParameteri( 
	    GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR
	);
	CHECK_GL_ERROR;

	glTexParameteri( 
	    GL_TEXTURE_2D, 
	    GL_TEXTURE_MIN_FILTER, 
	    GL_LINEAR_MIPMAP_NEAREST
	);
	CHECK_GL_ERROR;

	gluBuild2DMipmaps(
	    GL_TEXTURE_2D,
	    GL_RGBA,
	    bbImage->w, bbImage->h,
	    GL_RGBA, GL_UNSIGNED_BYTE,
	    bbImage->pixels
	);
	CHECK_GL_ERROR;

        retVal = 0;

	SDL_FreeSurface( bbImage);

    } /* End if */
    else
    {
	fprintf( 
	    stderr,
	    "\nERROR: Could not load image \"%s\" (%s)\n",
	    fileName, SDL_GetError( )
	);
	fflush( stderr);

        retVal = -1;

    } /* End else */

    return retVal;

} /* End function LoadJPGTexture */


/** 
 * Frees up the resources at the end of the program.
 */
void FreeResources( void)
{
    Uint32 i;

    /* Free the external model and associated resources */
    for( i = 0U; i < numExtMaps; i++)
    {
        extNumVerts[i] = 0U;
	free( extVertIndices[i]);
	extVertIndices[i] = NULL;

    } /* End for */

    free( extNumVerts);
    extNumVerts = NULL;
    free( extVertIndices);
    extVertIndices = NULL;

    glDeleteTextures( numExtMaps, extTextures);
    CHECK_GL_ERROR;

    free( extTextures);
    extTextures = NULL;
    free( extTexPriorities);
    extTexPriorities = NULL;

    if( useBSP == GL_TRUE)
    {
	FreeBSPTreeData( extBspModel);
	extBspModel = NULL;

    } /* End if */
    else
    {
        FreeGLData( extGldModel);
	extGldModel = NULL;

    } /* End else */
    
    FreeGLData( extColDetModel);
    extColDetModel = NULL;


    /* Ditto for the internal model and associated resources */
    for( i = 0U; i < numIntMaps; i++)
    {
        intNumVerts[i] = 0U;
	free( intVertIndices[i]);
	intVertIndices[i] = NULL;

    } /* End for */

    free( intNumVerts);
    intNumVerts = NULL;
    free( intVertIndices);
    intVertIndices = NULL;

    glDeleteTextures( numIntMaps, intTextures);
    CHECK_GL_ERROR;

    free( intTextures);
    intTextures = NULL;
    free( intTexPriorities);
    intTexPriorities = NULL;

    if( useBSP == GL_TRUE)
    {
	FreeBSPTreeData( intBspModel);
	intBspModel = NULL;

    } /* End if */
    else
    {
	FreeGLData( intGldModel);
	intGldModel = NULL;

    } /* End else */

    FreeGLData( intColDetModel);
    intColDetModel = NULL;

} /* End function FreeResources */

