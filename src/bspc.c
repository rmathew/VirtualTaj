/* Copyright (c) 2001 Ranjit Mathew. All rights reserved.
 * Use of this source code is governed by the terms of the BSD licence
 * that can be found in the LICENCE file.
 */

/*
 * BSPC.C: A simple BSP tree compiler.
 * 
 * This is a straightforward implementation of the BSP tree
 * generation algorithm from the BSP Tree FAQ posted regularly
 * to comp.graphics.algorithms and available at:
 *
 *     ftp://ftp.sgi.com/other/bspfaq/faq/bspfaq.html
 *
 * It was modified a bit to handle textures, maintaining 
 * anticlockwise triangles in splits, etc.
 * 
 * The heuristic for selecting the "best" splitting root
 * triangle during a subdivision is due to Tom Hammersley
 * (tomh@globalnet.co.uk). Tom also mentions the use of
 * flags and preorder writing to remove the need for 
 * pointer <-> index conversions while loading/saving a BSP
 * tree from/to a file.
 *
 * Most of the 3D Math in this program is from the chapter
 * "Vectors and Analytic Geometry in Space", of the book
 * "Calculus and Analytic Geometry", 9th Edition, by Thomas 
 * and Finney, published by Pearson Education.
 */


#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include <limits.h>
#include <float.h>

#include "bsp.h"

/* The number of vertex definitions per block during refactoring */
#define DEFS_BLK_SIZE 200

/* The assumed thickness of a plane for coplanarity comparisons */
#define PLANE_THICKNESS 0.0005

/* Data types used locally */

typedef struct _bsp_tri_node
{
    /* Three vertices of a triangle */
    GLfloat V[3][3];

    /* Plane containing the triangle - MUST be separately computed
     * using the GetPlaneForTri( ) function.
     */
    BSPPlane plane;

    /* Texture map index and mappings at the three vertices */
    Uint16 tIndex;
    GLfloat T[3][2];

    struct _bsp_tri_node *prev;
    struct _bsp_tri_node *next;

} BSPTriNode;


typedef struct _int_bsp_tree_node
{
    /* Partition plane for the convex space rooted at this node */
    BSPPlane partition;

    /* All triangles coplanar with the partition plane */
    Uint16 numTri;
    BSPTriNode *triHead;

    struct _int_bsp_tree_node *back;
    struct _int_bsp_tree_node *front;

} IntBSPTreeNode;


typedef struct _vert_defs
{
    Uint16 nDefs;

    GLfloat V[DEFS_BLK_SIZE][3];
    GLfloat T[DEFS_BLK_SIZE][2];

    struct _vert_defs *next;

} VertDefs;


/* A nifty macro to print out a triangle */

#ifdef BSPC_DEBUG
    #define PRNT_BSPTRI( stream, tri) \
        fprintf( stream, \
	    "(%.6f,%.6f,%.6f), (%.6f,%.6f,%.6f), (%.6f,%.6f,%.6f)", \
	    tri->V[0][0], tri->V[0][1], tri->V[0][2], \
	    tri->V[1][0], tri->V[1][1], tri->V[1][2], \
	    tri->V[2][0], tri->V[2][1], tri->V[2][2] \
        )
#endif


/* Type of a triangle with respect to a partition plane */
typedef enum { IN_BACK = 0, SPANNING, COINCIDENT, IN_FRONT} TriType;


/* Internal function prototypes */

static void BuildBSPTree( IntBSPTreeNode *treeNode, BSPTriNode *triList);

static BSPTriNode *SelectNextRoot( BSPTriNode *triList, BSPTriNode **rootPtr);
static void SplitTri( 
    BSPTriNode *aTri, BSPPlane *p, BSPTriNode **fList, BSPTriNode **bList
);
static TriType ClassifyTri( BSPTriNode *aTri, BSPPlane *partPlane);
static GLdouble IntersectPlaneLineSeg( 
    BSPPlane *p, GLfloat v0[], GLfloat v1[], GLfloat res[]
);
static int GetPlaneForTri( GLfloat V[][3], BSPPlane *planePtr);

static void WriteBSPTree( BSPTree *root, FILE *outFile);
static BSPTree *ReadBSPTree( FILE *inFile, BSPTreeData *bspData);

static BSPTree *ConvIntBSPTree( IntBSPTreeNode *intTree);

static void FreeBSPTree( BSPTree *root);

static Uint16 GetVertDefIndex( GLfloat v[], GLfloat t[], GLfloat resV[]);

static BSPTriNode *AddTriToList( BSPTriNode *list, BSPTriNode *node);
static BSPTriNode *RemoveTriFromList( BSPTriNode *listHead, BSPTriNode *node);


/* Global data */

#ifdef BSPC_DEBUG
    static Uint32 numInputFaces = 0U;

    static Uint32 trianglesConverted = 0U;
    static Uint32 nodesConverted = 0U;

    static Uint32 nodesSaved = 0U;
    static Uint32 trianglesSaved = 0U;

    static Uint32 nodesLoaded = 0U;
    static Uint32 trianglesLoaded = 0U;
#endif

static const char *vertCodes = "BCF";

static Uint16 numVertDefs;
static VertDefs *vertDefsPtr;

static Uint32 *texCtrs;

static GLfloat minX, maxX, minY, maxY, minZ, maxZ;

static Uint16 nodesCreated = 0U;
static Uint32 trianglesCreated = 0U;
static Uint16 maxDepthSoFar;
static Uint16 currDepth;


/**
 * Generates BSP tree data from the given set of triangles and
 * texture maps.
 */
