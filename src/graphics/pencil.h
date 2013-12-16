/*-
 * Copyright (c) 2013 Jason Lingle
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
#ifndef PENCIL_H_
#define PENCIL_H_

#include "abstract.h"
#include "canvas.h"

/**
 * The pencil is a simple drawing method which opaquely fills an area with one
 * solid colour. The weight of the pencil scales the thickness down from the
 * pencil's configured basic thickness. The pencil will always draw with a
 * thickness of at least 1.
 *
 * The accumulator for a pencil is just the target canvas.
 */
typedef struct {
  drawing_method meth;
  /**
   * The colour with which this pencil will draw. The alpha channel is copied
   * verbatim.
   */
  canvas_pixel colour;
  /**
   * The factor for the basic thickness of the pencil. The real thickness is
   * derived by scaling the canvas width by this amount.
   */
  zo_scaling_factor thickness;
} pencil_spec;

/**
 * Initialises the given pencil configuration. This sets the drawing_method up,
 * sets the colour to black, and the thickness to 1/1024.
 */
void pencil_init(pencil_spec*);

void pencil_draw_line(canvas*,
                      const pencil_spec*,
                      const vo3 from,
                      zo_scaling_factor from_weight,
                      const vo3 to,
                      zo_scaling_factor to_weight);

void pencil_draw_point(canvas*,
                       const pencil_spec*,
                       const vo3 where,
                       zo_scaling_factor weight);

void pencil_flush(canvas*, const pencil_spec*);

#endif /* PENCIL_H_ */
