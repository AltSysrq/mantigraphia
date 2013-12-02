/*
  Copyright (c) 2013 Jason Lingle
  All rights reserved.

  Redistribution and use in source and binary forms, with or without
  modification, are permitted provided that the following conditions
  are met:
  1. Redistributions of source code must retain the above copyright
     notice, this list of conditions and the following disclaimer.
  2. Redistributions in binary form must reproduce the above copyright
     notice, this list of conditions and the following disclaimer in the
     documentation and/or other materials provided with the distribution.
  3. Neither the name of the author nor the names of its contributors
     may be used to endorse or promote products derived from this software
     without specific prior written permission.

     THIS SOFTWARE IS PROVIDED BY THE AUTHORS AND CONTRIBUTORS ``AS IS'' AND
     ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
     IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
     ARE DISCLAIMED.  IN NO EVENT SHALL THE AUTHORS OR CONTRIBUTORS BE LIABLE
     FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
     DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
     OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
     HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
     LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
     OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
     SUCH DAMAGE.
*/
#ifndef GRAPHICS_DM_PROJ_H_
#define GRAPHICS_DM_PROJ_H_

#include "../coords.h"
#include "abstract.h"
#include "perspective.h"

/**
 * A dm_proj is a drawing method which projects vertices in world-space into
 * screen space to be given to another drawing method. Weights are adjusted
 * based on the resulting Z distance, and optionally according to perceived
 * rotation of the object being drawn.
 *
 * The dm_proj method has no accumulator of its own. The accumulator given to
 * it is passed to the underlying method. Underlying functions are not called
 * if any point fails to project.
 */
typedef struct {
  drawing_method meth;

  /**
   * The underlying drawing method to call.
   */
  const drawing_method* delegate;

  /**
   * The perspective used to transform world coordinates into screen
   * coordinates.
   */
  const perspective* proj;

  /**
   * Planes controlling the weight applied to points drawn. Anything nearer
   * than the near clipping plane, or farther than the far clipping plane, gets
   * a weight of zero. Points between the two max planes receive the nominal
   * weight. Points between a clipping and max plane have a weight linearly
   * interpolated between the two.
   */
  coord_offset near_clipping, near_max, far_max, far_clipping;

  /**
   * The nominal weight of the points to draw. This is used to scale the weight
   * passed in to the normal drawing method functions, before distance scales
   * it again.
   */
  zo_scaling_factor nominal_weight;

  /**
   * Only relevant if perp_weight_add is non-zero. Defines a "normal" for the
   * "surface" being drawn; some value between 0 and perp_weight_add is added
   * to the weight before applying distance scaling based on how parallel the
   * normal is with the camera. The effect is that such objects appear thicker
   * when facing the camera.
   */
  vo3 normal;
  /**
   * The magnitude of the normal vector, if relevant.
   */
  coord normal_magnitude;
  /**
   * The amount by which to adjust the non-depth-specific weight based upon
   * camera location.
   */
  zo_scaling_factor perp_weight_add;
} dm_proj;

/**
 * Initialises the meth field of the given dm_proj, and zeroes optional
 * fields.
 */
void dm_init(dm_proj*);

/**
 * Calculates a nominal width that can be used to obtain the appearance of the
 * given world size at the nominal distance for this dm_proj. This assumes that
 * the underlying drawing method interprets weight as a fraction of screen
 * width.
 */
zo_scaling_factor dm_proj_calc_weight(const dm_proj*,
                                      const canvas*,
                                      coord desired_size);

void dm_proj_draw_point(void*,
                        const dm_proj*,
                        const vc3,
                        zo_scaling_factor);

void dm_proj_draw_line(void*,
                       const dm_proj*,
                       const vc3,
                       zo_scaling_factor,
                       const vc3,
                       zo_scaling_factor);

void dm_proj_flush(void*, const dm_proj*);

#endif /* GRAPHICS_DM_PROJ_H_ */