BSPTreeData *GenBSPTreeData( 
    Uint32 nTri, 
    GLfloat *triVerts,
    Uint16 *texIndices,
    GLfloat *triTexCoords,
    Uint16 nMaps, char **texMapNames
)
{
    BSPTreeData *retVal = NULL;
    IntBSPTreeNode *genBSPTree = NULL;
    BSPTriNode *triList = NULL;
    unsigned int i, j;

    
    retVal = (BSPTreeData *)( malloc( sizeof( BSPTreeData )));
    if( retVal == NULL)
    {
        fprintf( stderr, "\nFATAL ERROR: Out of Memory!\n");
	exit( EXIT_FAILURE);

    } /* End if */


    retVal->nMaps = nMaps;

    retVal->mapNames = (char **)( malloc( nMaps * sizeof( char *)));
    if( retVal->mapNames == NULL)
    {
        fprintf( stderr, "\nFATAL ERROR: Out of Memory!\n");
	exit( EXIT_FAILURE);

    } /* End if */

    texCtrs = (Uint32 *)( malloc( nMaps * sizeof( Uint32)));
    if( texCtrs == NULL)
    {
        fprintf( stderr, "\nFATAL ERROR: Out of Memory!\n");
	exit( EXIT_FAILURE);

    } /* End if */


    for( i = 0U; i < nMaps; i++)
    {
        retVal->mapNames[i] = 
	    (char *)( malloc( ( strlen( texMapNames[i]) + 1) * sizeof( char)));

        if( retVal->mapNames[i] == NULL)
	{
	    fprintf( stderr, "\nFATAL ERROR: Out of Memory!\n");
	    exit( EXIT_FAILURE);

	} /* End if */

	strcpy( retVal->mapNames[i], texMapNames[i]);

	texCtrs[i] = 0U;

    } /* End for */


#ifdef BSPC_DEBUG
    numInputFaces = 0U;
#endif

    /* Convert the input triangles into a list of BSPTriNode-s */
    for( i = 0U; i < nTri; i++)
    {
        BSPTriNode *tmpTri = (BSPTriNode *)( malloc( sizeof( BSPTriNode)));

	if( tmpTri == NULL)
	{
	    fprintf( stderr, "\nFATAL ERROR: Out of Memory!\n");
	    exit( EXIT_FAILURE);

	} /* End if */

	tmpTri->next = tmpTri->prev = NULL;

	tmpTri->tIndex = texIndices[i];

        for( j = 0U; j < 3U; j++)
	{
	    tmpTri->V[j][0] = triVerts[9*i + 3*j + 0];
	    tmpTri->V[j][1] = triVerts[9*i + 3*j + 1];
	    tmpTri->V[j][2] = triVerts[9*i + 3*j + 2];

	    tmpTri->T[j][0] = triTexCoords[6*i + 2*j + 0];
	    tmpTri->T[j][1] = triTexCoords[6*i + 2*j + 1];

	} /* End for */

	/* Check if this is a "proper" triangle */
	if( GetPlaneForTri( tmpTri->V, &( tmpTri->plane)) != 0)
	{
#ifdef BSPC_DEBUG
            fprintf( stderr, 
	        "WARNING: Skipping malformed triangle in input!\n"
            );
	    fprintf( stderr, "[Vertices: ");
	    PRNT_BSPTRI( stderr, tmpTri);
	    fprintf( stderr, "]\n\n");
#endif
            
	    /* Free the triangle definition */
	    free( tmpTri);

	} /* End if */
	else
	{
	    triList = AddTriToList( triList, tmpTri);

#ifdef BSPC_DEBUG
            numInputFaces++;
#endif

	} /* End else */

    } /* End for */

    genBSPTree = (IntBSPTreeNode *)( malloc( sizeof( IntBSPTreeNode)));
    if( genBSPTree == NULL)
    {
        fprintf( stderr, "\nFATAL ERROR: Out of Memory!\n");
	exit( EXIT_FAILURE);

    } /* End if */


    genBSPTree->numTri = 0U;
    genBSPTree->triHead = NULL;
    genBSPTree->front = genBSPTree->back = NULL;


#ifdef BSPC_DEBUG
    printf( 
        "BSPC: Compiling BSP tree from %u input triangles...\n",
	numInputFaces
    );
    printf( "BSPC: Nodes created so far: 0       ");
    fflush( stdout);
#endif


    /* Build the BSP tree */
    currDepth = 0U;
    maxDepthSoFar = 0U;
    nodesCreated = 0U;
    trianglesCreated = 0U;
    BuildBSPTree( genBSPTree, triList);


#ifdef BSPC_DEBUG
    printf( "\b\b\b\b\b\b\b\b%-8d\n", nodesCreated);
    printf( "BSPC: BSP tree successfully generated!\n");
    nodesConverted = 0U;
    trianglesConverted = 0U;
    printf( 
        "BSPC: Refactoring generated BSP tree: %3u%%", 
	( ( nodesConverted * 100U) / nodesCreated)
    );
    fflush( stdout);
#endif


    retVal->numNodes = nodesCreated;
    retVal->maxDepth = maxDepthSoFar;

    minX = minY = minZ = FLT_MAX;
    maxX = maxY = maxZ = FLT_MIN;

    numVertDefs = 0U;
    vertDefsPtr = NULL;


    /* Convert the internal BSP tree representation */
    retVal->bspTree = ConvIntBSPTree( genBSPTree);


    /* By now we should know the bounds of the model... */
    retVal->minX = minX;
    retVal->maxX = maxX;

    retVal->minY = minY;
    retVal->maxY = maxY;

    retVal->minZ = minZ;
    retVal->maxZ = maxZ;

    /* ...as well as how many triangles are mapped to each texture. */
    retVal->mapTriNums = texCtrs;
    texCtrs = NULL;

    /* ...and how many triangles we finally created */
    retVal->numTri = trianglesCreated;


    /* Get the vertex definitions */

    retVal->nVertices = numVertDefs;
    retVal->vertCoords = 
        (GLfloat *)( malloc( numVertDefs * 3 * sizeof( GLfloat)));
    retVal->texCoords = 
        (GLfloat *)( malloc( numVertDefs * 2 * sizeof( GLfloat)));

    if( ( retVal->vertCoords == NULL) || ( retVal->texCoords == NULL))
    {
        fprintf( stderr, "\nFATAL ERROR: Out of Memory!\n");
	exit( EXIT_FAILURE);

    } /* End if */
    else
    {
	VertDefs *cvPtr, *pvPtr;

	cvPtr = vertDefsPtr;
	i = 0U;
	while( cvPtr != NULL)
	{
	    for( j = 0U; j < cvPtr->nDefs; j++)
	    {
		retVal->vertCoords[3*i + 0] = cvPtr->V[j][0];
		retVal->vertCoords[3*i + 1] = cvPtr->V[j][1];
		retVal->vertCoords[3*i + 2] = cvPtr->V[j][2];

		retVal->texCoords[2*i + 0] = cvPtr->T[j][0];
		retVal->texCoords[2*i + 1] = cvPtr->T[j][1];

		i++;

	    } /* End for */

	    pvPtr = cvPtr;
	    cvPtr = cvPtr->next;
	    free( pvPtr);

	} /* End while */

#ifdef BSPC_DEBUG
        /* Sanity check */
        if( i != numVertDefs)
	{
	    fprintf( 
	        stderr,
	        "ERROR: Invalid numVertDefs in GenBSPTreeData( )!\n"
	    );
	    exit( EXIT_FAILURE);

	} /* End if */
#endif

    } /* End else */


#ifdef BSPC_DEBUG 
    printf( "\b\b\b\b%3u%%\n", ( nodesConverted * 100U) / nodesCreated);
    printf( 
        "(Final: %u triangles, %u vertex definitions)\n",
        trianglesConverted, numVertDefs
    );
    fflush( stdout);
#endif

    return retVal;

} /* End function GenBSPTreeData */


/**
 * Builds a BSP tree starting at the given node, using the
 * given list of triangular faces.
 */
void BuildBSPTree( IntBSPTreeNode *treeNode, BSPTriNode *triList)
{
    BSPTriNode *rootTri;
    BSPTriNode *restOfList;
    BSPTriNode *frontList = NULL;
    BSPTriNode *backList = NULL;


    nodesCreated++;

    currDepth++;
    if( currDepth > maxDepthSoFar)
    {
        maxDepthSoFar = currDepth;

    } /* End if */


#ifdef BSPC_DEBUG
    printf( "\b\b\b\b\b\b\b\b%-8d", nodesCreated);
    fflush( stdout);
#endif

    /* Pick up the root triangle for partitioning this subspace */
    restOfList = SelectNextRoot( triList, &rootTri);

    treeNode->partition.A = rootTri->plane.A;
    treeNode->partition.B = rootTri->plane.B;
    treeNode->partition.C = rootTri->plane.C;
    treeNode->partition.D = rootTri->plane.D;

    /* Start the node's coplanar list with the root triangle */
    treeNode->triHead = rootTri;
    treeNode->numTri = 1U;

    /* Process the remaining triangles */
    while( restOfList != NULL)
    {
        BSPTriNode *aTri;
        TriType triKind;
	BSPTriNode *fSplitList, *bSplitList;

        /* Pick up and remove the head of the list */
        aTri = restOfList;
	restOfList = RemoveTriFromList( restOfList, aTri);

	triKind = ClassifyTri( aTri, &( treeNode->partition));
	switch( triKind)
	{
	case COINCIDENT:
	    treeNode->triHead = AddTriToList( treeNode->triHead, aTri);
	    treeNode->numTri++;
	    break;

	case IN_FRONT:
	    frontList = AddTriToList( frontList, aTri);
	    break;

	case IN_BACK:
	    backList = AddTriToList( backList, aTri);
	    break;

	case SPANNING:
	    fSplitList = bSplitList = NULL;
	    SplitTri( 
	        aTri, &( treeNode->partition), &fSplitList, &bSplitList
	    );

	    /* The triangle might have been split into two or three other
	     * triangles - however, on each side (front/back), only up to
	     * two triangles can be there.
	     */

	    if( fSplitList != NULL)
	    {
	        if( fSplitList->next != NULL)
		{
		    frontList = AddTriToList( frontList, fSplitList->next);

		} /* End if */

		frontList = AddTriToList( frontList, fSplitList);

	    } /* End if */


	    if( bSplitList != NULL)
	    {
	        if( bSplitList->next != NULL)
		{
		    backList = AddTriToList( backList, bSplitList->next);

		} /* End if */

		backList = AddTriToList( backList, bSplitList);

	    } /* End if */


	    /* The original triangle can be discarded */
	    aTri->next = aTri->prev = NULL;
	    free( aTri);
	    break;

        default:
#ifdef BSPC_DEBUG
            fprintf(
	        stderr,
		"BSPC: SNAFU! Invalid TriType in BuildBSPTree( )\n"
	    );
	    exit( EXIT_FAILURE);
#endif
	    break;

	} /* End switch */

    } /* End of while */


    if( frontList != NULL)
    {
        treeNode->front = (IntBSPTreeNode *)( malloc( sizeof( IntBSPTreeNode)));
	if( treeNode->front == NULL)
	{
	    fprintf( stderr, "\nFATAL ERROR: Out of Memory!\n");
	    exit( EXIT_FAILURE);

	} /* End if */


	treeNode->front->numTri = 0;
	treeNode->front->triHead = NULL;
	treeNode->front->front = NULL;
	treeNode->front->back = NULL;

	BuildBSPTree( treeNode->front, frontList);

    } /* End if */


    if( backList != NULL)
    {
        treeNode->back = (IntBSPTreeNode *)( malloc( sizeof( IntBSPTreeNode)));
	if( treeNode->back == NULL)
	{
	    fprintf( stderr, "\nFATAL ERROR: Out of Memory!\n");
	    exit( EXIT_FAILURE);

	} /* End if */


	treeNode->back->numTri = 0;
	treeNode->back->triHead = NULL;
	treeNode->back->front = NULL;
	treeNode->back->back = NULL;

	BuildBSPTree( treeNode->back, backList);

    } /* End if */


    currDepth--;

} /* End function BuildBSPTree */


