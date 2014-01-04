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

#include <string.h>
#include <stdlib.h>

#include "../coords.h"
#include "../alloc.h"
#include "../rand.h"
#include "../micromp.h"
#include "fog.h"

#define BOUNDS 65536
#define ZDIST_MAX fraction_of(64)
#define SPEED (BOUNDS/256/SECOND)

typedef struct {
  coord_offset x, y;
  coord_offset vx, vy;
  fraction z_dist;
  fraction visibility;
} wisp;

struct fog_effect_s {
  mersenne_twister twister;
  fraction wisp_w, wisp_h;
  signed xmin, xmax, ymin, ymax;
  unsigned num_wisps;

  wisp wisps[1];
};

static void randomise_direction(wisp*, fog_effect*);

fog_effect* fog_effect_new(unsigned num_wisps,
                           fraction wisp_w,
                           fraction wisp_h,
                           unsigned seed) {
  unsigned i;
  fog_effect* this = xmalloc(offsetof(fog_effect,wisps) +
                             num_wisps * sizeof(wisp));
  twister_seed(&this->twister, seed);
  this->wisp_w = wisp_w;
  this->wisp_h = wisp_h;
  this->num_wisps = num_wisps;
  this->xmin = -fraction_smul(BOUNDS, wisp_w);
  this->ymin = -fraction_smul(BOUNDS, wisp_h);
  this->xmax = BOUNDS+fraction_smul(BOUNDS, wisp_w);
  this->ymax = BOUNDS+fraction_smul(BOUNDS, wisp_h);
  for (i = 0; i < num_wisps; ++i) {
    this->wisps[i].x = twist(&this->twister) % BOUNDS;
    this->wisps[i].y = BOUNDS * i / num_wisps;
    this->wisps[i].z_dist = twist(&this->twister) % ZDIST_MAX;
    randomise_direction(this->wisps+i, this);
  }

  return this;
}

void fog_effect_delete(fog_effect* this) {
  free(this);
}

static void randomise_direction(wisp* this, fog_effect* fog) {
  angle direction = twist(&fog->twister);
  cossinms(&this->vx, &this->vy, direction, SPEED);
}

void fog_effect_update(fog_effect* this, chronon et) {
  unsigned i, t;
  for (i = 0; i < this->num_wisps; ++i) {
    for (t = 0; t < et; ++t) {
      this->wisps[i].x += this->wisps[i].vx;
      this->wisps[i].y += this->wisps[i].vy;
      while ((this->wisps[i].x < this->xmin && this->wisps[i].vx <= 0) ||
             (this->wisps[i].x > this->xmax && this->wisps[i].vx >= 0) ||
             (this->wisps[i].y < this->ymin && this->wisps[i].vy <= 0) ||
             (this->wisps[i].y > this->ymax && this->wisps[i].vy >= 0))
        randomise_direction(this->wisps+i, this);
    }
  }
}

static void wisp_occlude(wisp*restrict this, const canvas*restrict c,
                         unsigned ww, unsigned wh,
                         coord near, coord global_near_mid,
                         coord global_far_mid, coord far) {
  coord_offset zshift = fraction_smul(global_far_mid - global_near_mid,
                                      this->z_dist);
  coord near_mid = global_near_mid + zshift, far_mid = global_far_mid + zshift;
  coord_offset xmax = c->w, ymax = c->h;
  coord_offset wx, wy;
  coord dim, x0, x1, y0, y1, x, y;
  fraction min_visibility = fraction_of(1), visibility;
  coord maxdist = ww/2 + wh/2;
  fraction imaxdist = fraction_of(maxdist);
  fraction imidnear = fraction_of(near_mid - near);
  fraction ifarmid  = fraction_of(far - far_mid);
  fraction zmul;
  int is_visible = 0;
  const canvas_depth*restrict depth;

  /* The overall visibility of a wisp is simply the minimum visibility derived
   * from any pixel within the bounding box.
   *
   * Pixels nearer the near plane or beyond the far plane always have a
   * visibility of 1.0 (ie, they don't affect the wisp). For other pixels, the
   * visibility is based upon the manhattan distance from the wisp's centre to
   * the pixel in question, as well as a Z-multiplier derived from depth
   * differences.
   *
   * When the pixel is between the mid planes, the Z-multiplier is always 1.0,
   * and the visibility is (dist/maxdist). For the more general case,
   * visibility is computed as
   *
   *   1.0 - zmul*(maxdist - dist)/maxdist
   *
   * where zmul scales linearly from 0.0 to 1.0 between (near,near_mid) and
   * (far_mid,far). This formula has a couple important properties:
   *
   * - No pixel within the mid region will ever be occluded, as it causes the
   *   wisp to scale back just enough to not touch it.
   *
   * - As Z distance changes, the size of the wisp scales smoothly.
   *
   * - As the wisp's distance to various pixels changes, it's size scales
   *   smoothly.
   *
   * We also track whether any pixels actually make the wisp relevant; if no
   * pixels which may be occluded (between far_mid and far) exist, visibility
   * is set to zero, as applying the wisp will have no effect.
   */

  dim = (c->w > c->h? c->w : c->h);
  wx = this->x * dim / BOUNDS;
  wy = this->y * dim / BOUNDS;
  x0 = (wx - (signed)ww/2 > 0?    (coord)(wx - ww/2) : 0);
  x1 = (wx + (signed)ww/2 < xmax? (coord)(wx + ww/2) : (coord)xmax);
  y0 = (wy - (signed)wh/2 > 0?    (coord)(wy - wh/2) : 0);
  y1 = (wy + (signed)wh/2 < ymax? (coord)(wy + wh/2) : (coord)ymax);

  for (y = y0; y < y1; ++y) {
    depth = c->depth + canvas_offset(c, x0, y);
    for (x = x0; x < x1; ++x, ++depth) {
      if (*depth > near && *depth < far) {
        if (*depth < near_mid) {
          zmul = (*depth - near) * imidnear;
        } else if (*depth < far_mid) {
          zmul = fraction_of(1);
        } else {
          zmul = (far - *depth) * ifarmid;
          is_visible = 1;
        }

        visibility = fraction_of(1) -
          (fraction_umul(maxdist - abs(x-wx) + abs(y-wy), zmul)) * imaxdist;

        if (visibility < min_visibility)
          min_visibility = visibility;
      }
    }
  }

  this->visibility = is_visible? min_visibility : 0;
}

