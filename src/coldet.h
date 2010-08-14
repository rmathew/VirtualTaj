/* Copyright (c) 2001 Ranjit Mathew. All rights reserved.
 * Use of this source code is governed by the terms of the BSD licence
 * that can be found in the LICENCE file.
 */

/*
 * COLDET.H: Declarations for the collision detection functions.
 */

#ifndef _COLDET_H
#define _COLDET_H

/* We do not want SDL to automatically include "glext.h" */
#define NO_SDL_GLEXT

#include "SDL.h"
#include "SDL_opengl.h"

#include "gld.h"


/* Function prototypes */

extern GLboolean hasCollision( 
    GLData *model, 
    GLfloat fromPt[], GLfloat toPt[],
    GLfloat *dist
);

#endif    /* _COLDET_H */