/**
 * Selects the next root node from the given list.
 * This is a node that causes as few splits as possible
 * while keeping the tree balanced. This is an O(N^2)
 * method and is VERY expensive.
 *
 * Removes the selected node and returns the rest of 
 * the list.
 */
BSPTriNode *SelectNextRoot( BSPTriNode *triList, BSPTriNode **rootPtr)
{
    unsigned int minScore;
    BSPTriNode *bestNode;

    BSPTriNode *currNode;

    BSPTriNode *retVal;

    minScore = UINT_MAX;
    bestNode = NULL;

    currNode = triList;
    while( currNode != NULL)
    {
        BSPTriNode *testNode;
	unsigned int score;
	unsigned int splits, inFront, inBack, onPlane;

        splits = 0U;
	inFront = inBack = onPlane = 0U;
	testNode = triList;

	while( testNode != NULL)
	{
	    TriType triType = ClassifyTri( testNode, &( currNode->plane));

	    if( testNode != currNode)
	    {
		switch( triType)
		{
		case SPANNING:
		    splits++;
		    break;

		case IN_FRONT:
		    inFront++;
		    break;

		case IN_BACK:
		    inBack++;
		    break;

		case COINCIDENT:
		    onPlane++;
		    break;

		} /* End switch */

	    } /* End if */
#ifdef BSPC_DEBUG 
	    else if( triType != COINCIDENT)
	    {
	        GLfloat res1, res2, res3;
	        PointType vt[3];
		char triType[4];

		vt[0] = ClassifyPoint( testNode->V[0], &( currNode->plane));
		vt[1] = ClassifyPoint( testNode->V[1], &( currNode->plane));
		vt[2] = ClassifyPoint( testNode->V[2], &( currNode->plane));

		triType[0] = vertCodes[vt[0]];
		triType[1] = vertCodes[vt[1]];
		triType[2] = vertCodes[vt[2]];
		triType[3] = '\0';

                res1 = currNode->plane.D +
		    (currNode->plane.A * testNode->V[0][0]) + 
		    (currNode->plane.B * testNode->V[0][1]) + 
		    (currNode->plane.C * testNode->V[0][2]);

                res2 = currNode->plane.D +
		    (currNode->plane.A * testNode->V[1][0]) + 
		    (currNode->plane.B * testNode->V[1][1]) + 
		    (currNode->plane.C * testNode->V[1][2]);

                res3 = currNode->plane.D +
		    (currNode->plane.A * testNode->V[2][0]) + 
		    (currNode->plane.B * testNode->V[2][1]) + 
		    (currNode->plane.C * testNode->V[2][2]);

	        /* We are testing the candidate triangle against itself */
		fprintf( stderr,
		    "\nERROR: Triangle MUST be coplanar with its own plane!\n"
		);
		fprintf( stderr,
		    "(Triangle type: \"%s\", Plane Eqn Results: %f,%f,%f)\n", 
		    triType, res1, res2, res3
		);
		fprintf( stderr,
		    "(Plane: %.3fx + %.3fy + %.3fz + %.3f = 0)\n",
		    currNode->plane.A, currNode->plane.B, 
		    currNode->plane.C, currNode->plane.D
		);

		exit( EXIT_FAILURE);

	    } /* End else-if */
#endif

	    testNode = testNode->next;

	} /* End while */

        /* MinSplits and Balance have equal priority */
	score = splits + (unsigned int)( abs( inFront - inBack));

	if( score < minScore)
	{
	    minScore = score;
	    bestNode = currNode;

	} /* End if */


	/* Early exit: We have found a triangle that causes no splits
	 * and creates a perfectly balanced tree!
	 */
	if( score == 0U)
	{
	    break;

	} /* End if */

        currNode = currNode->next;

    } /* End while */

#ifdef BSPC_DEBUG
    if( bestNode == NULL)
    {
        /* We couldn't find any decent node - how can this be? */
	fprintf( stderr,
	    "\nERROR: SelectNextRoot( ) couldn't find a good node!\n"
	);
	exit( EXIT_FAILURE);

    } /* End if */
#endif

    (*rootPtr) = bestNode;
    retVal = RemoveTriFromList( triList, bestNode);

    return retVal;

} /* End function SelectNextRoot */


/**
 * Splits a spanning triangle with respect to the given
 * plane into front and back triangles. Assumes that the
 * original vertices were given in anticlockwise order
 * and ensures the same for the split triangles.
 * 
 * Traverses the edges of the triangle in anticlockwise
 * order and maintains lists of vertices in front and back of 
 * the partitioning plane. A spanning edge is split into 
 * two parts, with the intersection point being added to
 * both the lists.
 *
 * The lists are then used to create the respective new triangles.
 * Note that only a maximum of two edges of a triangle can
 * be intersected by a non-coincident plane.
 */
