/* Copyright (c) 2001 Ranjit Mathew. All rights reserved.
 * Use of this source code is governed by the terms of the BSD licence
 * that can be found in the LICENCE file.
 */

/*
 * GLD.C: Functions for working with GLData.
 */


#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include <float.h>
#include <limits.h>

#include "gld.h"


GLData *GenGLData( 
    Uint32 nTri, 
    GLfloat *triVerts, 
    Uint16 *texIndices, 
    GLfloat *triTexCoords, 
    Uint16 nMaps, char **texMapNames
)
{
    GLData *retVal;
    Uint32 i, j, k;
    GLboolean skippedTris = GL_FALSE;

    
    /* Check the sanity of the input */
    if( ( nTri == 0U) || ( triVerts == NULL) || ( texIndices == NULL) ||
        ( triTexCoords == NULL) || ( nMaps == 0U) || ( texMapNames == NULL)
    )
    {
        fprintf( stderr, 
	    "ERROR: GenGLData( ): Invalid input parameters!\n"
	);
        return NULL;

    } /* End if */

    retVal = (GLData *)( malloc( sizeof( GLData )));
    if( retVal == NULL)
    {
        fprintf( stderr, "\nFATAL ERROR: Out of Memory!\n");
	exit( EXIT_FAILURE);

    } /* End if */


    retVal->numTri = 0U;

    retVal->minX = retVal->minY = retVal->minZ = FLT_MAX;
    retVal->maxX = retVal->maxY = retVal->maxZ = FLT_MIN;

    retVal->nMaps = nMaps;


    /* Count the number of triangles associated with each texture */
    /* NOTE: Uses calloc( ) to initialise the contents to 0 */

    retVal->mapTriNums = (Uint32 *)( calloc( nMaps, sizeof( Uint32)));
    if( retVal->mapTriNums == NULL)
    {
        fprintf( stderr, "\nFATAL ERROR: Out of Memory!\n");
	exit( EXIT_FAILURE);

    } /* End if */

    
    for( i = 0U; i < nTri; i++)
    {
        Uint16 tIndex = texIndices[i];

        if( tIndex >= nMaps)
	{
	    fprintf( stderr, 
		"ERROR: GenGLData( ): Out of bounds texture index (%hu)!\n",
		tIndex
	    );
	    free( retVal->mapTriNums);
	    free( retVal);

	    return NULL;

	} /* End if */
	else
	{
	    retVal->mapTriNums[tIndex]++;

	} /* End else */

    } /* End for */


    /* Initialise texture names, index queues, etc. */

    retVal->mapNames = (char **)( malloc( nMaps * sizeof( char *)));
    if( retVal->mapNames == NULL)
    {
        fprintf( stderr, "\nFATAL ERROR: Out of Memory!\n");
	exit( EXIT_FAILURE);

    } /* End if */

    retVal->triFaces = (Uint16 **)( malloc( nMaps * sizeof( Uint16 *)));
    if( retVal->triFaces == NULL)
    {
        fprintf( stderr, "\nFATAL ERROR: Out of Memory!\n");
	exit( EXIT_FAILURE);

    } /* End if */

    for( i = 0U; i < nMaps; i++)
    {
        retVal->mapNames[i] = 
	    (char *)( malloc( ( strlen( texMapNames[i]) + 1) * sizeof( char)));

        retVal->triFaces[i] = 
	    (Uint16 *)( malloc( 3 * retVal->mapTriNums[i] * sizeof( Uint16)));

        if( ( retVal->triFaces[i] == NULL) ||
	    ( retVal->mapNames[i] == NULL)
	)
	{
	    fprintf( stderr, "\nFATAL ERROR: Out of Memory!\n");
	    exit( EXIT_FAILURE);

	} /* End if */

	strcpy( retVal->mapNames[i], texMapNames[i]);

        /* Reset mapped triangles counter for this texture.
	 * (We calculate it again later, as some triangles might be
	 * discarded.)
	 */
        retVal->mapTriNums[i] = 0U;

    } /* End for */

   
    /* Vertex definitions */

    retVal->nVertices = 0U;


    /* NOTE: The allocated sizes are extremely conservative 
     * estimates. In reality, vertex sharing ensures that we have 
     * a far lower requirement - we shall adjust this later on.
     */
    retVal->vertCoords = 
        (GLfloat *)( malloc( 3 * 3 * nTri * sizeof( GLfloat)));
    retVal->texCoords = 
        (GLfloat *)( malloc( 2 * 3 * nTri * sizeof( GLfloat)));

    if( ( retVal->vertCoords == NULL) || ( retVal->texCoords == NULL))
    {
        fprintf( stderr, "\nFATAL ERROR: Out of Memory!\n");
	exit( EXIT_FAILURE);

    } /* End if */


    /* Find out vertex indices of all the triangles,
     * generating vertex definitions as needed and weeding out 
     * degenerate triangles.
     */
    for( i = 0U; i < nTri; i++)
    {
        Uint16 tIndex = texIndices[i];
	Uint16 vInd[3];

        for( j = 0U; j < 3U; j++)
	{
	    GLfloat V[3], T[2];

	    V[0] = triVerts[9*i + 3*j + 0];
	    V[1] = triVerts[9*i + 3*j + 1];
	    V[2] = triVerts[9*i + 3*j + 2];

	    T[0] = triTexCoords[6*i + 2*j + 0];
	    T[1] = triTexCoords[6*i + 2*j + 1];


            /* Try to find out if a close-enough vertex has already
	     * been defined.
	     */
	    for( k = 0U; k < retVal->nVertices; k++)
	    {
	        if( ( fabs( retVal->vertCoords[3*k + 0] - V[0]) 
		        <= GLD_VERT_ORD_EPSILON) &&
                    ( fabs( retVal->vertCoords[3*k + 1] - V[1])
		        <= GLD_VERT_ORD_EPSILON) &&
                    ( fabs( retVal->vertCoords[3*k + 2] - V[2])
		        <= GLD_VERT_ORD_EPSILON) &&
                    ( fabs( retVal->texCoords[2*k + 0] - T[0])
		        <= GLD_TEX_ORD_EPSILON) &&
                    ( fabs( retVal->texCoords[2*k + 1] - T[1])
		        <= GLD_TEX_ORD_EPSILON)
                )
		{
		    /* We have found a match */
		    break;

		} /* End if */

	    } /* End for */

	    /* Did we find a match? */
	    if( k < retVal->nVertices)
	    {
	        /* Yes, use it */
	        vInd[j] = k;

	    } /* End if */
	    else
	    {
	        /* No, create a new one */
	        /* (NOTE: k *has* to be equal to 'retVal->nVertices') */

	        retVal->vertCoords[3*k + 0] = V[0];
	        retVal->vertCoords[3*k + 1] = V[1];
	        retVal->vertCoords[3*k + 2] = V[2];

	        retVal->texCoords[2*k + 0] = T[0];
	        retVal->texCoords[2*k + 1] = T[1];

		vInd[j] = k;

		retVal->nVertices++;


                /* Is this vertex at the edge of the known universe? */

		retVal->minX = ( V[0] < retVal->minX) ? V[0] : retVal->minX;
		retVal->maxX = ( V[0] > retVal->maxX) ? V[0] : retVal->maxX;

		retVal->minY = ( V[1] < retVal->minY) ? V[1] : retVal->minY;
		retVal->maxY = ( V[1] > retVal->maxY) ? V[1] : retVal->maxY;

		retVal->minZ = ( V[2] < retVal->minZ) ? V[2] : retVal->minZ;
		retVal->maxZ = ( V[2] > retVal->maxZ) ? V[2] : retVal->maxZ;

	    } /* End else */

	} /* End for */

	/* Verify the sanity of the triangle */
	if( ( vInd[0] == vInd[1]) || 
	    ( vInd[1] == vInd[2]) || 
	    ( vInd[2] == vInd[0])
        )
	{
	    skippedTris = GL_TRUE;

#ifdef GLD_DEBUG
            printf( "\nWARNING: Skipping degenerate triangle in input!\n");
	    fflush( stdout);
#endif
	} /* End if */
	else
	{
	    Uint16 idx = retVal->mapTriNums[tIndex];

	    retVal->triFaces[tIndex][3*idx + 0] = vInd[0];
	    retVal->triFaces[tIndex][3*idx + 1] = vInd[1];
	    retVal->triFaces[tIndex][3*idx + 2] = vInd[2];

	    retVal->mapTriNums[tIndex]++;

	    retVal->numTri++;

	} /* End else */

    } /* End for */


    /* Now adjust our memory usage */
    if( retVal->nVertices > 0U)
    {
        /* That was a bogus test just to introduce a new block */

	GLfloat *tmpPtr;


        /* Adjust usage for vertex definitions */

	tmpPtr = (GLfloat *)( realloc( 
	    retVal->vertCoords, ( 3 * retVal->nVertices * sizeof( GLfloat))
	));
	if( tmpPtr == NULL)
	{
	    /* This should not happen since we are only trimming the block */
	    fprintf( stderr, "\nFATAL ERROR: Out of Memory!\n");
	    exit( EXIT_FAILURE);

	} /* End if */
	else
	{
	    retVal->vertCoords = tmpPtr;

	} /* End else */

	tmpPtr = (GLfloat *)( realloc( 
	    retVal->texCoords, ( 2 * retVal->nVertices * sizeof( GLfloat))
	));
	if( tmpPtr == NULL)
	{
	    /* This should not happen since we are only trimming the block */
	    fprintf( stderr, "\nFATAL ERROR: Out of Memory!\n");
	    exit( EXIT_FAILURE);

	} /* End if */
	else
	{
	    retVal->texCoords = tmpPtr;

	} /* End else */

	
	/* If we skipped some triangles, we need to adjust vertex indices
	 * usage.
	 */
        if( skippedTris == GL_TRUE)
	{
	    for( i = 0U; i < nMaps; i++)
	    {
	        if( retVal->mapTriNums[i] == 0U)
		{
		    free( retVal->triFaces[i]);

		} /* End if */
		else
		{
		    Uint16 *aPtr;

		    aPtr = (Uint16 *)( realloc( 
		        retVal->triFaces[i], 
			( 3 * retVal->mapTriNums[i] * sizeof( Uint16))
		    ));

		    if( aPtr == NULL)
		    {
		        /* Should not happen as we are just trimming */
			fprintf( stderr, "\nFATAL ERROR: Out of Memory!\n");
			exit( EXIT_FAILURE);

		    } /* End if */
		    else
		    {
		        retVal->triFaces[i] = aPtr;

		    } /* End else */

		} /* End else */

	    } /* End for */

	} /* End if */

    } /* End if */


    /* Done. */
    return retVal;

} /* End function GenGLData */