static void wisp_apply(canvas*restrict dst, const wisp*restrict this,
                       const parchment*restrict bg,
                       unsigned ww, unsigned wh,
                       signed ymin, signed ymax,
                       coord global_near_mid,
                       coord global_far_mid) {
  coord_offset zshift = fraction_smul(global_far_mid - global_near_mid,
                                      this->z_dist);
  coord far_mid = global_far_mid + zshift;
  coord_offset xmax = dst->w;
  coord_offset wx, wy;
  coord dim, x0, x1, y0, y1, xl, xr, xw, y;

  if (!this->visibility) return;

  dim = (dst->w > dst->h? dst->w : dst->h);
  wx = this->x * dim / BOUNDS;
  wy = this->y * dim / BOUNDS;
  ww = fraction_umul(ww, this->visibility);
  wh = fraction_umul(wh, this->visibility);
  if (!ww/2 || !wh/2) return;

  x0 = (wx - (signed)ww/2 > 0?    (coord)(wx - ww/2) : 0);
  x1 = (wx + (signed)ww/2 < xmax? (coord)(wx + ww/2) : (coord)xmax);
  y0 = (wy - (signed)wh/2 > ymin? (coord)(wy - wh/2) : (coord)ymin);
  y1 = (wy + (signed)wh/2 < ymax? (coord)(wy + wh/2) : (coord)ymax);

  /* TODO: parchment instead of white */
  for (y = y0; y < y1; ++y) {
    xw = fisqrt(ww*ww/4 - ww*ww*(wy-y)*(wy-y)/wh/wh);
    xl = (wx > (signed)(x0 + xw)? wx - xw : x0);
    xr = ((signed)(wx + xw) < (signed)x1? wx + xw : x1);
    parchment_draw_subrow(dst, bg, xl, xr, y, far_mid);
  }
}

#define WISP_OCCLUSION_STRIDE UMP_CACHE_LINE_SZ
static canvas* wisp_apply_dst;
static fog_effect* wisp_apply_this;
static const parchment* wisp_apply_bg;
static coord wisp_apply_near, wisp_apply_near_mid,
  wisp_apply_far_mid, wisp_apply_far;
static void wisp_occlude_stride(unsigned, unsigned);
static void wisp_apply_stripe(unsigned, unsigned);
static ump_task wisp_occlude_task = {
  wisp_occlude_stride,
  0, /* Calculated dynamically */
  0, /* Unused (synchronous) */
};
static ump_task wisp_apply_task = {
  wisp_apply_stripe,
  0, /* Calculated dynamically */
  0, /* Unused (synchronous) */
};

void fog_effect_apply(canvas* dst, fog_effect* this,
                      const parchment* bg,
                      coord near, coord near_mid,
                      coord far_mid, coord far) {
  wisp_apply_dst = dst;
  wisp_apply_this = this;
  wisp_apply_bg = bg;
  wisp_apply_near = near;
  wisp_apply_near_mid = near_mid;
  wisp_apply_far_mid = far_mid;
  wisp_apply_far = far;

  ump_join();
  wisp_occlude_task.num_divisions =
    (this->num_wisps + WISP_OCCLUSION_STRIDE - 1) / WISP_OCCLUSION_STRIDE;
  ump_run_sync(&wisp_occlude_task);
  wisp_apply_task.num_divisions = 1 + ump_num_workers();
  ump_run_sync(&wisp_apply_task);
}

static void wisp_occlude_stride(unsigned ix, unsigned count) {
  unsigned first = ix * WISP_OCCLUSION_STRIDE;
  coord near = wisp_apply_near, near_mid = wisp_apply_near_mid,
    far_mid = wisp_apply_far_mid, far = wisp_apply_far;
  canvas*restrict dst = wisp_apply_dst;
  fog_effect*restrict this = wisp_apply_this;
  coord dim = (dst->w > dst->h? dst->w : dst->h);
  unsigned ww = fraction_umul(dim, this->wisp_w);
  unsigned wh = fraction_umul(dim, this->wisp_h);
  unsigned i;

  for (i = first; i < first+WISP_OCCLUSION_STRIDE &&
         i < this->num_wisps; ++i)
    wisp_occlude(this->wisps+i, dst, ww, wh, near, near_mid, far_mid, far);
}

static void wisp_apply_stripe(unsigned ix, unsigned count) {
  coord near_mid = wisp_apply_near_mid, far_mid = wisp_apply_far_mid;
  canvas*restrict dst = wisp_apply_dst;
  const fog_effect*restrict this = wisp_apply_this;
  const parchment*restrict bg = wisp_apply_bg;
  coord dim = (dst->w > dst->h? dst->w : dst->h);
  unsigned ww = fraction_umul(dim, this->wisp_w);
  unsigned wh = fraction_umul(dim, this->wisp_h);
  unsigned i;
  unsigned ymin = ix * dst->h / count;
  unsigned ymax = (ix+1) * dst->h / count;

  for (i = 0; i < this->num_wisps; ++i)
    wisp_apply(dst, this->wisps+i, bg, ww, wh, ymin, ymax, near_mid, far_mid);
}