void SplitTri( 
    BSPTriNode *aTri, BSPPlane *partnPlane, 
    BSPTriNode **fList, BSPTriNode **bList
)
{
    PointType vertTypes[3];
    unsigned int numFrontVerts, numBackVerts;

    GLfloat frontVerts[4][3];
    GLfloat frontTexCoords[4][2];

    GLfloat backVerts[4][3];
    GLfloat backTexCoords[4][2];

    char triSplitCode[4];

    unsigned int i, next1;

    numFrontVerts = numBackVerts = 0U;

    vertTypes[0] = ClassifyPoint( aTri->V[0], partnPlane);
    vertTypes[1] = ClassifyPoint( aTri->V[1], partnPlane);
    vertTypes[2] = ClassifyPoint( aTri->V[2], partnPlane);

    /* Construct "split code" for triangle - "BCF", "FFC", etc. */
    triSplitCode[0] = vertCodes[ vertTypes[0]];
    triSplitCode[1] = vertCodes[ vertTypes[1]];
    triSplitCode[2] = vertCodes[ vertTypes[2]];
    triSplitCode[3] = '\0';

    /* Traverse the triangle assuming anticlockwise order */
    for( i = 0U; i < 3U; i++)
    {
        /* Put the current vertex in its place */
        switch( vertTypes[i])
	{
	case ABOVE_PLANE:
	    frontVerts[ numFrontVerts][0] = aTri->V[i][0];
	    frontVerts[ numFrontVerts][1] = aTri->V[i][1];
	    frontVerts[ numFrontVerts][2] = aTri->V[i][2];

	    frontTexCoords[ numFrontVerts][0] = aTri->T[i][0];
	    frontTexCoords[ numFrontVerts][1] = aTri->T[i][1];

	    numFrontVerts++;
	    break;

	case BELOW_PLANE:
	    backVerts[ numBackVerts][0] = aTri->V[i][0];
	    backVerts[ numBackVerts][1] = aTri->V[i][1];
	    backVerts[ numBackVerts][2] = aTri->V[i][2];

	    backTexCoords[ numBackVerts][0] = aTri->T[i][0];
	    backTexCoords[ numBackVerts][1] = aTri->T[i][1];

	    numBackVerts++;
	    break;

	case ON_PLANE:
	    /* These vertices can form a part of both front and 
	     * back triangles 
	     */

	    frontVerts[ numFrontVerts][0] = aTri->V[i][0];
	    frontVerts[ numFrontVerts][1] = aTri->V[i][1];
	    frontVerts[ numFrontVerts][2] = aTri->V[i][2];

	    frontTexCoords[ numFrontVerts][0] = aTri->T[i][0];
	    frontTexCoords[ numFrontVerts][1] = aTri->T[i][1];

	    numFrontVerts++;

	    backVerts[ numBackVerts][0] = aTri->V[i][0];
	    backVerts[ numBackVerts][1] = aTri->V[i][1];
	    backVerts[ numBackVerts][2] = aTri->V[i][2];

	    backTexCoords[ numBackVerts][0] = aTri->T[i][0];
	    backTexCoords[ numBackVerts][1] = aTri->T[i][1];

	    numBackVerts++;

	    break;

	} /* End switch */

	/* Does the next vertex fall on the other side of the plane? */
	next1 = ( (i+1) % 3U);
	if( (( vertTypes[i] == ABOVE_PLANE) && 
	    ( vertTypes[next1] == BELOW_PLANE)) ||
	    (( vertTypes[i] == BELOW_PLANE) && 
	    ( vertTypes[next1] == ABOVE_PLANE))
        )
	{
	    GLdouble tcDiff[2];

	    /* Find the intersection point of the plane and this edge */

	    /* This intersection point is trivially coincident with
	     * the plane.
	     */
	    GLdouble t = IntersectPlaneLineSeg( 
	        partnPlane, aTri->V[i], aTri->V[ next1], 
		backVerts[ numBackVerts]
            );

	    /* Suitably interpolate texture coordinates */

            tcDiff[0] = ( 
	        (GLdouble )( aTri->T[next1][0]) - 
		(GLdouble )( aTri->T[i][0])
	    );
            tcDiff[1] = ( 
	        (GLdouble )( aTri->T[next1][1]) - 
		(GLdouble )( aTri->T[i][1])
	    );

	    backTexCoords[ numBackVerts][0] =
	        (GLfloat )( (GLdouble )( aTri->T[i][0]) + t * tcDiff[0]);

	    backTexCoords[ numBackVerts][1] =
	        (GLfloat )( (GLdouble )( aTri->T[i][1]) + t * tcDiff[1]);

	    frontVerts[ numFrontVerts][0] = backVerts[ numBackVerts][0];
	    frontVerts[ numFrontVerts][1] = backVerts[ numBackVerts][1];
	    frontVerts[ numFrontVerts][2] = backVerts[ numBackVerts][2];

	    frontTexCoords[ numFrontVerts][0] = 
	        backTexCoords[ numBackVerts][0];

	    frontTexCoords[ numFrontVerts][1] = 
	        backTexCoords[ numBackVerts][1];

	    numBackVerts++;
	    numFrontVerts++;

	} /* End if */

    } /* End for */


    /* If this method was invoked with the correct data (actual
     * spanning triangle for the plane), we now MUST have at least 
     * three front/back vertices.
     *
     * And if this method has been behaving sanely, we MUST NOT
     * have more than four front/back vertices. 
     */
    if( ( numBackVerts < 3U) || ( numFrontVerts < 3U))
    {
        fprintf( stderr,
	    "\nERROR: SplitTri( ) asked to split non-spanning triangle!\n"
	);
	exit( EXIT_FAILURE);

    } /* End if */
    else if( ( numBackVerts > 4U) || (numFrontVerts > 4U))
    {
        fprintf( stderr,
	    "\nERROR: SplitTri( ) SNAFU: %u front, %u back vertices\n",
	    numFrontVerts, numBackVerts
	);
	exit( EXIT_FAILURE);

    } /* End else-if */


    /* Make triangles from vertices in front of the plane */

    *fList = (BSPTriNode *)( malloc( sizeof( BSPTriNode)));
    if( *fList == NULL)
    {
        fprintf( stderr, "\nFATAL ERROR: Out of Memory!\n");
	exit( EXIT_FAILURE);

    } /* End if */


    (*fList)->next = (*fList)->prev = NULL;
    (*fList)->tIndex = aTri->tIndex;

    for( i = 0U; i < 3U; i++)
    {
        (*fList)->V[i][0] = frontVerts[i][0];
        (*fList)->V[i][1] = frontVerts[i][1];
        (*fList)->V[i][2] = frontVerts[i][2];

        (*fList)->T[i][0] = frontTexCoords[i][0];
        (*fList)->T[i][1] = frontTexCoords[i][1];

    } /* End for */

    if( GetPlaneForTri( (*fList)->V, &( (*fList)->plane)) != 0)
    {
        /* We have created a degenerate triangle - discard it */
	free( *fList);
	*fList = NULL;

    } /* End if */


    if( numFrontVerts == 4U)
    {
	BSPTriNode *tmpTri = (BSPTriNode *)( malloc( sizeof( BSPTriNode)));
	if( tmpTri == NULL)
	{
	    fprintf( stderr, "\nFATAL ERROR: Out of Memory!\n");
	    exit( EXIT_FAILURE);

	} /* End if */
	

        tmpTri->tIndex = aTri->tIndex;

	for( i = 2U; i < 5U; i++)
	{
	    tmpTri->V[i-2][0] = frontVerts[i%4U][0];
	    tmpTri->V[i-2][1] = frontVerts[i%4U][1];
	    tmpTri->V[i-2][2] = frontVerts[i%4U][2];

	    tmpTri->T[i-2][0] = frontTexCoords[i%4U][0];
	    tmpTri->T[i-2][1] = frontTexCoords[i%4U][1];

	} /* End for */

	if( GetPlaneForTri( tmpTri->V, &( tmpTri->plane)) != 0)
	{
	    /* We have created a degenerate triangle - discard it */
	    free( tmpTri);

	} /* End if */
	else
	{
	    *fList = AddTriToList( *fList, tmpTri);

	} /* End else */

    } /* End if */


    /* Make triangles from vertices in back of the plane */

    *bList = (BSPTriNode *)( malloc( sizeof( BSPTriNode)));
    if( *bList == NULL)
    {
        fprintf( stderr, "\nFATAL ERROR: Out of Memory!\n");
	exit( EXIT_FAILURE);

    } /* End if */


    (*bList)->next = (*bList)->prev = NULL;
    (*bList)->tIndex = aTri->tIndex;

    for( i = 0U; i < 3U; i++)
    {
        (*bList)->V[i][0] = backVerts[i][0];
        (*bList)->V[i][1] = backVerts[i][1];
        (*bList)->V[i][2] = backVerts[i][2];

        (*bList)->T[i][0] = backTexCoords[i][0];
        (*bList)->T[i][1] = backTexCoords[i][1];

    } /* End for */

    if( GetPlaneForTri( (*bList)->V, &( (*bList)->plane)) != 0)
    {
        /* We have created a degenerate triangle - discard it */
	free( *bList);
	*bList = NULL;

    } /* End if */


    if( numBackVerts == 4U)
    {
	BSPTriNode *tmpTri = (BSPTriNode *)( malloc( sizeof( BSPTriNode)));
	if( tmpTri == NULL)
	{
	    fprintf( stderr, "\nFATAL ERROR: Out of Memory!\n");
	    exit( EXIT_FAILURE);

	} /* End if */
	

        tmpTri->tIndex = aTri->tIndex;

	for( i = 2U; i < 5U; i++)
	{
	    tmpTri->V[i-2][0] = backVerts[i%4U][0];
	    tmpTri->V[i-2][1] = backVerts[i%4U][1];
	    tmpTri->V[i-2][2] = backVerts[i%4U][2];

	    tmpTri->T[i-2][0] = backTexCoords[i%4U][0];
	    tmpTri->T[i-2][1] = backTexCoords[i%4U][1];

	} /* End for */

	if( GetPlaneForTri( tmpTri->V, &( tmpTri->plane)) != 0)
	{
	    /* We have created a degenerate triangle - discard it */
	    free( tmpTri);

	} /* End if */
	else
	{
	    *bList = AddTriToList( *bList, tmpTri);

	} /* End else */

    } /* End if */

} /* End function SplitTri */


/**
 * Constructs the equation of the plane containing
 * the given triangle with points A, B and C. Returns
 * '0' if successful, '-1' if the triangle is a
 * degenerate triangle.
 *
 * IMPORTANT: Assumes points are given in anticlockwise 
 * order with respect to the front of the triangle, to
 * be able to correctly calculate the normal.
 *
 * Find the cross product of AB and AC, which gives 
 * the normal vector at point A. The normal's X, Y 
 * and Z coordinate values give the respective values 
 * for 'A', 'B' and 'C' in the plane equation
 * "Ax + By + Cz + D = 0", while 'D' can be calculated
 * by substituting for the point A in this equation.
 *
 * The normal is scaled to be a unit vector, which can
 * help a lot in calculations.
 */