void SaveGLData( GLData *glData, FILE *outFile)
{
    if( glData != NULL)
    {
        unsigned int i;
	Uint8 glDataVer = GLD_VER;

        /* Write out the format signature */
        fwrite( 
	    GLD_FILE_MAGIC, 
	    sizeof( char), ( strlen( GLD_FILE_MAGIC) + 1),
	    outFile
	);
       
        /* Write out the current format version */
	fwrite(
	    &glDataVer,
	    sizeof( glDataVer), 1,
	    outFile
	);

        /* Write out texture map names */
	fwrite(
	    &( glData->nMaps),
	    sizeof( glData->nMaps), 1,
	    outFile
	);

	for( i = 0U; i < glData->nMaps; i++)
	{
	    fwrite(
	        glData->mapNames[i],
		sizeof( char), ( strlen( glData->mapNames[i]) + 1),
		outFile
	    );

	} /* End for */

        /* Write out the number of triangles mapped to each texture */
	fwrite(
	    glData->mapTriNums,
	    sizeof( glData->mapTriNums[0]), glData->nMaps,
	    outFile
	);

        /* Write out the vertex definitions */
	fwrite(
	    &( glData->nVertices),
	    sizeof( glData->nVertices), 1,
	    outFile
	);

	fwrite(
	    glData->vertCoords,
	    sizeof( GLfloat), ( 3 * glData->nVertices),
	    outFile
	);

	fwrite(
	    glData->texCoords,
	    sizeof( GLfloat), ( 2 * glData->nVertices),
	    outFile
	);

        /* Write out the model bounds */
	fwrite( &( glData->minX), sizeof( GLfloat), 1, outFile);
	fwrite( &( glData->maxX), sizeof( GLfloat), 1, outFile);

	fwrite( &( glData->minY), sizeof( GLfloat), 1, outFile);
	fwrite( &( glData->maxY), sizeof( GLfloat), 1, outFile);

	fwrite( &( glData->minZ), sizeof( GLfloat), 1, outFile);
	fwrite( &( glData->maxZ), sizeof( GLfloat), 1, outFile);

        /* Write out the number of triangles */
	fwrite( &( glData->numTri), sizeof( glData->numTri), 1, outFile);

	/* Write out the vertex indices for each triangle sorted
	 * according to textures.
	 */
        for( i = 0U; i < glData->nMaps; i++)
	{
	    fwrite( 
	        glData->triFaces[i], 
		sizeof( Uint16), ( 3 * glData->mapTriNums[i]),
		outFile
	    );

	} /* End for */

    } /* End if */

} /* End function SaveGLData */


