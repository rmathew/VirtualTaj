/* Copyright (c) 2001 Ranjit Mathew. All rights reserved.
 * Use of this source code is governed by the terms of the BSD licence
 * that can be found in the LICENCE file.
 */

/*
 * OBJ3D.C: Simple Wavefront Object Loader routines.
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <math.h>
#include <limits.h>
#include <float.h>

#include "obj3d.h"

#define MAX_LINE_LEN 256


/* Generic list node to hold a vertex, normal, etc. */
typedef struct dynArrNode
{
    void *anElement;
    struct dynArrNode *next;

} DynArrNode;


/**
 * Reads in an object defined in the given file, assuming it is
 * a simple Wavefront Object file.
 */
Object3d *ReadObjModel( const char *fileName)
{
    Object3d *retVal = NULL;
    FILE *inFile = NULL;
    char lineBuff[MAX_LINE_LEN];


    if( ( inFile = fopen( fileName, "r")) != NULL)
    {
        DynArrNode *vertsHead = NULL;
        DynArrNode *normsHead = NULL;
        DynArrNode *tptsHead = NULL;
	DynArrNode *facesHead = NULL;
	DynArrNode *mtlsHead = NULL;

	int currMtlIndex = -1;


        /* Allocate space for the object */
        retVal = (Object3d *)( malloc( sizeof( Object3d)));
	if( retVal == NULL)
	{
	    fprintf( stderr, "\nFATAL ERROR: Out of Memory!\n");
	    exit( EXIT_FAILURE);

	} /* End if */

        /* Initialise various elements of the object */
	retVal->numVerts = retVal->numTexCoords = retVal->numNormals = 0;

	retVal->vertices = NULL;
	retVal->texCoords = NULL;
	retVal->normals = NULL;

	retVal->numFaces = 0;
	retVal->faces = NULL;

	retVal->mtlLib = NULL;
	retVal->numMtls = 0;
	retVal->mtls = NULL;

	retVal->minX = retVal->minY = retVal->minZ = FLT_MAX;
	retVal->maxX = retVal->maxY = retVal->maxZ = FLT_MIN;

        while( fgets( lineBuff, MAX_LINE_LEN, inFile) != NULL)
	{
	    char *cPtr = lineBuff;

            /* Skip whitespace */
	    for( ; ( isspace( *cPtr) && (*cPtr != '\0')); cPtr++)
	        ;

            /* Skip comments and empty lines */
            if( ( *cPtr != '\0') && ( *cPtr != '#'))
	    {
	        char *ident = strtok( cPtr, " \t\n");

		if( strcmp( "v", ident) == 0)
		{
		    /* This is a Vertex definition */

		    DynArrNode *aNode = 
		        (DynArrNode *)( malloc( sizeof( DynArrNode)));
		    Vertex *aVert = (Vertex *)( malloc( sizeof( Vertex)));

                    if( aNode == NULL)
		    {
		        fprintf( stderr, "\nFATAL ERROR: Out of Memory!\n");
			exit( EXIT_FAILURE);

		    } /* End if */
		    else if( aVert == NULL)
		    {
		        fprintf( stderr, "\nFATAL ERROR: Out of Memory!\n");
			exit( EXIT_FAILURE);

		    } /* End else-if */

		    aVert->x = atof( strtok( NULL, " \t\n"));
		    aVert->y = atof( strtok( NULL, " \t\n"));
		    aVert->z = atof( strtok( NULL, " \t\n"));

		    aNode->anElement = aVert;
		    aNode->next = vertsHead;
		    vertsHead = aNode;

		    retVal->numVerts++;

		    retVal->minX = minOrd( retVal->minX, aVert->x);
		    retVal->minY = minOrd( retVal->minY, aVert->y);
		    retVal->minZ = minOrd( retVal->minZ, aVert->z);

		    retVal->maxX = maxOrd( retVal->maxX, aVert->x);
		    retVal->maxY = maxOrd( retVal->maxY, aVert->y);
		    retVal->maxZ = maxOrd( retVal->maxZ, aVert->z);

		} /* End if */
		else if( strcmp( "vn", ident) == 0)
		{
		    /* This is a Normal definition */

		    DynArrNode *aNode = 
		        (DynArrNode *)( malloc( sizeof( DynArrNode)));
		    Normal *aNorm = 
		        (Normal *)( malloc( sizeof( Normal)));

		    GLdouble normMag;

                    if( aNode == NULL)
		    {
		        fprintf( stderr, "\nFATAL ERROR: Out of Memory!\n");
			exit( EXIT_FAILURE);

		    } /* End if */
		    else if( aNorm == NULL)
		    {
		        fprintf( stderr, "\nFATAL ERROR: Out of Memory!\n");
			exit( EXIT_FAILURE);

		    } /* End else-if */

		    aNorm->nx = atof( strtok( NULL, " \t\n"));
		    aNorm->ny = atof( strtok( NULL, " \t\n"));
		    aNorm->nz = atof( strtok( NULL, " \t\n"));

		    normMag = (GLdouble )( sqrt(
			( (GLdouble )aNorm->nx * (GLdouble )aNorm->nx) + 
			( (GLdouble )aNorm->ny * (GLdouble )aNorm->ny) + 
			( (GLdouble )aNorm->nz * (GLdouble )aNorm->nz)
		    ));

                    /* Normalise the normal (just to be sure) */
		    if( normMag > 0.0)
		    {
		        aNorm->nx = 
			    (GLfloat )( (GLdouble )aNorm->nx / normMag);
		        aNorm->ny = 
			    (GLfloat )( (GLdouble )aNorm->ny / normMag);
		        aNorm->nz = 
			    (GLfloat )( (GLdouble )aNorm->nz / normMag);

		    } /* End if */
		    else
		    {
		        fprintf( stderr, 
			    "\nERROR: Invalid normal vector in input!\n"
                        );

		    } /* End else */

		    aNode->anElement = aNorm;
		    aNode->next = normsHead;
		    normsHead = aNode;

		    retVal->numNormals++;

		} /* End else-if */
		else if( strcmp( "vt", ident) == 0)
		{
		    /* This is a Texture Coordinate definition */

		    DynArrNode *aNode = 
		        (DynArrNode *)( malloc( sizeof( DynArrNode)));
		    TexCoord *aCoord = 
		        (TexCoord *)( malloc( sizeof( TexCoord)));

                    if( aNode == NULL)
		    {
		        fprintf( stderr, "\nFATAL ERROR: Out of Memory!\n");
			exit( EXIT_FAILURE);

		    } /* End if */
		    else if( aCoord == NULL)
		    {
		        fprintf( stderr, "\nFATAL ERROR: Out of Memory!\n");
			exit( EXIT_FAILURE);

		    } /* End else-if */

		    aCoord->u = atof( strtok( NULL, " \t\n"));
		    aCoord->v = atof( strtok( NULL, " \t\n"));

                    /* SDL_image gives images from top to bottom */
		    aCoord->v *= -1.0f;

		    aNode->anElement = aCoord;
		    aNode->next = tptsHead;
		    tptsHead = aNode;

		    retVal->numTexCoords++;

		} /* End else-if */
		else if( strcmp( "f", ident) == 0)
		{
		    /* This is a Face definition */

		    DynArrNode *aNode = 
		        (DynArrNode *)( malloc( sizeof( DynArrNode)));
		    TriFace *aFace = 
		        (TriFace *)( malloc( sizeof( TriFace)));

		    char *vStr[3];
		    int i;

                    if( aNode == NULL)
		    {
		        fprintf( stderr, "\nFATAL ERROR: Out of Memory!\n");
			exit( EXIT_FAILURE);

		    } /* End if */
		    else if( aFace == NULL)
		    {
		        fprintf( stderr, "\nFATAL ERROR: Out of Memory!\n");
			exit( EXIT_FAILURE);

		    } /* End else-if */

		    vStr[0] = strtok( NULL, " \t\n");
		    vStr[1] = strtok( NULL, " \t\n");
		    vStr[2] = strtok( NULL, " \t\n");

		    for( i = 0; i < 3; i++)
		    {
		        char *idxPtr = strtok( vStr[i], "/\n\t ");
			aFace->vIndices[i] = atoi( idxPtr) - 1;

			idxPtr = strtok( NULL, "/\t ");
			if( idxPtr != NULL)
			{
			    if( strcmp( idxPtr, "") != 0)
			    {
				aFace->tcIndices[i] = atoi( idxPtr) - 1;

			    } /* End if */
			    else
			    {
			        aFace->tcIndices[i] = -1;

			    } /* End else */

			    idxPtr = strtok( NULL, "/\n\t ");
			    if( 
			        ( idxPtr != NULL) && 
				( strcmp( idxPtr, "") != 0)
			    )
			    {
				aFace->nIndices[i] = atoi( idxPtr) - 1;

			    } /* End if */
			    else
			    {
				aFace->nIndices[i] = -1;

                            } /* End else */

			} /* End if */
			else
			{
			    aFace->tcIndices[i] = -1;
			    aFace->nIndices[i] = -1;

			} /* End else */

		    } /*  End for */

		    aFace->mtlIndex = currMtlIndex;

		    aNode->anElement = aFace;
		    aNode->next = facesHead;
		    facesHead = aNode;

		    retVal->numFaces++;

		} /* End else-if */
		else if( strcmp( "usemtl", ident) == 0)
		{
		    /* Switch current material */

		    DynArrNode *aNode = 
		        (DynArrNode *)( malloc( sizeof( DynArrNode)));
		    char *mtlName, *tmpMtlName;

                    if( aNode == NULL)
		    {
		        fprintf( stderr, "\nFATAL ERROR: Out of Memory!\n");
			exit( EXIT_FAILURE);

		    } /* End if */

		    tmpMtlName = strtok( NULL, " \t\n");
		    mtlName = (char *)( malloc(
			sizeof( char) * ( strlen( tmpMtlName) + 1)
		    ));
		    if( mtlName == NULL)
		    {
		        fprintf( stderr, "\nFATAL ERROR: Out of Memory!\n");
			exit( EXIT_FAILURE);

		    } /* End if */

		    strcpy( mtlName, tmpMtlName);
                    
		    currMtlIndex++;
                    
		    aNode->anElement = mtlName;
		    aNode->next = mtlsHead;
		    mtlsHead = aNode;

		    retVal->numMtls++;

		} /* End else-if */
		else if( strcmp( "mtllib", ident) == 0)
		{
		    /* Reference to a materials library file */

		    if( retVal->mtlLib == NULL)
		    {
		        char *libFileName = strtok( NULL, " \t\n");

			retVal->mtlLib = (char *)( malloc(
			    sizeof( char) * ( strlen( libFileName) + 1)
			));
			if( retVal->mtlLib == NULL)
			{
			    fprintf( stderr, "\nFATAL ERROR: Out of Memory!\n");
			    exit( EXIT_FAILURE);

			} /* End if */

			strcpy( retVal->mtlLib, libFileName);

		    } /* End if */
		    else
		    {
		        fprintf( stderr, 
			    "\nERROR: Multiple \"mtllib\" in input!\n"
                        );

		    } /* End else */

		} /* End else-if */
		else
		{
		    /* Do not handle other commands */

		} /* End else */

	    } /* End if */

	} /* End while */

	fclose( inFile);

        
	/* Convert list of vertices to array of vertices */
        if( retVal->numVerts > 0)
	{
	    DynArrNode *tmpPtr, *toFreePtr;
	    int i;

	    retVal->vertices = 
		(Vertex **)( malloc( retVal->numVerts * sizeof( Vertex *)));
            if( retVal->vertices == NULL)
	    {
	        fprintf( stderr, "\nFATAL ERROR: Out of Memory!\n");
		exit( EXIT_FAILURE);

	    } /* End if */

            tmpPtr = vertsHead;
            for( i = ( retVal->numVerts - 1); i >= 0; i--)
	    {
	        retVal->vertices[i] = tmpPtr->anElement;

                toFreePtr = tmpPtr;
		tmpPtr = tmpPtr->next;

		toFreePtr->next = NULL;
		free( toFreePtr);

	    } /* End for */

	} /* End if */


	/* Convert list of normals to array of normals */
        if( retVal->numNormals > 0)
	{
	    DynArrNode *tmpPtr, *toFreePtr;
	    int i;

	    retVal->normals = 
		(Normal **)( malloc( retVal->numNormals * sizeof( Normal *)));
            if( retVal->normals == NULL)
	    {
	        fprintf( stderr, "\nFATAL ERROR: Out of Memory!\n");
		exit( EXIT_FAILURE);

	    } /* End if */

            tmpPtr = normsHead;
            for( i = ( retVal->numNormals - 1); i >= 0; i--)
	    {
	        retVal->normals[i] = tmpPtr->anElement;

                toFreePtr = tmpPtr;
		tmpPtr = tmpPtr->next;

		toFreePtr->next = NULL;
		free( toFreePtr);

	    } /* End for */

	} /* End if */


	/* Convert list of texture-coordinates to array of tex-coords */
        if( retVal->numTexCoords > 0)
	{
	    DynArrNode *tmpPtr, *toFreePtr;
	    int i;

	    retVal->texCoords = (TexCoord **)( 
	        malloc( retVal->numTexCoords * sizeof( TexCoord *))
	    );
	    if( retVal->texCoords == NULL)
	    {
	        fprintf( stderr, "\nFATAL ERROR: Out of Memory!\n");
		exit( EXIT_FAILURE);

	    } /* End if */

            tmpPtr = tptsHead;
            for( i = ( retVal->numTexCoords - 1); i >= 0; i--)
	    {
	        retVal->texCoords[i] = tmpPtr->anElement;

                toFreePtr = tmpPtr;
		tmpPtr = tmpPtr->next;

		toFreePtr->next = NULL;
		free( toFreePtr);

	    } /* End for */

	} /* End if */


        /* Convert list of faces to array of faces */
        if( retVal->numFaces > 0)
	{
	    DynArrNode *tmpPtr, *toFreePtr;
	    int i;

	    retVal->faces = 
	        (TriFace **)( malloc( retVal->numFaces * sizeof( TriFace *)));
            if( retVal->faces == NULL)
	    {
	        fprintf( stderr, "\nFATAL ERROR: Out of Memory!\n");
		exit( EXIT_FAILURE);

	    } /* End if */

            tmpPtr = facesHead;
            for( i = ( retVal->numFaces - 1); i >= 0; i--)
	    {
	        retVal->faces[i] = tmpPtr->anElement;

                toFreePtr = tmpPtr;
		tmpPtr = tmpPtr->next;

		toFreePtr->next = NULL;
		free( toFreePtr);

	    } /* End for */

	} /* End if */


        /* Convert list of materials to array of materials */
        if( retVal->numMtls > 0)
	{
	    DynArrNode *tmpPtr, *toFreePtr;
	    int i;

	    retVal->mtls = 
	        (char **)( malloc( retVal->numMtls * sizeof( char *)));
            if( retVal->mtls == NULL)
	    {
	        fprintf( stderr, "\nFATAL ERROR: Out of Memory!\n");
		exit( EXIT_FAILURE);

	    } /* End if */

            tmpPtr = mtlsHead;
            for( i = ( retVal->numMtls - 1); i >= 0; i--)
	    {
	        retVal->mtls[i] = tmpPtr->anElement;

                toFreePtr = tmpPtr;
		tmpPtr = tmpPtr->next;

		toFreePtr->next = NULL;
		free( toFreePtr);

	    } /* End for */

	} /* End if */


#ifdef OBJ3D_DEBUG
	printf( "\n");
        printf( "OBJ3D: Model Path: \"%s\"\n", fileName);
	printf( "\tVertices: %d\n", retVal->numVerts);
	printf( "\tTexCoords: %d\n", retVal->numTexCoords);
	printf( "\tNormals: %d\n", retVal->numNormals);
	printf( "\tFaces: %d\n", retVal->numFaces);

	printf( "\tX-Range: %f -> %f\n", retVal->minX, retVal->maxX);
	printf( "\tY-Range: %f -> %f\n", retVal->minY, retVal->maxY);
	printf( "\tZ-Range: %f -> %f\n", retVal->minZ, retVal->maxZ);

	if( retVal->mtlLib != NULL)
	{
	    printf( "\tMaterials Lib: %s\n", retVal->mtlLib);

	} /* End if */

	printf( "\tNumber of Materials Used: %d\n", retVal->numMtls);
	fflush( stdout);
#endif

    } /* End if */

    return retVal;

} /* End function ReadObjModel */