int GetPlaneForTri( GLfloat V[][3], BSPPlane *planePtr)
{
    GLdouble AB[3], AC[3], Normal[3];
    GLdouble normMag;
    int retVal;

    retVal = -1;

    /* Vector AB = B - A */
    AB[0] = (GLdouble )( V[1][0]) - (GLdouble )( V[0][0]);
    AB[1] = (GLdouble )( V[1][1]) - (GLdouble )( V[0][1]);
    AB[2] = (GLdouble )( V[1][2]) - (GLdouble )( V[0][2]);

    /* Vector AC = C - A */
    AC[0] = (GLdouble )( V[2][0]) - (GLdouble )( V[0][0]);
    AC[1] = (GLdouble )( V[2][1]) - (GLdouble )( V[0][1]);
    AC[2] = (GLdouble )( V[2][2]) - (GLdouble )( V[0][2]);

    /* Normal = Cross Product ABxAC
     *        = Value of the determinant:
     *              |                       |
     *              |   i       j       k   |
     *              |                       |
     *              | AB[0]   AB[1]   AB[2] |
     *              |                       |
     *              | AC[0]   AC[1]   AC[2] |
     *              |                       |
     */
    Normal[0] = AB[1]*AC[2] - AB[2]*AC[1];
    Normal[1] = AB[2]*AC[0] - AB[0]*AC[2];
    Normal[2] = AB[0]*AC[1] - AB[1]*AC[0];

    /* Scale the normal to be a unit vector */
    normMag = 0.0;
    normMag += ( Normal[0] * Normal[0]);
    normMag += ( Normal[1] * Normal[1]);
    normMag += ( Normal[2] * Normal[2]);

    normMag = sqrt( normMag);

    if( normMag > DBL_EPSILON)
    {
	Normal[0] /= normMag;
	Normal[1] /= normMag;
	Normal[2] /= normMag;

	/* For any point P(x,y,z) on the plane, the dot product 
	 * AP.Normal is zero, including the special case of P = A.
	 *
	 * The following therefore, holds true:
	 */

	planePtr->A = Normal[0];
	planePtr->B = Normal[1];
	planePtr->C = Normal[2];

	planePtr->D = 0.0 - 
	    ( Normal[0]*( (GLdouble )( V[0][0]))) - 
	    ( Normal[1]*( (GLdouble )( V[0][1]))) - 
	    ( Normal[2]*( (GLdouble )( V[0][2])));

        retVal = 0;

    } /* End if */
    else
    {
        /* The vertices are almost collinear and this is too needle-like
	 * a triangle for our comfort.
	 */
        retVal = -1;

    } /* End else */

    return retVal;

} /* End function GetPlaneForTri */


/**
 * Classifies a triangle with respect to the given partition
 * plane.
 */
TriType ClassifyTri( BSPTriNode *aTri, BSPPlane *partPlane)
{
    unsigned int i;
    unsigned int vOnPlane, vAbove, vBelow;
    TriType retVal;

    vOnPlane = vAbove = vBelow = 0U;

    for( i = 0U; i < 3U; i++)
    {
        PointType vType = ClassifyPoint( aTri->V[i], partPlane);
        
	switch( vType)
	{
	case ON_PLANE:
	    vOnPlane++;
	    break;

	case ABOVE_PLANE:
	    vAbove++;
	    break;

	case BELOW_PLANE:
	    vBelow++;
	    break;

	} /* End switch */

    } /* End for */


    if( vOnPlane == 3U)
    {
        retVal = COINCIDENT;

    } /* End if */
    else if( ( vAbove + vOnPlane) == 3U)
    {
        retVal = IN_FRONT;

    } /* End else-if */
    else if( ( vBelow + vOnPlane) == 3U)
    {
        retVal = IN_BACK;

    } /* End else-if */
    else
    {
        retVal = SPANNING;

    } /* End else */
    
    return retVal;

} /* End function ClassifyTri */


/**
 * Finds out if a given point is below, on, or above,
 * the given plane.
 * Direction is determined by the normal of the plane.
 */
PointType ClassifyPoint( GLfloat aPt[], BSPPlane *partPlane)
{
    PointType retVal;

    /* Substitute the point in the LHS of the plane equation 
     * Ax + By + Cz + D = 0 and get the distance along the 
     * plane normal. This is valid since our normals are unit 
     * vectors - otherwise the result would merely have been 
     * proportional to the distance.
     */
    GLdouble vDist = 
	( partPlane->A * (GLdouble )( aPt[0])) +
	( partPlane->B * (GLdouble )( aPt[1])) +
	( partPlane->C * (GLdouble )( aPt[2])) +
	partPlane->D;


    /* Due to roundoff errors during various computations, we can't
     * be too sure of the accuracy of the distance calculated above.
     * The recourse is to make the plane a bit "thick" and test the
     * point against it.
     */
    if( fabs( vDist) <= PLANE_THICKNESS)
    {
	/* Vertex is coincident with the plane */
	retVal = ON_PLANE;

    } /* End if */
    else if( vDist > PLANE_THICKNESS)
    {
	/* Vertex is above the plane */
	retVal = ABOVE_PLANE;

    } /* End else-if */
    else
    {
	/* Vertex is below the plane */
	retVal = BELOW_PLANE;

    } /* End else */

    return retVal;

} /* End function ClassifyPoint */


/**
 * Intersects the given plane with the given line segment and stores
 * the result in the given array. Returns 't' such that the point
 * of intersection 'P' = V0 + t*(V1-V0)
 *
 * WARNING: The result is undefined if the line DOES NOT intersect
 * the plane.
 */
GLdouble IntersectPlaneLineSeg( 
    BSPPlane *plane, GLfloat v0[], GLfloat v1[], GLfloat res[]
)
{
    GLdouble t = DBL_MAX;
    GLdouble lSeg[3];
    GLdouble epsilon;
    GLdouble numer, denom;


    /* Compute the differences between the coordinates of the vertices */
    lSeg[0] = (GLdouble )( v1[0]) - (GLdouble )( v0[0]);
    lSeg[1] = (GLdouble )( v1[1]) - (GLdouble )( v0[1]);
    lSeg[2] = (GLdouble )( v1[2]) - (GLdouble )( v0[2]);


    /* Use the parametric form for a line from V0 to V1:
     *
     *     V = V0 + t*(V1-V0) 
     *
     * Plug this into the plane equation "Ax + By + Cz + D = 0"
     * to find the value of 't':
     *
     *         - ( A*x0 + B*y0 + C*z0 + D)
     * t = --------------------------------------
     *      ( A*(x1-x0) + B*(y1-y0) + C*(z1-z0))
     *
     * The line intersects the plane if the denominator is greater 
     * than zero, otherwise is parallel to, or on, the plane.
     */

    denom = 
        ( plane->A * lSeg[0]) + 
	( plane->B * lSeg[1]) + 
	( plane->C * lSeg[2]);


    /* Scale floating-point epsilon for comparison */
    epsilon = 
        fabs( ( ( plane->A + (GLdouble )( v1[2])) * DBL_EPSILON) / 2.0);

    if( fabs( denom) > epsilon)
    {
	numer = ( plane->A * (GLdouble )( v0[0])) + 
	        ( plane->B * (GLdouble )( v0[1])) + 
	        ( plane->C * (GLdouble )( v0[2])) + 
		plane->D;

        numer *= -1.0;

	t = numer / denom;

	res[0] = (GLfloat )( (GLdouble )( v0[0]) + t * lSeg[0]);
	res[1] = (GLfloat )( (GLdouble )( v0[1]) + t * lSeg[1]);
	res[2] = (GLfloat )( (GLdouble )( v0[2]) + t * lSeg[2]);

    } /* End if */
#ifdef BSPC_DEBUG
    else
    {
        fprintf(
	    stderr,
	    "\nERROR: No intersection for parallel line/plane!\n"
	);
	exit( EXIT_FAILURE);
    } /* End else */
#endif

    return t;

} /* End function IntersectPlaneLineSeg */