GLData *LoadGLData( FILE *inFile)
{
    GLData *retVal = NULL;

    if( inFile != NULL)
    {
        unsigned int sigSize;
	Uint8 glDataVer = 0U;
	char *savedSig = NULL;
	unsigned int i;

	sigSize = strlen( GLD_FILE_MAGIC) + 1U;
	savedSig = (char *)( malloc( sigSize * sizeof( char)));
	if( savedSig == NULL)
	{
	    fprintf( stderr, "\nFATAL ERROR: Out of Memory!\n");
	    exit( EXIT_FAILURE);
	    
	} /* End if */

        fread( savedSig, sizeof( char), sigSize, inFile);
	fread( &glDataVer, sizeof( glDataVer), 1, inFile);

	if( ( strcmp( GLD_FILE_MAGIC, savedSig) == 0) && 
	    ( glDataVer == GLD_VER)
        )
	{
	    free( savedSig);

	    retVal = (GLData *)( malloc( sizeof( GLData)));
	    if( retVal == NULL)
	    {
	        fprintf( stderr, "\nFATAL ERROR: Out of Memory!\n");
		exit( EXIT_FAILURE);

	    } /* End if */

            /* Read in texture map names and mapping statistics */
	    fread(
	        &( retVal->nMaps),
		sizeof( retVal->nMaps), 1,
		inFile
	    );

	    retVal->mapNames = 
	        (char **)( malloc( retVal->nMaps * sizeof( char *)));

            retVal->mapTriNums =
	        (Uint32 *)( malloc( retVal->nMaps * sizeof( Uint32)));

            if( ( retVal->mapNames == NULL) ||
	        ( retVal->mapTriNums == NULL)
	    )
	    {
	        fprintf( stderr, "\nFATAL ERROR: Out of Memory!\n");
		exit( EXIT_FAILURE);

	    } /* End if */

	    for( i = 0U; i < retVal->nMaps; i++)
	    {
	        char inpTexName[256];

		int j = -1;

		do
		{
		    j++;
		    fread( &( inpTexName[j]), sizeof( char), 1, inFile);

		} while( ( inpTexName[j] != '\0') && ( j < 255));

		/* Just to be sure */
		inpTexName[255] = '\0';

		retVal->mapNames[i] = (char *)( malloc(
		    ( strlen( inpTexName) + 1) * sizeof( char)
		));

		if( retVal->mapNames[i] == NULL)
		{
		    fprintf( stderr, "\nFATAL ERROR: Out of Memory!\n");
		    exit( EXIT_FAILURE);

		} /* End if */

		strcpy( retVal->mapNames[i], inpTexName);

	    } /* End for */

	    fread(
	        retVal->mapTriNums,
		sizeof( Uint32), retVal->nMaps,
		inFile
	    );

            /* Read in the vertex definitions */
	    fread( 
	        &( retVal->nVertices), 
		sizeof( retVal->nVertices), 1, 
		inFile
	    );

	    retVal->vertCoords = (GLfloat *)( malloc(
	        retVal->nVertices * 3 * sizeof( GLfloat)
	    ));

	    retVal->texCoords = (GLfloat *)( malloc(
	        retVal->nVertices * 2 * sizeof( GLfloat)
	    ));

	    if( ( retVal->vertCoords == NULL) || 
	        ( retVal->texCoords == NULL)
	    )
	    {
		fprintf( stderr, "\nFATAL ERROR: Out of Memory!\n");
		exit( EXIT_FAILURE);

	    } /* End if */

	    fread( 
	        retVal->vertCoords,
	        sizeof( GLfloat), ( 3 * retVal->nVertices),
		inFile
	    );

	    fread( 
	        retVal->texCoords,
	        sizeof( GLfloat), ( 2 * retVal->nVertices),
		inFile
	    );

	    /* Read in the model bounds */
	    fread( &( retVal->minX), sizeof( GLfloat), 1, inFile);
	    fread( &( retVal->maxX), sizeof( GLfloat), 1, inFile);

	    fread( &( retVal->minY), sizeof( GLfloat), 1, inFile);
	    fread( &( retVal->maxY), sizeof( GLfloat), 1, inFile);

	    fread( &( retVal->minZ), sizeof( GLfloat), 1, inFile);
	    fread( &( retVal->maxZ), sizeof( GLfloat), 1, inFile);

            /* Read in the number of triangles */
	    fread( &( retVal->numTri), sizeof( retVal->numTri), 1, inFile);

	    /* Read in the triangle vertex indices sorted on textures */
	    retVal->triFaces = 
	        (Uint16 **)( malloc( retVal->nMaps * sizeof( Uint16 *)));

            if( retVal->triFaces == NULL)
	    {
		fprintf( stderr, "\nFATAL ERROR: Out of Memory!\n");
		exit( EXIT_FAILURE);

	    } /* End if */

	    for( i = 0U; i < retVal->nMaps; i++)
	    {
	        retVal->triFaces[i] = (Uint16 *)( 
		    malloc( 3 * retVal->mapTriNums[i] * sizeof( Uint16))
		);

		if( retVal->triFaces[i] == NULL)
		{
		    fprintf( stderr, "\nFATAL ERROR: Out of Memory!\n");
		    exit( EXIT_FAILURE);

		} /* End if */

                fread(
		    retVal->triFaces[i],
		    sizeof( Uint16), ( 3 * retVal->mapTriNums[i]),
		    inFile
		);

	    } /* End for */

	} /* End if */
	else
	{
#ifdef GLD_DEBUG
            fprintf( stderr, 
	        "\nERROR: Invalid GLData or incorrect version!\n"
	    );
#endif
	} /* End else */

    } /* End if */

    return retVal;

} /* End function LoadGLData */


void FreeGLData( GLData *glData)
{
    if( glData != NULL)
    {
        unsigned int i;

	for( i = 0U; i < glData->nMaps; i++)
	{
	    free( glData->mapNames[i]);
	    glData->mapNames[i] = NULL;

	    free( glData->triFaces[i]);
	    glData->triFaces[i] = NULL;

	    glData->mapTriNums[i] = 0U;

	} /* End for */
	free( glData->mapNames);
	glData->mapNames = NULL;
	
	free( glData->mapTriNums);
	glData->mapTriNums = NULL;

	glData->nMaps = 0U;

        free( glData->vertCoords);
	glData->vertCoords = NULL;

        free( glData->texCoords);
	glData->texCoords = NULL;

	glData->nVertices = 0U;

        free( glData->triFaces);

	free( glData);

    } /* End if */

} /* End function FreeGLData */


