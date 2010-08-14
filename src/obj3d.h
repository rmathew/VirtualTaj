/* Copyright (c) 2001 Ranjit Mathew. All rights reserved.
 * Use of this source code is governed by the terms of the BSD licence
 * that can be found in the LICENCE file.
 */

/*
 * OBJ3D.H: Declarations for the Wavefront Object Loader functions.
 */

#ifndef _OBJ3D_H
#define _OBJ3D_H

#define NO_SDL_GLEXT

#include "SDL.h"
#include "SDL_opengl.h"

/* Macro definitions */

#define minOrd( x1, x2) ( ( (x1) < (x2)) ? (x1) : (x2))
#define maxOrd( x1, x2) ( ( (x1) > (x2)) ? (x1) : (x2))


/* Data type definitions */

typedef struct
{
    GLfloat x, y, z;

} Vertex;

typedef struct
{
    GLfloat nx, ny, nz;

} Normal;

typedef struct
{
    GLfloat u, v;

} TexCoord;

typedef struct
{
    unsigned int vIndices[3];
    int tcIndices[3];
    int nIndices[3];
    int mtlIndex;

} TriFace;

typedef struct
{
    unsigned int numVerts;
    Vertex **vertices;

    unsigned int numTexCoords;
    TexCoord **texCoords;

    unsigned int numNormals;
    Normal **normals;

    unsigned int numFaces;
    TriFace **faces;

    char *mtlLib;
    unsigned int numMtls;
    char **mtls;

    GLfloat minX, maxX;
    GLfloat minY, maxY;
    GLfloat minZ, maxZ;

} Object3d;

typedef struct
{
    char *name;
    GLfloat ambColour[3];
    GLfloat diffColour[3];
    GLfloat specColour[3];
    GLuint illum;
    GLfloat shine;
    char *texMapFile;

} Material;

typedef struct
{
    char *libName;
    unsigned int numMtls;
    Material **mtls;

} MaterialsLib;


/* Function prototypes */

extern Object3d *ReadObjModel( const char *fileName);
extern void FreeObjModel( Object3d *aModel);
extern MaterialsLib *ReadObjMaterialsLib( 
    const char *fileName, const char *name
);
extern void FreeObjMaterialsLib( MaterialsLib *aMatLib);

#endif    /* _OBJ3D_H */


