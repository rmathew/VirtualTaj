/* Copyright (c) 2001 Ranjit Mathew. All rights reserved.
 * Use of this source code is governed by the terms of the BSD licence
 * that can be found in the LICENCE file.
 */

/*
 * OBJ2GLD.C: Textured Wavefront Model (OBJ) to GLData converter.
 */


#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <float.h>
#include <limits.h>

#include "gld.h"
#include "obj3d.h"


/* Constants representing information about command-line args */

#define NUM_REQ_ARGS 3

#define PROG_NAME_ARG 0
#define MDL_FILE_ARG 1
#define MTL_LIB_ARG 2
#define OUTFILE_ARG 3


/**
 * Entry point into the OBJ2GLD converter program. Takes in the OBJ 
 * model, the materials library and output file names (in that order).
 *
 * All the polygons in the model are assumed to be just triangles,
 * textured and oriented correctly (vertices in anticlockwise
 * order). All of the textures referenced in the model must be
 * found in the materials library.
 */
int main( int argc, char *argv[])
{
    Object3d *inModel = NULL;
    MaterialsLib *inMtlLib = NULL;

    Uint16 nTri;
    GLfloat *triVerts;
    Uint16 *texIndices;
    GLfloat *triTexCoords;

    Uint16 nMaps;
    char **texMapNames = NULL;

    GLData *glData = NULL;
    FILE *outFile, *inFile;

    Uint16 i, j;


    /* Check command-line arguments */
    if( argc != ( NUM_REQ_ARGS + 1))
    {
        fprintf( stderr, 
	    "OBJ2GLD: Generate GLData from a Wavefront OBJ model\n"
	);
        fprintf( stderr, 
	    "Usage: %s <objfile> <mtlfile> <outfile>\n", 
	    argv[PROG_NAME_ARG]
	);

        return EXIT_FAILURE;

    } /* End if */

    
    /* Read in the model */
    inModel = ReadObjModel( argv[MDL_FILE_ARG]);

    if( inModel == NULL)
    {
        fprintf( stderr, 
	    "\nERROR: Unable to read in OBJ model from \"%s\"!\n",
	    argv[MDL_FILE_ARG]
        );
	return EXIT_FAILURE;

    } /* End if */

    printf( "OBJ2GLD: Read OBJ model from \"%s\"\n", argv[MDL_FILE_ARG]);
    fflush( stdout);

    
    /* Read in the materials library */
    inMtlLib = ReadObjMaterialsLib( argv[MTL_LIB_ARG], inModel->mtlLib);

    if( inMtlLib == NULL)
    {
        fprintf( stderr, 
	    "\nERROR: Unable to read in materials library from \"%s\"!\n",
	    argv[MTL_LIB_ARG]
        );
	FreeObjModel( inModel);
	return EXIT_FAILURE;

    } /* End if */

    printf( "OBJ2GLD: Read materials library from \"%s\"\n", argv[MTL_LIB_ARG]);
    fflush( stdout);


    /* Get names of the texture maps used in the model */
    nMaps = inModel->numMtls;

    texMapNames = 
        (char **)( malloc( sizeof( char *) * inModel->numMtls));
    if( texMapNames == NULL)
    {
        fprintf( stderr, "\nFATAL ERROR: Out of Memory!\n");
	exit( EXIT_FAILURE);

    } /* End if */


    /* For each of the referenced materials in the model, find the
     * corresponding material's texture map in the materials library.
     */
    for( i = 0; i < ( inModel->numMtls); i++)
    {
	for( j = 0; j < inMtlLib->numMtls; j++)
	{
	    if( strcmp( inMtlLib->mtls[j]->name, inModel->mtls[i]) == 0)
	    {
		break;

	    } /* End if */
	    
	} /* End for */

	if( ( j < inMtlLib->numMtls) && 
	    ( inMtlLib->mtls[j]->texMapFile != NULL)
	)
	{
	    /* We found a match */
	    char *tFile = inMtlLib->mtls[j]->texMapFile;

	    texMapNames[i] = 
	        (char *)( malloc( ( strlen( tFile) + 1) * sizeof( char)));
	    if( texMapNames[i] == NULL)
	    {
		fprintf( stderr, "\nFATAL ERROR: Out of Memory!\n");
		exit( EXIT_FAILURE);

	    } /* End if */


	    strcpy( texMapNames[i], tFile);

	} /* End if */
	else
	{
	    /* We do not handle triangles that are not textured */

	    fprintf( 
	        stderr, 
	        "\nERROR: No match in materials lib for \'%s\' from model!\n",
                inModel->mtls[i]
            );
	    exit( EXIT_FAILURE);

	} /* End else */

    } /* End for */


    /* Convert the model to the form needed by the GLData generator */
    nTri = inModel->numFaces;
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


    for( i = 0U; i < nTri; i++)
    {
        TriFace *aFace = (inModel->faces)[i];

        texIndices[i] = aFace->mtlIndex;

	for( j = 0U; j < 3U; j++)
	{
	    unsigned int vertIndex = aFace->vIndices[j];
	    int texIndex = aFace->tcIndices[j];

	    triVerts[i*9 + 3*j + 0] = inModel->vertices[ vertIndex]->x;
	    triVerts[i*9 + 3*j + 1] = inModel->vertices[ vertIndex]->y;
	    triVerts[i*9 + 3*j + 2] = inModel->vertices[ vertIndex]->z;

	    triTexCoords[i*6 + 2*j + 0] = inModel->texCoords[ texIndex]->u;
	    triTexCoords[i*6 + 2*j + 1] = inModel->texCoords[ texIndex]->v;

	} /* End for */

    } /* End for */


    /* Free up the space taken up by the model and the materials library */
    FreeObjModel( inModel);
    FreeObjMaterialsLib( inMtlLib);


    /* Generate GLData */
    glData = GenGLData( 
        nTri, triVerts, texIndices, triTexCoords, nMaps, texMapNames
    );

    if( glData == NULL)
    {
        fprintf( stderr, "\nERROR: Could not generate GLData!\n");
	exit( EXIT_FAILURE);

    } /* End if */
    else
    {
	printf( "OBJ2GLD: GLData successfully generated!\n");
	printf( 
	    "\t( %hu vertices and %u triangles)\n",
	    glData->nVertices, glData->numTri
	);
	fflush( stdout);

    } /* End else */


    /* Free up the space taken by the arguments arrays */
    for( i = 0U; i < nMaps; i++)
    {
        free( texMapNames[i]);

    } /* End for */
    free( texMapNames);

    free( triVerts);
    free( triTexCoords);
    free( texIndices);


    /* Now write out the GLData to the given file */
    outFile = fopen( argv[OUTFILE_ARG], "wb");

    if( outFile == NULL)
    {
        fprintf( stderr,
	    "\nERROR: Unable to open file \"%s\" for writing!\n",
	    argv[OUTFILE_ARG]
        );
	return EXIT_FAILURE;

    } /* End if */

    SaveGLData( glData, outFile);

    /* Just to be sure */
    fflush( outFile);
    fclose( outFile);

    printf( "OBJ2GLD: GLData saved to \"%s\"\n", argv[OUTFILE_ARG]);
    fflush( stdout);

    /* Free the GLData now that it has been saved */
    FreeGLData( glData);

    
    /* Verify that the data was properly written out */
    printf( "OBJ2GLD: Now loading back the saved data...\n");
    fflush( stdout);

    inFile = fopen( argv[OUTFILE_ARG], "rb");
    glData = LoadGLData( inFile);
    fclose( inFile);

    if( glData == NULL)
    {
        fprintf( stderr, "\nERROR: Could not read back saved GLData!\n");
	exit( EXIT_FAILURE);

    } /* End if */

    printf( "OBJ2GLD: Done.\n");
    fflush( stdout);


    /* Free the loaded GLData */
    FreeGLData( glData);


    return EXIT_SUCCESS;

} /* End function main */