/**
 * Adds the given node to the given list (can be empty).
 * Returns the new list starting from the given node.
 */
BSPTriNode *AddTriToList( BSPTriNode *list, BSPTriNode *node)
{
    BSPTriNode *retVal;

    retVal = node;

    if( node != NULL)
    {
	node->next = list;
	node->prev = NULL;

	if( list != NULL)
	{
	    list->prev = node;

	} /* End if */

    } /* End if */
    else
    {
#ifdef BSPC_DEBUG
        fprintf( 
	    stderr, 
	    "BSPC: SNAFU! Node NULL in AddTriToList( )\n"
        );
	exit( EXIT_FAILURE);
#endif

    } /* End else */

    return retVal;

} /* End function AddTriToList */


/**
 * Removes the given node from the given list and returns
 * the modified list.
 */
BSPTriNode *RemoveTriFromList( BSPTriNode *listHead, BSPTriNode *node)
{
    BSPTriNode *retVal;

    retVal = listHead;

    if( ( node != NULL) && ( listHead != NULL))
    {
        if( listHead == node)
	{
	    retVal = listHead->next;

	} /* End if */

	if( node->prev != NULL)
	{
	    node->prev->next = node->next;

	} /* End if */

	if( node->next != NULL)
	{
	    node->next->prev = node->prev;

	} /* End if */

	node->next = node->prev = NULL;

    } /* End if */
    else
    {
#ifdef BSPC_DEBUG
        fprintf( 
	    stderr, 
	    "BSPC: SNAFU! List or node NULL in RemoveTriFromList( )\n"
        );
	exit( EXIT_FAILURE);
#endif

    } /* End else */

    return retVal;

} /* End function RemoveTriFromList */


/**
 * Saves the given BSP tree to the given file.
 */
void SaveBSPTreeData( BSPTreeData *bspData, FILE *outFile)
{
    if( bspData != NULL)
    {
        unsigned int i;
	Uint8 bspDataVer = BSP_DATA_VER;

        /* Write out a small signature */
	fwrite( 
	    BSP_FILE_MAGIC, 
	    sizeof( char), ( strlen( BSP_FILE_MAGIC) + 1),
	    outFile
        );

	/* Write out BSP data version number */
	fwrite( 
	    &bspDataVer, 
	    sizeof( bspDataVer), 1,
	    outFile
        );

	/* Write out the texture maps information */
	fwrite( 
	    &( bspData->nMaps), 
	    sizeof( bspData->nMaps), 1,
	    outFile
        );

	for( i = 0U; i < bspData->nMaps; i++)
	{
	    fwrite( 
		bspData->mapNames[i], 
		sizeof( char), ( strlen( bspData->mapNames[i]) + 1),
		outFile
	    );

	} /* End for */

	fwrite( 
	    bspData->mapTriNums, 
	    sizeof( bspData->mapTriNums[0]), bspData->nMaps, 
	    outFile
	);

	
	/* Write out the vertex definitions */
	fwrite(
	    &( bspData->nVertices),
	    sizeof( bspData->nVertices), 1,
	    outFile
	);

	fwrite(
	    bspData->vertCoords,
	    sizeof( GLfloat), ( 3 * bspData->nVertices),
	    outFile
	);

	fwrite(
	    bspData->texCoords,
	    sizeof( GLfloat), ( 2 * bspData->nVertices),
	    outFile
	);


	/* Write out the model bounds */
	fwrite( &( bspData->minX), sizeof( GLfloat), 1, outFile);
	fwrite( &( bspData->maxX), sizeof( GLfloat), 1, outFile);

	fwrite( &( bspData->minY), sizeof( GLfloat), 1, outFile);
	fwrite( &( bspData->maxY), sizeof( GLfloat), 1, outFile);

	fwrite( &( bspData->minZ), sizeof( GLfloat), 1, outFile);
	fwrite( &( bspData->maxZ), sizeof( GLfloat), 1, outFile);

        
	/* Write out some information about the BSP tree */
	fwrite( &( bspData->maxDepth), sizeof( bspData->maxDepth), 1, outFile);
	fwrite( &( bspData->numNodes), sizeof( bspData->numNodes), 1, outFile);
	fwrite( &( bspData->numTri), sizeof( bspData->numTri), 1, outFile);


	/* Finally, write out the actual BSP tree itself */
	WriteBSPTree( bspData->bspTree, outFile);

	/* Just to be sure */
	fflush( outFile);

#ifdef BSPC_DEBUG
	printf( 
	    "BSPC: Saved %u triangles spread over %u nodes (%hu levels).\n",
	    trianglesSaved, nodesSaved, bspData->maxDepth
	);
	fflush( stdout);
#endif

    } /* End if */

} /* End function SaveBSPTreeData */


/**
 * Writes out the given BSP tree to the given file in preorder.
 */
void WriteBSPTree( BSPTree *root, FILE *outFile)
{
    if( root != NULL)
    {
	Uint8 cFlag;
	Uint16 i;

        fwrite( &( root->numTri), sizeof( root->numTri), 1, outFile);

        for( i = 0U; i < root->numTri; i++)
	{
	    BSPTriFace *triFace = (root->triDefs + i);

	    fwrite( 
	        &( triFace->texIndex), 
		sizeof( triFace->texIndex), 1, 
		outFile
	    );
	    fwrite( 
	        triFace->vIndices, 
		sizeof( triFace->vIndices[0]), 3, 
		outFile
	    );

	} /* End for */

        /* Need to write the partition plane only if there are no
	 * triangles left in this node.
	 */
        if( root->numTri == 0U)
	{
	    fwrite( 
	        &( root->partPlane), 
		sizeof( root->partPlane), 1, 
		outFile
	    );

	} /* End if */

        /* Write out the flags that indicate presence of front/back 
	 * child trees.
	 */
        cFlag = 0x00;

	if( root->back != NULL)
	{
	    cFlag |= 0xB0;

	} /* End if */

	if( root->front != NULL)
	{
	    cFlag |= 0x0F;

	} /* End if */


#ifdef BSPC_DEBUG
        /* Sanity check */
        if( ( cFlag != 0x00) && 
	    ( cFlag != 0xB0) && 
	    ( cFlag != 0x0F) && 
	    ( cFlag != 0xBF)
        )
	{
	    fprintf( stderr, "\nERROR: WriteBSPTree( ) has gone bonkers!\n");
	    fprintf( stderr, "(cFlag = %2x)\n", cFlag);
	    exit( EXIT_FAILURE);

	} /* End if */
#endif

        fwrite( &cFlag, sizeof( Uint8), 1, outFile);


	if( root->back != NULL)
	{
	    WriteBSPTree( root->back, outFile);

	} /* End if */

	if( root->front != NULL)
	{
	    WriteBSPTree( root->front, outFile);

	} /* End if */


#ifdef BSPC_DEBUG
        nodesSaved++;
	trianglesSaved += root->numTri;
#endif

    } /* End if */

} /* End function WriteBSPTree */


/**
 * Loads BSP Tree data from the given file. This file must have been
 * written out by a corresponding call to the SaveBSPTreeData( )
 * function elsewhere on the same platform.
 */
