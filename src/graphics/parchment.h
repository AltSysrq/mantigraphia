/*-
 * Copyright (c) 2013, 2014 Jason Lingle
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. Neither the name of the author nor the names of its contributors
 *    may be used to endorse or promote products derived from this software
 *    without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR ``AS IS'' AND ANY EXPRESS OR
 * IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES
 * OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.
 * IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR ANY DIRECT, INDIRECT,
 * INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT
 * NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
 * THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */
#ifndef GRAPHICS_PARCHMENT_H_
#define GRAPHICS_PARCHMENT_H_

#include "canvas.h"
#include "../coords.h"

/**
 * A parchment is a tilable texture usually drawn as the background of the
 * canvas. When drawn, it is transformed so as to match rotation of the camera
 * (ie, it looks like the camera pans across a fixed painting).
 */
typedef struct parchment_s parchment;

/**
 * Loads and converts the externally-stored parchment texture into memory, so
 * that parchments can be drawn. This must be called exactly once before any
 * other parchment functions are called. This call is somewhat expensive.
 *
 * If this function fails, a diagnostic is printed and the program exits.
 */
void parchment_init(void);

/**
 * Allocates a new parchment with the camera set to its origin.
 */
parchment* parchment_new(void);
/**
 * Releases the resources held by the given parchment.
 */
void parchment_delete(parchment*);

/**
 * Draws the parchment to cover the entire screen with OpenGL.
 */
void parchment_draw(canvas*, const parchment*);

/**
 * Updates the transformation of the parchment according to the old and new
 * angle information. yaw is rotation about the Y axis; pitch is rotation about
 * the X axis (relative to the yaw).
 */
void parchment_xform(parchment*,
                     angle old_yaw,
                     angle old_pitch,
                     angle new_yaw,
                     angle new_pitch,
                     angle fov_x,
                     angle fov_y,
                     coord_offset screen_w,
                     coord_offset screen_h);

#endif /* GRAPHICS_PARCHMENT_H_ */
