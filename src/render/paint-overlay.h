/*-
 * Copyright (c) 2014, 2015 Jason Lingle
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
#ifndef RENDER_PAINT_OVERLAY_H_
#define RENDER_PAINT_OVERLAY_H_

#include "../graphics/canvas.h"
#include "context.h"

/**
 * The paint overlay is a post-rendering effect which transforms the OpenGL
 * framebuffer into a distribution of brush strokes.
 *
 * The alpha channel of the colour buffer is overloaded for three purposes.
 * Firstly, it indicates stroke direction of the stroke centred on the pixel,
 * where theta=alpha*4*2*pi. Secondly, it indicates whether the stroke is
 * a single point or a continuous line. Thirdly, it indicates whether that
 * pixel should be rendered with a graphite colour in the final form (this is a
 * direct input-to-output mapping, independent of the centre of the brush
 * stroke.)
 *
 * The following table shows the four distinct alpha ranges and which flags are
 * activated as a result:
 * - 0.00 == alpha                      discard
 * - 0.00 < alpha <= 0.25               graphite passthrough + continuous
 * - 0.25 < alpha <= 0.50               graphite passthrough + splotch
 * - 0.50 < alpha <= 0.75               continuous
 * - 0.75 < alpha <= 1.00               splotch
 *
 * (Note that while graphite passthrough is a direct mapping, other features
 * are based on the pixel at the centre of the stroke.)
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
 *
 * The input canvas is used to size the portion of the framebuffer which is
 * *read*, whereas the one given to the paint_overlay_new() determines the size
 * of the framebuffer which is *written*.
 */
void paint_overlay_preprocess(paint_overlay*, const rendering_context*restrict,
                              const canvas*);

/**
 * Post-processes the current state of the OpenGL framebuffer using the results
 * of paint_overlay_preprocess().
 */
void paint_overlay_postprocess(paint_overlay*,
                               const rendering_context*restrict);

#endif /* RENDER_PAINT_OVERLAY_H_ */
