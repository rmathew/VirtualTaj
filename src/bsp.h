/* Copyright (c) 2001 Ranjit Mathew. All rights reserved.
 * Use of this source code is governed by the terms of the BSD licence
 * that can be found in the LICENCE file.
 */

/*
 * BSP.H: Declarations for the BSP Tree Compiler functions.
 */

/**
 * This version of the BSP tree compiler can handle only
 * up to 65535 texture maps and 65535 vertex definitions.
 * 
 * Stream format for a stored BSP tree:
 *
 *  1. File Type Identifier: "BSP" (4 bytes, including the '\0')
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
 * 15. maxDepth: maximum depth of the tree (16 bits)
 * 16. numNodes: total number of tree nodes (16 bits)
 * 17. numTri: total number of mapped triangles (32 bits)
 *
 * 18. BSP tree nodes:
 *         i. numTri: Number of coplanar triangles in this node (16 bits)
 *        ii. triDefs: 'numTri' triangle definitions:
 *                a. texIndex: Texture map index (16 bits)
 *                b. vIndices: Vertex defintion indices (3 x 16 bits)
 *       iii. partPlane: Partition plane equation (4 x 64-bit floats)
 *                (Only if 'numTri' is 0, otherwise computed on loading)
 *        iv. cFlag: Sub-tree flag, if node has back/front sub-trees (8 bits):
 *                Possible values: 0x00, 0xB0, 0x0F, 0xBF
 *                ('B'=Has back sub-tree, 'F'=Has front sub-tree)
 *
 * NOTE: All numbers are little-endian and all strings are in 7-bit ASCII.
 */

#ifndef _BSP_H
#define _BSP_H

/* We do not want SDL to automatically include "glext.h" */
#define NO_SDL_GLEXT

#include "SDL.h"
#include "SDL_opengl.h"


/* These form the "signature" of a saved BSP Tree data file */
#define BSP_FILE_MAGIC "BSP"
#define BSP_DATA_VER 0x10


/* Vertex coordinates differing only upto this value in their 
 * respective ordinate magnitudes, are considered essentially 
 * the same. This is roughly equal to what a single pixel
 * maps to on a 1024 vertical resolution display, at a distance 
 * of 1.0, with a total vertical viewing angle of 60 degrees
 * (= 1.0 * tan( 30) / 512).
 */
#define BSP_VERT_ORD_EPSILON 0.0011276372445F


/* Texture coordinates differing only upto this value (= 1/256)
 * in their respective ordinate magnitudes, are considered 
 * essentially the same (since we can only handle texture maps upto 
 * 256x256, and OpenGL uses effective texture coordinates between 
 * 0.0 and 1.0).
 */
#define BSP_TEX_ORD_EPSILON 0.00390625F


/* Data type definitions */

/* Type of a point with respect to a partition plane */
typedef enum { BELOW_PLANE = 0, ON_PLANE, ABOVE_PLANE} PointType;


/* A partition plane (or any plane for that matter) definition */
typedef struct _bsp_plane
{
    /* From the plane equation "Ax + By + Cz + D = 0" */
    GLdouble A, B, C, D;

} BSPPlane;


/* A texture-mapped, triangular face */
typedef struct _bsp_tri_face
{
    Uint16 texIndex;
    Uint16 vIndices[3];

} BSPTriFace;


/* A BSP tree node corresponding to a partition plane and containing
 * coplanar texture-mapped triangular faces.
 */
typedef struct _bsp_tree
{
    Uint16 numTri;
    BSPTriFace *triDefs;

    BSPPlane partPlane;

    struct _bsp_tree *back;
    struct _bsp_tree *front;

} BSPTree;


/* A container for a BSP tree with all kinds of information related
 * to the tree.
 */
typedef struct _bsp_tree_data
{
    Uint16 nMaps;
    char **mapNames;
    Uint32 *mapTriNums;

    Uint16 nVertices;
    GLfloat *vertCoords;    /* 'nVertices' packed triads of (x,y,z) values */
    GLfloat *texCoords;     /* 'nVertices' packed pairs of (u,v) values */

    GLfloat minX, maxX;
    GLfloat minY, maxY;
    GLfloat minZ, maxZ;

    Uint16 maxDepth;
    Uint16 numNodes;
    Uint32 numTri;

    BSPTree *bspTree;

} BSPTreeData;


/* Function Prototypes */

/**
 * Generates a BSP Tree from the given textured triangles.
 * Inputs are the total number of triangles, (x,y,z) values of 
 * each of the vertices of the triangles in anticlockwise order, 
 * indices of the textures of the triangles, (u,v) texture map
 * coordinates at each of the vertices of the triangles in 
 * anticlockwise order, overall number of texture maps, 
 * null-terminated names of the texture maps, respectively (phew!).
 */
extern BSPTreeData *GenBSPTreeData( 
    Uint32 nTri, 
    GLfloat *triVerts,
    Uint16 *texIndices,
    GLfloat *triTexCoords,
    Uint16 nMaps, char **mapNames
);


/**
 * Saves the given BSP Tree and associated texture map information
 * into the given file. The file must be opened for writing binary
 * data, must have sufficient permissions and the disc must not be
 * full.
 */
extern void SaveBSPTreeData( BSPTreeData *bspData, FILE *outFile);


/**
 * Loads a BSP Tree and texture map information from the given file.
 * The file must have been opened for reading binary data, must
 * have sufficient permissions, etc.
 * 
 * Returns NULL on error.
 */
extern BSPTreeData *LoadBSPTreeData( FILE *inFile);


/**
 * Frees a BSP Tree and texture map information that has either been
 * loaded from a file or freshly generated.
 */
extern void FreeBSPTreeData( BSPTreeData *bspData);


/**
 * Classifies a given point as being below, on or above the
 * given plane.
 */
extern PointType ClassifyPoint( GLfloat aPt[], BSPPlane *partPlane);

#endif    /* _BSP_H */


