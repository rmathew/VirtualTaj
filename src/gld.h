/* Copyright (c) 2001 Ranjit Mathew. All rights reserved.
 * Use of this source code is governed by the terms of the BSD licence
 * that can be found in the LICENCE file.
 */

/*
 * GLD.H: Declarations for the GLD file format functions.
 * ('GLD' stands for GLData.)
 */

/**
 * This version of the GLD format can handle only
 * up to 65535 texture maps and 65535 vertex definitions.
 * 
 * Stream format for a GLD file:
 *
 *  1. File Type Identifier: "GLD" (4 bytes, including the '\0')
 *  2. Version: Major + Minor (4 high + 4 low bits). Currently 0x10 (8 bits)
 *
 *  3. nMaps: number of texture maps (16 bits)
 *  4. mapNames: 'nMaps' '\0' terminated strings
 *  5. mapTriNums: number of triangles using each of the maps ('nMaps'x32-bits)
 *
 *  6. nVertices: number of vertex definitions (16 bits)
 *  7. vertCoords: 'nVertices' vertex coordinates (each 3 x 32-bit floats)
 *  8. texCoords: 'nVertices' texture mappings (each 2 x 32-bit floats)
 *
 *  9. minX: Minimum overall X ordinate value (32-bit float)
 * 10. maxX: Maximum overall X ordinate value (32-bit float)
 *
 * 11. minY: Minimum overall Y ordinate value (32-bit float)
 * 12. maxY: Maximum overall Y ordinate value (32-bit float)
 *
 * 13. minZ: Minimum overall Z ordinate value (32-bit float)
 * 14. maxZ: Maximum overall Z ordinate value (32-bit float)
 *
 * 15. numTri: total number of mapped triangles (32 bits)
 * 16. For( 0 <= i < nMaps),
 *         'mapTriNums[i]' vertex definition indices (3 x 16 bits)
 *
 * NOTE: All numbers are little-endian and all strings are in 7-bit ASCII.
 */

#ifndef _GLD_H
#define _GLD_H

/* We do not want SDL to automatically include "glext.h" */
#define NO_SDL_GLEXT

#include "SDL.h"
#include "SDL_opengl.h"


/* These form the "signature" of a GLD file */
#define GLD_FILE_MAGIC "GLD"
#define GLD_VER 0x10


/* Vertex coordinates differing only upto this value in their 
 * respective ordinate magnitudes, are considered essentially 
 * the same. This is roughly equal to what a single pixel
 * maps to on a 1024 vertical resolution display, at a distance 
 * of 1.0, with a total vertical viewing angle of 60 degrees
 * (= 1.0 * tan( 30) / 512).
 */
#define GLD_VERT_ORD_EPSILON 0.0011276372445F


/* Texture coordinates differing only upto this value (= 1/256)
 * in their respective ordinate magnitudes, are considered 
 * essentially the same (since we can only handle texture maps upto 
 * 256x256, and OpenGL uses effective texture coordinates between 
 * 0.0 and 1.0).
 */
#define GLD_TEX_ORD_EPSILON 0.00390625F


/* Data type definitions */

/* Run-time representation of a GLD file.
 */
typedef struct _gldata
{
    Uint16 nMaps;
    char **mapNames;
    Uint32 *mapTriNums;

    Uint16 nVertices;
    GLfloat *vertCoords;  /* 'nVertices' packed triads of (x,y,z) values */
    GLfloat *texCoords;   /* 'nVertices' packed pairs of (u,v) values */

    GLfloat minX, maxX;
    GLfloat minY, maxY;
    GLfloat minZ, maxZ;

    Uint32 numTri;

    /* 'mapTriNums[i]' packed triads of vertex indices
     * for 0 <= i < 'nMaps'.
     */
    Uint16 **triFaces;

} GLData;


/* Function Prototypes */

/**
 * Generates GLData from the given textured triangles.
 * Inputs are the total number of triangles, (x,y,z) values of 
 * each of the vertices of the triangles in anticlockwise order, 
 * indices of the textures of the triangles, (u,v) texture map
 * coordinates at each of the vertices of the triangles in 
 * anticlockwise order, overall number of texture maps, 
 * null-terminated names of the texture maps, respectively (phew!).
 */
extern GLData *GenGLData( 
    Uint32 nTri, 
    GLfloat *triVerts,
    Uint16 *texIndices,
    GLfloat *triTexCoords,
    Uint16 nMaps, char **mapNames
);


/**
 * Saves the given GLData into the given file. 
 * The file must be opened for writing binary
 * data, must have sufficient permissions and the disc 
 * must not be full.
 */
extern void SaveGLData( GLData *glData, FILE *outFile);


/**
 * Loads GLData from the given file.
 * The file must have been opened for reading binary data, must
 * have sufficient permissions, etc.
 * 
 * Returns NULL on error.
 */
extern GLData *LoadGLData( FILE *inFile);


/**
 * Frees GLData that has either been loaded from a file 
 * or freshly generated.
 */
extern void FreeGLData( GLData *glData);

#endif    /* _GLD_H */


