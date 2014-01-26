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
#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include <string.h>

#include "../simd.h"
#include "../coords.h"
#include "../graphics/abstract.h"
#include "draw-queue.h"
#include "turtle.h"

int turtle_init(turtle_state* this, const perspective* proj,
                const vc3 init, unsigned scale) {
  vc3 wtsx, wtsy, wtsz;
  /* These are each four-elements long so that the compiler can do a direct SSE
   * load of each, instead of loading each dword one at a time.
   */
  coord_offset stsx[4], stsy[4], stsz[4], sinit[4];
  simd4 vsinit;
  static const simd4 zero = simd_init4(0,0,0,0);

  this->proj = proj;

  wtsx[0] = init[0] + scale;
  wtsy[1] = init[1] + scale;
  wtsz[2] = init[2] + scale;
  wtsy[0] = wtsz[0] = init[0];
  wtsx[1] = wtsz[1] = init[1];
  wtsx[2] = wtsy[2] = init[2];

  perspective_xlate(sinit, init, proj);
  perspective_xlate(stsx, wtsx, proj);
  perspective_xlate(stsy, wtsy, proj);
  perspective_xlate(stsz, wtsz, proj);

  sinit[3] = stsx[3] = stsy[3] = stsz[3] = 0;

  vsinit = simd_of_vo4(sinit);
  this->pos.curr = vsinit;
  this->pos.prev = vsinit;
  this->space.x = simd_subvv(simd_of_vo4(stsx), vsinit);
  this->space.y = simd_subvv(simd_of_vo4(stsy), vsinit);
  this->space.z = simd_subvv(simd_of_vo4(stsz), vsinit);

  /* Test for collapsed space */
  if (simd_eq(zero, this->space.x) ||
      simd_eq(zero, this->space.y) ||
      simd_eq(zero, this->space.z))
    return 0;

  /* OK */
  return 1;
}

void turtle_move(turtle_state* this, coord_offset dx,
                 coord_offset dy, coord_offset dz) {
  this->pos.prev = this->pos.curr;
  this->pos.curr =
    simd_addvv(this->pos.curr,
               simd_addvv(
                 simd_mulvs(this->space.x, dx),
                 simd_addvv(
                   simd_mulvs(this->space.y, dy),
                   simd_mulvs(this->space.z, dz))));
}

void turtle_rotate_axes(simd4*restrict xp, simd4*restrict yp, angle ang) {
  simd4 ox = *xp, oy = *yp;
  signed cos = zo_cos(ang), sin = zo_sin(ang);
  *xp = simd_divvs(simd_subvv(simd_mulvs(ox, cos), simd_mulvs(oy, sin)),
                   (signed)ZO_SCALING_FACTOR_MAX);
  *yp = simd_divvs(simd_addvv(simd_mulvs(oy, cos), simd_mulvs(ox, sin)),
                   (signed)ZO_SCALING_FACTOR_MAX);
}

int turtle_scale_down(turtle_state* this, unsigned shift) {
  const simd4 zero = simd_init4(0,0,0,0);

  this->space.x = simd_shra(this->space.x, shift);
  this->space.y = simd_shra(this->space.y, shift);
  this->space.z = simd_shra(this->space.z, shift);

  return
    !simd_eq(zero, this->space.x) &&
    !simd_eq(zero, this->space.y) &&
    !simd_eq(zero, this->space.z);
}

int turtle_put_point(drawing_queue_burst* dst, const turtle_state* this,
                     zo_scaling_factor weight) {
  coord_offset rwhere[4];
  vo3 where;

  simd_to_vo4(rwhere, this->pos.curr);
  if (!perspective_proj_rel(where, rwhere, this->proj))
    return 0;

  drawing_queue_put_point(dst, where, weight);
  return 1;
}

int turtle_put_points(drawing_queue_burst* dst, const turtle_state* this,
                      zo_scaling_factor from_weight,
                      zo_scaling_factor to_weight) {
  coord_offset rfrom[4], rto[4];
  vo3 from, to;

  simd_to_vo4(rfrom, this->pos.prev);
  simd_to_vo4(rto, this->pos.curr);

  if (!perspective_proj_rel(from, rfrom, this->proj) ||
      !perspective_proj_rel(to, rto, this->proj))
    return 0;

  drawing_queue_put_points(dst, from, from_weight, to, to_weight);
  return 1;
}