/**
 * Frees up all the memory associated with the given Object
 * that was obtained from a previous call to ReadObjModel( ).
 */
void FreeObjModel( Object3d *aModel)
{
    unsigned int i;

    if( aModel != NULL)
    {
	for( i = 0; i < aModel->numVerts; i++)
	{
	    free( aModel->vertices[i]);

	} /* End for */

	free( aModel->vertices);


	for( i = 0; i < aModel->numTexCoords; i++)
	{
	    free( aModel->texCoords[i]);

	} /* End for */

	free( aModel->texCoords);


	for( i = 0; i < aModel->numNormals; i++)
	{
	    free( aModel->normals[i]);

	} /* End for */

	free( aModel->normals);


	for( i = 0; i < aModel->numFaces; i++)
	{
	    free( aModel->faces[i]);

	} /* End for */

	free( aModel->faces);


	if( aModel->mtlLib != NULL)
	{
	    free( aModel->mtlLib);

	} /* End if */

	for( i = 0; i < aModel->numMtls; i++)
	{
	    free( aModel->mtls[i]);

	} /* End for */

        free( aModel->mtls);

	free( aModel);
    
    } /* End if */

} /* End function FreeObjModel */


/**
 * Reads in a Wavefront materials library from the given
 * file.
 */
MaterialsLib *ReadObjMaterialsLib( const char *fileName, const char *givenName)
{
    MaterialsLib *retVal = NULL;
    FILE *inFile = NULL;
    char lineBuff[MAX_LINE_LEN];


    if( ( inFile = fopen( fileName, "r")) != NULL)
    {
        DynArrNode *mtlsHead = NULL;

        /* Allocate space for the materials library */
        retVal = (MaterialsLib *)( malloc( sizeof( MaterialsLib)));
	if( retVal == NULL)
	{
	    fprintf( stderr, "\nFATAL ERROR: Out of Memory!\n");
	    exit( EXIT_FAILURE);

	} /* End if */


        /* Initialise various elements of the materials library */

	retVal->libName = 
	    (char *)( malloc( sizeof( char) * ( strlen( givenName) + 1)));
        if( retVal->libName == NULL)
	{
	    fprintf( stderr, "\nFATAL ERROR: Out of Memory!\n");
	    exit( EXIT_FAILURE);

	} /* End if */

	strcpy( retVal->libName, givenName);

	retVal->numMtls = 0;
	retVal->mtls = NULL;

        /* Read in the materials library */
        while( fgets( lineBuff, MAX_LINE_LEN, inFile) != NULL)
	{
	    char *cPtr = lineBuff;

            /* Skip whitespace */
	    for( ; ( isspace( *cPtr) && (*cPtr != '\0')); cPtr++)
	        ;

            /* Skip comments and empty lines */
            if( ( *cPtr != '\0') && ( *cPtr != '#'))
	    {
	        char *ident = strtok( cPtr, " \t\n");

		if( strcmp( "newmtl", ident) == 0)
		{
		    /* Start a new material definition */

		    char *mtlName = strtok( NULL, " \t\n");
		    DynArrNode *aNode = 
		        (DynArrNode *)( malloc( sizeof( DynArrNode)));
                    Material *aMtl = 
		        (Material *)( malloc( sizeof( Material)));

                    if( aNode == NULL)
		    {
		        fprintf( stderr, "\nFATAL ERROR: Out of Memory!\n");
			exit( EXIT_FAILURE);

		    } /* End if */
                    else if( aMtl == NULL)
		    {
		        fprintf( stderr, "\nFATAL ERROR: Out of Memory!\n");
			exit( EXIT_FAILURE);

		    } /* End else-if */


                    aMtl->name = (char *)( 
		        malloc( sizeof( char) * ( strlen( mtlName) + 1))
		    );
                    if( aMtl->name == NULL)
		    {
		        fprintf( stderr, "\nFATAL ERROR: Out of Memory!\n");
			exit( EXIT_FAILURE);

		    } /* End if */

		    strcpy( aMtl->name, mtlName);

		    aMtl->illum = 0;
		    aMtl->shine = 1.0f;
		    aMtl->texMapFile = NULL;

		    aNode->anElement = aMtl;
		    aNode->next = mtlsHead;
		    mtlsHead = aNode;

		    retVal->numMtls++;

		} /* End if */
		else if( strcmp( "Ka", ident) == 0)
		{
		    /* Define ambient colour of current material */

		    Material *currMat = (Material *)( mtlsHead->anElement);

		    currMat->ambColour[0] = atof( strtok( NULL, " \t\n"));
		    currMat->ambColour[1] = atof( strtok( NULL, " \t\n"));
		    currMat->ambColour[2] = atof( strtok( NULL, " \t\n"));

		} /* End else-if */
		else if( strcmp( "Kd", ident) == 0)
		{
		    /* Define diffuse colour of current material */

		    Material *currMat = (Material *)( mtlsHead->anElement);

		    currMat->diffColour[0] = atof( strtok( NULL, " \t\n"));
		    currMat->diffColour[1] = atof( strtok( NULL, " \t\n"));
		    currMat->diffColour[2] = atof( strtok( NULL, " \t\n"));

		} /* End else-if */
		else if( strcmp( "Ks", ident) == 0)
		{
		    /* Define specular colour of current material */

		    Material *currMat = (Material *)( mtlsHead->anElement);

		    currMat->specColour[0] = atof( strtok( NULL, " \t\n"));
		    currMat->specColour[1] = atof( strtok( NULL, " \t\n"));
		    currMat->specColour[2] = atof( strtok( NULL, " \t\n"));

		} /* End else-if */
		else if( strcmp( "illum", ident) == 0)
		{
		    /* How should the current material be illuminated? */

		    Material *currMat = (Material *)( mtlsHead->anElement);

		    currMat->illum = atoi( strtok( NULL, " \t\n"));

		} /* End else-if */
		else if( strcmp( "Ns", ident) == 0)
		{
		    /* Shininess of the material (1.0f to 128.0f) */

		    Material *currMat = (Material *)( mtlsHead->anElement);

		    currMat->shine = atof( strtok( NULL, " \t\n"));

		} /* End else-if */
		else if( strcmp( "map_Kd", ident) == 0)
		{
		    /* Texture map for the material */

		    Material *currMat = (Material *)( mtlsHead->anElement);

		    char *texFileName = strtok( NULL, " \t\n");
		    
		    currMat->texMapFile = (char *)( malloc(
		        sizeof( char) * ( strlen( texFileName) + 1)
		    ));
                    if( currMat->texMapFile == NULL)
		    {
		        fprintf( stderr, "\nFATAL ERROR: Out of Memory!\n");
			exit( EXIT_FAILURE);

		    } /* End if */

		    strcpy( currMat->texMapFile, texFileName);

		} /* End else-if */
            
	    } /* End if */

        } /* End while */

        /* Convert list of materials to array of materials */
        if( retVal->numMtls > 0)
	{
	    DynArrNode *tmpPtr, *toFreePtr;
	    int i;

	    retVal->mtls = 
	        (Material **)( malloc( retVal->numMtls * sizeof( Material *)));
	    if( retVal->mtls == NULL)
	    {
		fprintf( stderr, "\nFATAL ERROR: Out of Memory!\n");
		exit( EXIT_FAILURE);

	    } /* End if */

            tmpPtr = mtlsHead;
            for( i = ( retVal->numMtls - 1); i >= 0; i--)
	    {
	        retVal->mtls[i] = tmpPtr->anElement;

                toFreePtr = tmpPtr;
		tmpPtr = tmpPtr->next;

		toFreePtr->next = NULL;
		free( toFreePtr);

	    } /* End for */
      
        } /* End if */

#ifdef OBJ3D_DEBUG
	printf( "\n");
	printf( "OBJ3D: Materials Library Path: \"%s\"\n", fileName);
	printf( "\tNumber of materials: %d\n", retVal->numMtls);
	fflush( stdout);
#endif

    } /* End if */

    return retVal;

} /* End function ReadObjMaterialsLib */


/**
 * Frees the material library that was previously read in by a call
 * to ReadObjMaterialsLib( ).
 */
void FreeObjMaterialsLib( MaterialsLib *aMatLib)
{
    unsigned int i;

    if( aMatLib != NULL)
    {
        if( aMatLib->libName != NULL)
	{
	    free( aMatLib->libName);

	} /* End if */

	for( i = 0; i < aMatLib->numMtls; i++)
	{
	    free( aMatLib->mtls[i]->name);
	    free( aMatLib->mtls[i]->texMapFile);

	    free( aMatLib->mtls[i]);

	} /* End for */

        free( aMatLib->mtls);

        free( aMatLib);

    } /* End if */

} /* End function FreeObjMaterialsLib */


