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
#ifndef GRAPHICS_FAST_BRUSH_H_
#define GRAPHICS_FAST_BRUSH_H_

#include "../math/coords.h"
#include "canvas.h"
#include "abstract.h"
#include "brush.h"

/**
 * Prepares data needed by the fast-brush. This must be called exactly once
 * before any other fast-brush function, and must be called after
 * brush_load().
 */
void fast_brush_load(void);

/**
 * Generates a "fast brush" drawing method object. A fast brush is a cached
 * representation of a brush, using texture-mapped triangles instead of
 * algorithmic rendering to achieve far higher rendering speeds at the cost of
 * some quality.
 *
 * Creating a fast brush is relatively expensive. Furthermore, each fast brush
 * takes memory proportional to the maximum expected drawing sizes. Because of
 * this, they are stored as opaque on-heap objects rather than on-stack values
 * as with brushes.
 *
 * There are some behavioural differences as compared to the normal brush.
 *
 * - Lines have no variance. Two lines drawn adjacent to each other will appear
 *   completely identical, and it is impossible to change this cheaply as with
 *   the brush, which merely requires a different seed.
 *
 * - Lines are not terminated by points.
 *
 * - Points are unaffected by bristle status; every point will appear as a
 *   freshly flushed brush. Furthermore, points do not degrade the
 *   bristles. However, points do still cycle through the precalculated forms.
 *
 * - Per-point weight affects the scale of the brush, rather than which
 *   bristles apply. This is usually desirable for objects drawn in 3-space
 *   anyway.
 *
 * - The fast brush has a finite maximum size. Exceeding the maximum width
 *   causes the texture to be scaled up, possibly causing visual pixelation. If
 *   the maximum length is exceeded, the tail of the brush will be
 *   repeated. This will cause visual artefacts, and its presence is generally
 *   a bug. The repetition is used to minimise its impact to the user while not
 *   introducing a meaningful performance penalty for its implementation.
 *
 * - There is no per-spec weight. Weights passed in via the drawing operations
 *   are the only way to control width, which is relative to the logical width
 *   of the screen.
 *
 * A fast brush obtained from fast_brush_new() can be freed with a call to
 * fast_brush_delete(). Manipulation is as per the standard drawing_method
 * protocol.
 *
 * @see fast_brush_accum
 */
drawing_method* fast_brush_new(const brush_spec*,
                               coord max_width,
                               coord max_length,
                               unsigned brush_random_seed);
/**
 * Frees the memory held by the given fast_brush.
 */
void fast_brush_delete(drawing_method*);

/**
 * The accumulator for a fast brush. It also includes some data that would
 * normally be considered to belong in the "spec", in order to facilitate
 * sharing of the expensive fast brush objects even with different colours.
 */
typedef struct {
  /**
   * The canvas to which to draw.
   */
  canvas*restrict dst;

  /**
   * An array of colours to use for the pallet. The first pallet_size values
   * are used. This is indexed as with the field of the same name on
   * brush_spec, with out-of-bounds pixels being left untouched.
   */
  const canvas_pixel*restrict colours;
  /**
   * The number of colours in the colours array. Maximum meaningful value is
   * 255.
   */
  unsigned num_colours;

  /**
   * The random seed to which to reset on flush.
   */
  unsigned random_seed;

  /**
   * The random state, used to select point variants. Should be initialised to
   * random_seed.
   */
  unsigned random;

  /**
   * The total distance drawn so far. Initialise to zero.
   */
  unsigned distance;
} fast_brush_accum;

#endif /* GRAPHICS_FAST_BRUSH_H_ */
