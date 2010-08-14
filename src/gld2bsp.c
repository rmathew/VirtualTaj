/* Copyright (c) 2001 Ranjit Mathew. All rights reserved.
 * Use of this source code is governed by the terms of the BSD licence
 * that can be found in the LICENCE file.
 */

/*
 * GLD2BSP.C: GLData (GLD) to BSP Tree converter.
 */


#include <stdio.h>
#include <stdlib.h>
#include <float.h>
#include <limits.h>

#include "gld.h"
#include "bsp.h"


/* Constants representing information about command-line args */

#define NUM_REQ_ARGS 2

#define PROG_NAME_ARG 0
#define GLD_FILE_ARG 1
#define OUTFILE_ARG 2


/**
 * Entry point into the GLD2BSP converter program. Takes in the GLD 
 * model and output file names (in that order).
 *
 * All the polygons in the model are assumed to be just triangles,
 * textured and oriented correctly (vertices in anticlockwise
 * order).
 */
int main( int argc, char *argv[])
{
    GLData *inModel = NULL;

    Uint32 nTri;
    GLfloat *triVerts;
    Uint16 *texIndices;
    GLfloat *triTexCoords;

    Uint16 nMaps;
    char **texMapNames = NULL;

    BSPTreeData *bspData = NULL;
    FILE *outFile, *inFile;

    Uint32 i, j, k;
    Uint32 triConverted;


    /* Check command-line arguments */
    if( argc != ( NUM_REQ_ARGS + 1))
    {
        fprintf( stderr, 
	    "GLD2BSP: Generate BSP Tree from a GLD model\n"
	);
        fprintf( stderr, 
	    "Usage: %s <gldfile> <outfile>\n", 
	    argv[PROG_NAME_ARG]
	);

        return EXIT_FAILURE;

    } /* End if */

    
    /* Read in the model */
    inFile = fopen( argv[GLD_FILE_ARG], "rb");
    if( inFile == NULL)
    {
        fprintf( stderr,
	    "\nERROR: Unable to open file \"%s\" for reading!\n",
	    argv[GLD_FILE_ARG]
        );
	return EXIT_FAILURE;

    } /* End if */

    inModel = LoadGLData( inFile);

    fclose( inFile);

    if( inModel == NULL)
    {
        fprintf( stderr, 
	    "\nERROR: Unable to read in GLD model from \"%s\"!\n",
	    argv[GLD_FILE_ARG]
        );
	return EXIT_FAILURE;

    } /* End if */

#ifdef BSPC_DEBUG
    printf( "GLD2BSP: Read GLD model from \"%s\"\n", argv[GLD_FILE_ARG]);
    fflush( stdout);
#endif

    
    /* Reuse texture map names from the input model structure */
    nMaps = inModel->nMaps;
    texMapNames = inModel->mapNames;


    /* Convert the model to the form needed by the BSP tree compiler */
    nTri = inModel->numTri;
    triVerts = (GLfloat *)( malloc( nTri * 3 * 3 * sizeof( GLfloat)));
    triTexCoords = (GLfloat *)( malloc( nTri * 3 * 2 * sizeof( GLfloat)));
    texIndices = (Uint16 *)( malloc( nTri * sizeof( Uint16)));

    if( 
        ( triVerts == NULL) || 
	( triTexCoords == NULL) || 
	( texIndices == NULL))
    {
        fprintf( stderr, "\nFATAL ERROR: Out of Memory!\n");
	exit( EXIT_FAILURE);

    } /* End if */

    
    triConverted = 0U;
    for( i = 0U; i < nMaps; i++)
    {
        Uint16 *triFaceIndices = inModel->triFaces[i];

	for( j = 0U; j < inModel->mapTriNums[i]; j++)
	{
	    texIndices[triConverted] = i;

	    for( k = 0U; k < 3U; k++)
	    {
	        Uint16 vIndex = triFaceIndices[3*j + k];

	        triVerts[9*triConverted + 3*k + 0] = 
		    *( inModel->vertCoords + 3*vIndex + 0);

	        triVerts[9*triConverted + 3*k + 1] = 
		    *( inModel->vertCoords + 3*vIndex + 1);

	        triVerts[9*triConverted + 3*k + 2] = 
		    *( inModel->vertCoords + 3*vIndex + 2);


	        triTexCoords[6*triConverted + 2*k + 0] = 
		    *( inModel->texCoords + 2*vIndex + 0);

	        triTexCoords[6*triConverted + 2*k + 1] = 
		    *( inModel->texCoords + 2*vIndex + 1);

	    } /* End for */
	    
	    triConverted++;

	} /* End for */

    } /* End for */


    /* Generate the BSP Tree */
    bspData = GenBSPTreeData( 
        nTri, triVerts, texIndices, triTexCoords, nMaps, texMapNames
    );

    if( bspData == NULL)
    {
        fprintf( stderr, "\nERROR: Could not generate BSP Tree!\n");
	exit( EXIT_FAILURE);

    } /* End if */


    /* Free up the space taken by the arguments arrays */
    free( triVerts);
    free( triTexCoords);
    free( texIndices);


    /* Now write out the BSP Tree to the given file */
    outFile = fopen( argv[OUTFILE_ARG], "wb");

    if( outFile == NULL)
    {
        fprintf( stderr,
	    "\nERROR: Unable to open file \"%s\" for writing!\n",
	    argv[OUTFILE_ARG]
        );
	return EXIT_FAILURE;

    } /* End if */

    SaveBSPTreeData( bspData, outFile);

    /* Just to be sure */
    fflush( outFile);
    fclose( outFile);

#ifdef BSPC_DEBUG
    printf( "GLD2BSP: BSP tree saved to \"%s\"\n", argv[OUTFILE_ARG]);
    fflush( stdout);
#endif

    /* Free the original model and the BSP Tree, now that 
     * it has been saved.
     */
    FreeBSPTreeData( bspData);
    FreeGLData( inModel);

    
    /* Verify that the tree was properly written out */
    printf( "GLD2BSP: Now loading back the BSP tree...\n");
    fflush( stdout);

    inFile = fopen( argv[OUTFILE_ARG], "rb");
    bspData = LoadBSPTreeData( inFile);
    fclose( inFile);

    if( bspData == NULL)
    {
        fprintf( stderr, "\nERROR: Could not read back saved BSP Tree!\n");
	exit( EXIT_FAILURE);

    } /* End if */

    printf( "GLD2BSP: Done.\n");
    fflush( stdout);


    /* Free the loaded BSP Tree */
    FreeBSPTreeData( bspData);


    return EXIT_SUCCESS;

} /* End function main */


