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
#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include <stdlib.h>
#include <assert.h>

#include "../coords.h"
#include "perspective.h"
#include "abstract.h"
#include "dm-proj.h"

void dm_init(dm_proj* this) {
  this->meth.draw_point = (drawing_method_draw_point_t)dm_proj_draw_point;
  this->meth.draw_line  = (drawing_method_draw_line_t) dm_proj_draw_line;
  this->meth.flush      = (drawing_method_flush_t)     dm_proj_flush;
  this->perp_weight_add = 0;
  this->nominal_weight = ZO_SCALING_FACTOR_MAX;
}

zo_scaling_factor dm_proj_calc_weight(const canvas* canv,
                                      const perspective* proj,
                                      coord distance,
                                      coord desired_size) {
  /* Project a point at relative coordinates (size,0,far_max), as well as a
   * point at (0,0,far_max) to account for centring translation. The X
   * resulting coordinate of the projected point is the negative of the actual
   * size on screen. This actual size can then be converted to a
   * screen-width-relative size.
   */
  vo3 sample, origin;
  vo3 psample, porigin;
  coord_offset screen_dist;

  sample[0] = desired_size;
  sample[1] = 0;
  sample[2] = -(coord_offset)distance;
  origin[0] = 0;
  origin[1] = 0;
  origin[2] = -(coord_offset)distance;

  if (!perspective_proj_rel(psample, sample, proj)) abort();
  if (!perspective_proj_rel(porigin, origin, proj)) abort();

  screen_dist = psample[0] - porigin[0];
  assert(screen_dist >= 0);

  return screen_dist * ZO_SCALING_FACTOR_MAX / canv->w;
}

static zo_scaling_factor adjust_weight(const dm_proj* this,
                                       zo_scaling_factor sweight,
                                       const vo3 point) {
  signed long long weight = sweight;
  vo3 xnorm;
  coord_offset z = point[2];

  if (z <= this->near_clipping || z >= this->far_clipping)
    return 0;

  /* Adjust weight based on normal, if requested */
  if (this->perp_weight_add) {
    /* Translate the normal into relative world space. After this translation,
     * the absolute value of the Z coordinate is proportional to the dot
     * product of the normal with the camera angle, and can be normalised by
     * dividing by the normal_magnitude.
     */
    perspective_xlate(xnorm, (const coord*)this->normal, this->proj);
    weight += this->perp_weight_add * abs(xnorm[2]) / this->normal_magnitude;
  }

  if (z >= this->near_max && z <= this->far_max)
    /* Nominal distance; no adjustment required */
    return weight;
  else if (z < this->near_max)
    return weight * (z - this->near_clipping) /
      (this->near_max - this->near_clipping);
  else
    return weight * (this->far_clipping - z) /
      (this->far_clipping - this->far_max);
}

void dm_proj_draw_point(void* accum,
                        const dm_proj* this,
                        const vc3 where,
                        zo_scaling_factor weight) {
  vo3 projected;
  if (!perspective_proj(projected, where, this->proj)) return;

  weight = adjust_weight(this, weight, projected);
  dm_draw_point(accum, this->delegate, projected, weight);
}

void dm_proj_draw_line(void* accum,
                       const dm_proj* this,
                       const vc3 from,
                       zo_scaling_factor from_weight,
                       const vc3 to,
                       zo_scaling_factor to_weight) {
  vo3 pfrom, pto;

  if (!perspective_proj(pfrom, from, this->proj)) return;
  if (!perspective_proj(pto, to, this->proj))     return;

  from_weight = adjust_weight(this, from_weight, pfrom);
  to_weight   = adjust_weight(this, to_weight,   pto);

  dm_draw_line(accum, this->delegate, pfrom, from_weight, pto, to_weight);
}

void dm_proj_flush(void* accum,
                   const dm_proj* this) {
  dm_flush(accum, this->delegate);
}
