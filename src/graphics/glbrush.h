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

#include "../math/frac.h"
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
   * A value between 0 and 1 which offsets the texture used by the shader, to
   * add variety to the brush strokes produced.
   *
   * This value is passed directly to the shader without processing in
   * software.
   */
  float texoff;
  /**
   * The initial value for the accumulator's distance. This is often zero, but
   * non-zero values are sometimes useful.
   */
  float base_distance;
  /**
   * The random seed to use.
   */
  unsigned random_seed;
  /**
   * The logical width, in pixels, of the screen.
   */
  unsigned screen_width;
  /**
   * The logical height, in pixels, of the screen.
   */
  unsigned screen_height;
  /**
   * The slabs to which drawing operations will occur.
   *
   * Initialised by glbrush_init().
   */
  glm_slab* line_slab, * point_slab, * point_poly_slab;
} glbrush_spec;

/**
 * Accumulator for glbrush.
 */
typedef struct {
  /**
   * The total distance covered so far, in terms of texture
   * coordinates. Initialise to zero, in most cases.
   */
  float distance;
  /**
   * The current random value. Initialise to random_seed.
   */
  unsigned rand;
} glbrush_accum;

/**
 * A glbrush_handle is a heavy-weight handle on the GL information needed to
 * render a glbrush.
 */
typedef struct glbrush_handle_s glbrush_handle;

/**
 * Information which is constant wrt a glbrush_handle.
 */
typedef struct {
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
   * The colour palette for the brush. The brush begins with the zeroth colour,
   * and progresses through the others, becomming blank after the final is
   * passed. Adjacent colours are blended into one another as necessary.
   */
  const canvas_pixel* palette;
  unsigned palette_size;
} glbrush_handle_info;

/**
 * Prepares static resources needed by glbrush. This must be called exactly
 * once before any other glbrush function, but after GLM is initialised.
 */
void glbrush_load(void);

/**
 * Creates a glbrush_handle with the given parameters.
 */
glbrush_handle* glbrush_hnew(const glbrush_handle_info*);
/**
 * Reconfigures the given glbrush_handle to carry the given parameters. The
 * effect this change has on pending drawing operations is undefined.
 */
void glbrush_hconfig(glbrush_handle*, const glbrush_handle_info*);
/**
 * If *handle is NULL, sets *handle to the result of glbrush_hnew() with the
 * given info. Otherwise, if permit_refresh, calls glbrush_hconfig() on the
 * handle with the given info.
 *
 * permit_refresh is intended to be used to only, eg, refresh the underlying
 * texture four times per second. This is necessary for GL implementations like
 * Mesa, where updating a texture, even a miniscule one as used by glbrush, is
 * strangely expensive.
 */
void glbrush_hset(glbrush_handle**, const glbrush_handle_info*,
                  int permit_refresh);
/**
 * Destroys the given glbrush_handle.
 */
void glbrush_hdelete(glbrush_handle*);

/**
 * Initialises the vtable and slab for the given spec. Other fields are left
 * uninitialised. The brush becomes invalid when the associated handle is
 * destroyed. No action need be taken to destroy the brush itself.
 */
void glbrush_init(glbrush_spec*, glbrush_handle*);

void glbrush_draw_point(glbrush_accum*, const glbrush_spec*,
                        const vo3, zo_scaling_factor);
void glbrush_draw_line(glbrush_accum*, const glbrush_spec*,
                       const vo3, zo_scaling_factor,
                       const vo3, zo_scaling_factor);
void glbrush_flush(glbrush_accum*, const glbrush_spec*);

#endif /* GRAPHICS_GLBRUSH_H_ */