BSPTreeData *LoadBSPTreeData( FILE *inFile)
{
    BSPTreeData *retVal = NULL;

    if( inFile != NULL)
    {
        unsigned int sigSize;
	Uint8 bspDataVer = 0U;
        char *savedSig = NULL;
	unsigned int i;

#ifdef BSPC_DEBUG
        nodesLoaded = 0U;
        trianglesLoaded = 0U;
#endif

	sigSize = strlen( BSP_FILE_MAGIC) + 1U;
	savedSig = (char *)( malloc( sizeof( char) * sigSize));
	if( savedSig == NULL)
	{
	    fprintf( stderr, "\nFATAL ERROR: Out of Memory!\n");
	    exit( EXIT_FAILURE);

	} /* End if */

	fread( savedSig, sizeof( char), sigSize, inFile);
	fread( &bspDataVer, sizeof( bspDataVer), 1, inFile);

	if( 
	    ( strcmp( BSP_FILE_MAGIC, savedSig) == 0) && 
	    ( bspDataVer == BSP_DATA_VER)
        )
	{
	    free( savedSig);

	    retVal = (BSPTreeData *)( malloc( sizeof( BSPTreeData)));
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
	        (char **)( malloc( sizeof( char *) * retVal->nMaps));

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
		    sizeof( char) * ( strlen( inpTexName) + 1)
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


            /* Read in vertex definitions */
            fread( 
	        &( retVal->nVertices), 
		sizeof( retVal->nVertices), 1, 
		inFile
	    );
	    

	    retVal->vertCoords = (GLfloat *)( malloc( 
	        retVal->nVertices * 3 * sizeof( GLfloat)
	    ));

            if( retVal->vertCoords == NULL)
	    {
	        fprintf( stderr, "\nFATAL ERROR: Out of Memory!\n");
		exit( EXIT_FAILURE);

	    } /* End if */

	    fread(
	        retVal->vertCoords,
		sizeof( GLfloat), ( 3 * retVal->nVertices),
		inFile
	    );


	    retVal->texCoords = (GLfloat *)( malloc( 
	        retVal->nVertices * 2 * sizeof( GLfloat)
	    ));

            if( retVal->texCoords == NULL)
	    {
	        fprintf( stderr, "\nFATAL ERROR: Out of Memory!\n");
		exit( EXIT_FAILURE);

	    } /* End if */

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


            /* Read in some information about the tree */
	    fread( &( retVal->maxDepth), sizeof( retVal->maxDepth), 1, inFile);
	    fread( &( retVal->numNodes), sizeof( retVal->numNodes), 1, inFile);
	    fread( &( retVal->numTri), sizeof( retVal->numTri), 1, inFile);


	    /* Finally, read in the actual BSP tree */
	    retVal->bspTree = ReadBSPTree( inFile, retVal);


#ifdef BSPC_DEBUG
            printf( 
	        "BSPC: Loaded %u triangles spread over %u nodes "
		"(%hu levels).\n",
		trianglesLoaded, nodesLoaded, retVal->maxDepth
	    );
	    printf( "\t(%u mapped vertex definitions)\n", retVal->nVertices);
	    fflush( stdout);
#endif

	} /* End if */
	else
	{
#ifdef BSPC_DEBUG
	    fprintf( stderr, 
		"\nERROR: Invalid BSP Tree Data or incorrect version!\n"
	    );
#endif

	} /* End else */

    } /* End if */
    else
    {
#ifdef BSPC_DEBUG
        fprintf( stderr, 
	    "\nERROR: Invalid input stream in LoadBSPTreeData( )!\n"
        );
#endif

    } /* End else */

    return retVal;

} /* End function LoadBSPTreeData */


/**
 * Reads a BSP Tree in preorder from the given file.
 */
BSPTree *ReadBSPTree( FILE *inFile, BSPTreeData *bspData)
{
    BSPTree *retVal = NULL;
    unsigned int i;
    Uint8 cFlag;
    GLboolean hasFrontTree, hasBackTree;


    retVal = (BSPTree *)( malloc( sizeof( BSPTree)));
    if( retVal == NULL)
    {
        fprintf( stderr, "\nFATAL ERROR: Out of Memory!\n");
	exit( EXIT_FAILURE);

    } /* End if */

#ifdef BSPC_DEBUG
    nodesLoaded++;
#endif

    fread( &( retVal->numTri), sizeof( retVal->numTri), 1, inFile);
    
    if( retVal->numTri > 0U)
    {
	retVal->triDefs = 
	    (BSPTriFace *)( malloc( retVal->numTri * sizeof( BSPTriFace)));
	 
	if( retVal->triDefs == NULL)
	{
	    fprintf( stderr, "\nFATAL ERROR: Out of Memory!\n");
	    exit( EXIT_FAILURE);

	} /* End if */

	for( i = 0U; i < retVal->numTri; i++)
	{
	    BSPTriFace *aTriFacePtr = ( retVal->triDefs + i);

	    fread( 
		&( aTriFacePtr->texIndex), 
		sizeof( aTriFacePtr->texIndex), 1, 
		inFile
	    );
	    fread( 
		aTriFacePtr->vIndices, 
		sizeof( aTriFacePtr->vIndices[0]), 3, 
		inFile
	    );

	} /* End for */

    } /* End if */
    else
    {
        retVal->triDefs = NULL;

    } /* End else */


#ifdef BSPC_DEBUG
    trianglesLoaded+= retVal->numTri;
#endif

    /* Need to read in the partition plane equation only if
     * there were no triangles in this node, otherwise recalculate
     * it.
     */
    if( retVal->numTri == 0U)
    {
	fread( 
	    &( retVal->partPlane), 
	    sizeof( retVal->partPlane), 1, 
	    inFile
	);

    } /* End if */
    else
    {
        GLfloat triVerts[3][3];
	unsigned int k;

        for( k = 0U; k < 3U; k++)
	{
	    unsigned int vIndex = 3*( retVal->triDefs[0].vIndices[k]);

	    triVerts[k][0] = bspData->vertCoords[ vIndex + 0];
	    triVerts[k][1] = bspData->vertCoords[ vIndex + 1];
	    triVerts[k][2] = bspData->vertCoords[ vIndex + 2];

	} /* End for */

	if( GetPlaneForTri( triVerts, &( retVal->partPlane)) != 0)
	{
	    fprintf( stderr, "ERROR: Degenerate triangle in saved file!\n");
	    exit( EXIT_FAILURE);

	} /* End else */

    } /* End else */

    fread( &cFlag, sizeof( Uint8), 1, inFile);

    hasFrontTree = hasBackTree = GL_FALSE;
    switch( cFlag)
    {
    case 0x00:
	break;

    case 0xB0:
	hasBackTree = GL_TRUE;
	break;

    case 0x0F:
	hasFrontTree = GL_TRUE;
	break;

    case 0xBF:
	hasBackTree = GL_TRUE;
	hasFrontTree = GL_TRUE;
	break;
    
    default:
	fprintf( stderr, "\nERROR: Corrupt file (cFlag=%2x)!\n", cFlag);
	exit( EXIT_FAILURE);
	break;

    } /* End switch */

    if( hasBackTree == GL_TRUE)
    {
	retVal->back = ReadBSPTree( inFile, bspData);

    } /* End if */
    else
    {
	retVal->back = NULL;

    } /* End else */

    if( hasFrontTree == GL_TRUE)
    {
	retVal->front = ReadBSPTree( inFile, bspData);

    } /* End if */
    else
    {
	retVal->front = NULL;

    } /* End else */

    return retVal;

} /* End function ReadBSPTree */


