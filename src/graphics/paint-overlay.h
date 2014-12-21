/*-
 * Copyright (c) 2014 Jason Lingle
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
#ifndef GRAPHICS_PAINT_OVERLAY_H_
#define GRAPHICS_PAINT_OVERLAY_H_

#include "canvas.h"

/* TODO: The docs in this file describe how it *will* work. It doesn't work
 * this way yet.
 */

/**
 * The paint overlay is a post-rendering effect which transforms the OpenGL
 * framebuffer into a distribution of brush strokes.
 *
 * The input state of the framebuffer does not occur in true colour. Instead of
 * (R,G,B), each pixel is a (offset,plane,direction) triple.
 *
 * Direction specifies the desired stroke direction. The direction of the pixel
 * at the centre of a brush stroke determines the whole stroke's orientation.
 *
 * The actual colour to use for a point in space is found by reading from a
 * texture at (offset,plane), usually based on the pixel at the centre of the
 * brush stroke.
 *
 * plane additionally determines how the framebuffer is interpreted. If the
 * pixel at the fragment being replaced is in plane 128 (0.5), it is passed
 * through, only being affected by the colour lookup. If the pixel at the
 * centre of the stroke is in plane 0, the whole fragment is discarded.
 * Otherwise, the pixel at the centre of the stroke determines the brush shape:
 * Planes 1..127 are static strokes (ie, a touch of the brush to the canvas);
 * planes 129..255 are dynamic strokes (ie, supposed to look like a continuous
 * stroke across the canvas when possible).
 */
typedef struct paint_overlay_s paint_overlay;

/**
 * Prepares a new overlay which can apply to a framebuffer whose dimensions
 * match the given canvas. This must be run on the GL thread.
 */
paint_overlay* paint_overlay_new(const canvas*);
/**
 * Frees the resources held by the given paint overlay. This must be run on the
 * GL thread.
 */
void paint_overlay_delete(paint_overlay*);

/**
 * Prepares the current state of the OpenGL framebuffer for post-processing.
 * This should be performed before the background is filled in.
 */
void paint_overlay_preprocess(paint_overlay*);

/**
 * Post-processes the current state of the OpenGL framebuffer using the results
 * of paint_overlay_preprocess().
 */
void paint_overlay_postprocess(paint_overlay*);

#endif /* GRAPHICS_PAINT_OVERLAY_H_ */
