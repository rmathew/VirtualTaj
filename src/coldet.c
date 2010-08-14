/* Copyright (c) 2001 Ranjit Mathew. All rights reserved.
 * Use of this source code is governed by the terms of the BSD licence
 * that can be found in the LICENCE file.
 */

/*
 * COLDET.C: Collision detection routines.
 *
 * Based on the paper "Fast, Minimum Storage Ray/Triangle Intersection",
 * by Tomas Moller and Ben Trumbore, and the given source code. See 
 * "http://www.acm.org/jgt/papers/MollerTrumbore97/" for more details.
 */


#include <stdio.h>
#include <math.h>
#include <limits.h>
#include <float.h>

#include "coldet.h"


/* Useful constant and macro definitions */

#define CROSS( dest, v1, v2) \
    dest[0] = v1[1]*v2[2] - v1[2]*v2[1]; \
    dest[1] = v1[2]*v2[0] - v1[0]*v2[2]; \
    dest[2] = v1[0]*v2[1] - v1[1]*v2[0]

#define DOT( v1, v2) ( v1[0]*v2[0] + v1[1]*v2[1] + v1[2]*v2[2])

#define SUB( dest, v1, v2) \
    dest[0] = v1[0] - v2[0]; \
    dest[1] = v1[1] - v2[1]; \
    dest[2] = v1[2] - v2[2]


/* Local function prototypes */

static GLboolean intersectsFace( 
    GLfloat orig[], GLfloat dir[], 
    GLfloat vert0[], GLfloat vert1[], GLfloat vert2[],
    GLfloat *t, GLfloat *u, GLfloat *v
);


GLboolean hasCollision( 
    GLData *model, 
    GLfloat fromPt[], GLfloat toPt[],
    GLfloat *dist
)
{
    GLboolean retVal = GL_FALSE;

    GLfloat dir[3];
    GLdouble dirMag;
    unsigned int i, j;


    /* Initialise stuff */
    *dist = FLT_MAX;


    /* Prepare the normalised direction of movement vector */

    dir[0] = toPt[0] - fromPt[0];
    dir[1] = toPt[1] - fromPt[1];
    dir[2] = toPt[2] - fromPt[2];

    dirMag = sqrt( 
        ( (GLdouble )dir[0] * (GLdouble )dir[0]) +
        ( (GLdouble )dir[1] * (GLdouble )dir[1]) +
        ( (GLdouble )dir[2] * (GLdouble )dir[2])
    );

    if( dirMag > 0.0)
    {
        dir[0] = (GLfloat )( (GLdouble )dir[0] / dirMag);
        dir[1] = (GLfloat )( (GLdouble )dir[1] / dirMag);
        dir[2] = (GLfloat )( (GLdouble )dir[2] / dirMag);

    } /* End if */
    else
    {
#ifdef VTAJ_DEBUG
        fprintf( stderr, "ERROR: dirMag is 0 in function hasCollision( )\n");
#endif
        return GL_TRUE;

    } /* End else */

    /* Now just iterate over all the triangles comprising the model
     * to find the nearest hit. Brute force and expensive.
     */
    for( i = 0U; i < model->nMaps; i++)
    {
        for( j = 0U; j < model->mapTriNums[i]; j++)
	{
	    GLfloat v0[3], v1[3], v2[3];
	    Uint16 vInd[3];
	    GLfloat tmpT = FLT_MAX;
	    GLfloat tmpU = FLT_MAX;
	    GLfloat tmpV = FLT_MAX;

	    vInd[0] = *( model->triFaces[i] + 3*j + 0);
	    vInd[1] = *( model->triFaces[i] + 3*j + 1);
	    vInd[2] = *( model->triFaces[i] + 3*j + 2);

            v0[0] = *( model->vertCoords + 3*vInd[0] + 0);
            v0[1] = *( model->vertCoords + 3*vInd[0] + 1);
            v0[2] = *( model->vertCoords + 3*vInd[0] + 2);

            v1[0] = *( model->vertCoords + 3*vInd[1] + 0);
            v1[1] = *( model->vertCoords + 3*vInd[1] + 1);
            v1[2] = *( model->vertCoords + 3*vInd[1] + 2);

            v2[0] = *( model->vertCoords + 3*vInd[2] + 0);
            v2[1] = *( model->vertCoords + 3*vInd[2] + 1);
            v2[2] = *( model->vertCoords + 3*vInd[2] + 2);

	    if( intersectsFace( fromPt, dir, v0, v1, v2, &tmpT, &tmpU, &tmpV)
		== GL_TRUE
	    )
	    {
		if( ( tmpT >= 0.0f) && ( tmpT < *dist) && ( tmpT <= dirMag))
		{
		    *dist = tmpT;

		    retVal = GL_TRUE;

		} /* End if */

	    } /* End if */

	} /* End for */

    } /* End for */

    return retVal;

} /* End function hasCollision */


GLboolean intersectsFace( 
    GLfloat orig[], GLfloat dir[], 
    GLfloat vert0[], GLfloat vert1[], GLfloat vert2[],
    GLfloat *t, GLfloat *u, GLfloat *v
)
{
    GLfloat edge1[3], edge2[3], tVec[3], pVec[3], qVec[3];
    GLfloat det, invDet;


    /* Find vectors for two edges sharing vert0 */
    SUB( edge1, vert1, vert0);
    SUB( edge2, vert2, vert0);

    /* Begin calculating determinant - also used to calculate U param */
    CROSS( pVec, dir, edge2);

    /* If determinant is near zero, ray lies in plane of triangle */
    det = DOT( edge1, pVec);


    /* NOTE: TEST_CULL code branch from the paper omitted */


    if( ( det > -FLT_EPSILON) && ( det < FLT_EPSILON))
    {
        return GL_FALSE;

    } /* End if */

    invDet = +1.0f / det;

    /* Calculate distance from vert0 to ray origin */
    SUB( tVec, orig, vert0);

    /* Calculate U parameter and test bounds */
    *u = ( DOT( tVec, pVec) * invDet);
    if( ( *u < 0.0f) || ( *u > 1.0f))
    {
        return GL_FALSE;

    } /* End if */

    /* Prepare to test V parameter */
    CROSS( qVec, tVec, edge1);

    /* Calculate V parameter and test bounds */
    *v = ( DOT( dir, qVec) * invDet);
    if( ( *v < 0.0f) || ( ( *u + *v) > 1.0f))
    {
        return GL_FALSE;

    } /* End if */

    /* Calculate T - ray intersects triangle after all */
    *t = ( DOT( edge2, qVec) * invDet);

    return GL_TRUE;

} /* End function intersectsFace */


