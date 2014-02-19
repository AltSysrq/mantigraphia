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
#ifndef GRAPHICS_GLBRUSH_H_
#define GRAPHICS_GLBRUSH_H_

#include "../frac.h"
#include "../gl/marshal.h"
#include "abstract.h"
#include "canvas.h"

/**
 * The glbrush uses OpenGL shaders to quickly accomplish an effect similar to
 * that of the normal brush, albeit with less flexibility.
 *
 * glbrush_spec values have thread affinity.
 */
typedef struct {
  /**
   * Vtable. Initialised by glbrush_init().
   */
  drawing_method meth;

  /**
   * Control the interpretation of "scale" of the X and Y texture
   * coordinates. A value of fraction_of(1) will require an entire screen width
   * of drawing to cross the texture once.
   */
  fraction xscale, yscale;
  /**
   * The rate of brush decay. Larger values cause the brush to transition more
   * quickly to the secondary colour before petering out.
   *
   * This value is passed directly to the shader without processing in
   * software.
   */
  float decay;
  /**
   * The noise level for interpretation of the texture. A value of 1.0 will
   * cause every colour between the primary and secondary to be used equally,
   * even without decay. A value of 0.0 will make the brush solidly the primary
   * colour. More generally, in the absence of decay, the colour can be thought
   * to be determined by (t*primary + (1-t)*secondary), where (t =
   * rand(1.0-noise, 1.0)).
   *
   * This value is passed directly to the shader without processing in
   * software.
   */
  float noise;
  /**
   * A value between 0 and 1 which offsets the texture used by the shader, to
   * add variety to the brush strokes produced.
   *
   * This value is passed directly to the shader without processing in
   * software.
   */
  float texoff;
  /**
   * The primary and secondary colours for the brush. The brush begins with the
   * primary colour, and progresses to the secondary colour, becomming blank
   * after the secondary colour is passed.
   */
  canvas_pixel colour[2];
  /**
   * The logical width, in pixels, of the screen.
   */
  unsigned screen_width;
  /**
   * The slab to which drawing operations will occur.
   *
   * Initialised by glbrush_init().
   */
  glm_slab* slab;
} glbrush_spec;

/**
 * Accumulator for glbrush.
 */
typedef struct {
  /**
   * The total distance covered so far, in terms of texture
   * coordinates. Initialise to zero.
   */
  float distance;
} glbrush_accum;

/**
 * Prepares static resources needed by glbrush. This must be called exactly
 * once before any other glbrush function, but after GLM is initialised.
 */
void glbrush_load(void);

/**
 * Initialises the vtable and slab for the given spec. Other fields are left
 * uninitialised.
 */
void glbrush_init(glbrush_spec*);

void glbrush_draw_point(glbrush_accum*, const glbrush_spec*,
                        const vo3, zo_scaling_factor);
void glbrush_draw_line(glbrush_accum*, const glbrush_spec*,
                       const vo3, zo_scaling_factor,
                       const vo3, zo_scaling_factor);
void glbrush_flush(glbrush_accum*, const glbrush_spec*);

#endif /* GRAPHICS_GLBRUSH_H_ */