BSPTree *ConvIntBSPTree( IntBSPTreeNode *intTree)
{
    BSPTree *retVal = NULL;
    BSPTriNode *tmpTri;
    unsigned int i;

    retVal = (BSPTree *)( malloc( sizeof( BSPTree)));
    if( retVal == NULL)
    {
        fprintf( stderr, "\nFATAL ERROR: Out of Memory!\n");
	exit( EXIT_FAILURE);

    } /* End if */

    retVal->partPlane.A = intTree->partition.A;
    retVal->partPlane.B = intTree->partition.B;
    retVal->partPlane.C = intTree->partition.C;
    retVal->partPlane.D = intTree->partition.D;

    retVal->numTri = intTree->numTri;

    /* 'intTree->numTri' would definitely be greater than 1 */

    retVal->triDefs = 
        (BSPTriFace *)( malloc( retVal->numTri * sizeof( BSPTriFace)));

    if( retVal->triDefs == NULL)
    {
        fprintf( stderr, "\nFATAL ERROR: Out of Memory!\n");
	exit( EXIT_FAILURE);

    } /* End if */

    i = 0U;
    tmpTri = intTree->triHead;
    while( tmpTri != NULL)
    {
        Uint32 vInd[3];
	GLfloat resV[3][3];
	BSPTriNode *prevPtr;
	BSPPlane tmpPlane;

	vInd[0] = GetVertDefIndex( tmpTri->V[0], tmpTri->T[0], resV[0]);
	vInd[1] = GetVertDefIndex( tmpTri->V[1], tmpTri->T[1], resV[1]);
	vInd[2] = GetVertDefIndex( tmpTri->V[2], tmpTri->T[2], resV[2]);

        /* Have we created a degenerate triangle? */
        if( ( vInd[0] == vInd[1]) || 
	    ( vInd[1] == vInd[2]) || 
	    ( vInd[2] == vInd[0]) ||
	    ( GetPlaneForTri( resV, &tmpPlane) != 0)
        )
	{
	    /* Skip this triangle and adjust the output accordingly */
	    /* Note that 'i' is not incremented in this path */

	    retVal->numTri = ( retVal->numTri - 1U);

	} /* End if */
	else
	{
            /* This is a decent and well-behaved triangle */
	    if( i == 0U)
	    {
	        /* Recalculate plane equation to adjust for loss
		 * of precision during refactoring of vertices.
		 */
	        retVal->partPlane.A = tmpPlane.A;
	        retVal->partPlane.B = tmpPlane.B;
	        retVal->partPlane.C = tmpPlane.C;
	        retVal->partPlane.D = tmpPlane.D;

	    } /* End if */

	    retVal->triDefs[i].vIndices[0] = vInd[0];
	    retVal->triDefs[i].vIndices[1] = vInd[1];
	    retVal->triDefs[i].vIndices[2] = vInd[2];

	    retVal->triDefs[i].texIndex = tmpTri->tIndex;

	    texCtrs[ tmpTri->tIndex]++;

	    i++;

	} /* End else */

        prevPtr = tmpTri;
        tmpTri = tmpTri->next;
	free( prevPtr);

    } /* End while */

    /* Adjust memory usage if we have discarded some or all triangles */
    if( retVal->numTri == 0U)
    {
        free( retVal->triDefs);
	retVal->triDefs = NULL;

    } /* End if */
    else if( retVal->numTri < intTree->numTri)
    {
        BSPTriFace *tmpPtr = NULL;
	
	tmpPtr = (BSPTriFace *)( realloc( 
	    retVal->triDefs, ( retVal->numTri * sizeof( BSPTriFace))
	));
        if( tmpPtr != NULL)
	{
	    retVal->triDefs = tmpPtr;

	} /* End if */
	else if( retVal->numTri > 0U)
	{
	    fprintf( stderr, "\nFATAL ERROR: Memory Allocation Error!\n");
	    exit( EXIT_FAILURE);

	} /* End else */

    } /* End if */


    trianglesCreated += retVal->numTri;


#ifdef BSPC_DEBUG 
    nodesConverted++;
    trianglesConverted += retVal->numTri;
    printf( "\b\b\b\b%3u%%", ( nodesConverted * 100U) / nodesCreated);
    fflush( stdout);
#endif


    if( intTree->back != NULL)
    {
        retVal->back = ConvIntBSPTree( intTree->back);

    } /* End if */
    else
    {
        retVal->back = NULL;

    } /* End if */

    if( intTree->front != NULL)
    {
        retVal->front = ConvIntBSPTree( intTree->front);

    } /* End if */
    else
    {
        retVal->front = NULL;

    } /* End if */

    /* Free the internal tree node */
    intTree->numTri = 0U; intTree->triHead = NULL;
    intTree->back = intTree->front = NULL;
    free( intTree);

    return retVal;

} /* End function ConvIntBSPTree */


Uint16 GetVertDefIndex( GLfloat v[], GLfloat t[], GLfloat resV[])
{
    Uint16 retVal;
    VertDefs *currPtr, *prevPtr;

    resV[0] = v[0];
    resV[1] = v[1];
    resV[2] = v[2];

    prevPtr = currPtr = vertDefsPtr;
    retVal = 0U;
    
    while( currPtr != NULL)
    {
	Uint16 i;

	for( i = 0U; i < currPtr->nDefs; i++)
	{
	    if( ( fabs( currPtr->V[i][0] - v[0]) <= BSP_VERT_ORD_EPSILON) &&
		( fabs( currPtr->V[i][1] - v[1]) <= BSP_VERT_ORD_EPSILON) &&
		( fabs( currPtr->V[i][2] - v[2]) <= BSP_VERT_ORD_EPSILON) &&
		( fabs( currPtr->T[i][0] - t[0]) <= BSP_TEX_ORD_EPSILON) &&
		( fabs( currPtr->T[i][1] - t[1]) <= BSP_TEX_ORD_EPSILON)
	    )
	    {
		/* We have found a match */
		resV[0] = currPtr->V[i][0];
		resV[1] = currPtr->V[i][1];
		resV[2] = currPtr->V[i][2];

		break;

	    } /* End if */

	    retVal++;

	} /* End for */

	if( i < currPtr->nDefs)
	{
	    /* We found a match in the loop above */
	    break;

	} /* End if */
	else if( currPtr->nDefs < DEFS_BLK_SIZE)
	{
	    /* We did not find a match and there is some space in */
	    /* this node itself. ('i' is equal to 'currPtr->nDefs') */

	    currPtr->V[i][0] = v[0];
	    currPtr->V[i][1] = v[1];
	    currPtr->V[i][2] = v[2];

	    currPtr->T[i][0] = t[0];
	    currPtr->T[i][1] = t[1];

	    currPtr->nDefs++;

	    numVertDefs++;


            /* Is this vertex at the edge of the known universe? */

	    minX = ( v[0] < minX) ? ( v[0]) : minX;
	    maxX = ( v[0] > maxX) ? ( v[0]) : maxX;

	    minY = ( v[1] < minY) ? ( v[1]) : minY;
	    maxY = ( v[1] > maxY) ? ( v[1]) : maxY;

	    minZ = ( v[2] < minZ) ? ( v[2]) : minZ;
	    maxZ = ( v[2] > maxZ) ? ( v[2]) : maxZ;

	    /* 'retVal' has the correct value */
	    break;

	} /* End else-if */
	else
	{
	    /* This does not match, carry on... */
	    prevPtr = currPtr;
	    currPtr = currPtr->next;

	} /* End else */

    } /* End while */


    if( currPtr == NULL)
    {
	/* We couldn't find a match or any space in a node */
	currPtr = (VertDefs *)( malloc( sizeof( VertDefs)));
	if( currPtr == NULL)
	{
	    fprintf( stderr, "\nFATAL ERROR: Out of Memory!\n");
	    exit( EXIT_FAILURE);

	} /* End if */

	currPtr->V[0][0] = v[0];
	currPtr->V[0][1] = v[1];
	currPtr->V[0][2] = v[2];

	currPtr->T[0][0] = t[0];
	currPtr->T[0][1] = t[1];

	currPtr->nDefs = 1U;
	numVertDefs++;


	/* Is this vertex at the edge of the known universe? */

	minX = ( v[0] < minX) ? ( v[0]) : minX;
	maxX = ( v[0] > maxX) ? ( v[0]) : maxX;

	minY = ( v[1] < minY) ? ( v[1]) : minY;
	maxY = ( v[1] > maxY) ? ( v[1]) : maxY;

	minZ = ( v[2] < minZ) ? ( v[2]) : minZ;
	maxZ = ( v[2] > maxZ) ? ( v[2]) : maxZ;


	currPtr->next = NULL;

	if( prevPtr != NULL)
	{
	    prevPtr->next = currPtr;

	} /* End if */
	else
	{
	    vertDefsPtr = currPtr;

	} /* End else */

        /* Amazingly, 'retVal' still has the correct value */

    } /* End if */

    return retVal;

} /* End function GetVertDefIndex */


void FreeBSPTreeData( BSPTreeData *bspData)
{
    if( bspData != NULL)
    {
	Uint16 i;

	for( i = 0U; i < bspData->nMaps; i++)
	{
	    free( bspData->mapNames[i]);
	    bspData->mapNames[i] = NULL;

	} /* End for */
	free( bspData->mapNames);
	bspData->mapNames = NULL;

        free( bspData->mapTriNums);
	bspData->mapTriNums = NULL;

	bspData->nMaps = 0U;

	free( bspData->vertCoords);
	free( bspData->texCoords);
	bspData->nVertices = 0U;

        FreeBSPTree( bspData->bspTree);

        bspData->bspTree = NULL;

	bspData->maxDepth = bspData->numNodes = 0U;
	bspData->numTri = 0U;

	free( bspData);

    } /* End if */

} /* End function FreeBSPTreeData */


void FreeBSPTree( BSPTree *root)
{
    if( root != NULL)
    {
	if( root->back != NULL)
	{
	    FreeBSPTree( root->back);
	    root->back = NULL;

	} /* End if */

	if( root->front != NULL)
	{
	    FreeBSPTree( root->front);
	    root->front = NULL;

	} /* End if */

        root->numTri = 0U;
	free( root->triDefs);

	free( root);

    } /* End if */

} /* End function FreeBSPTree */
