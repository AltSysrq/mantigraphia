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

#include <stddef.h>
#include <stdlib.h>
#include <string.h>

#include "../alloc.h"
#include "../frac.h"
#include "../rand.h"
#include "../simd.h"
#include "canvas.h"
#include "abstract.h"
#include "brush.h"
#include "tscan.h"
#include "fast-brush.h"

#define FB_SPLOTCH_DIM (4*BRUSH_SPLOTCH_DIM)

/* Typed so it can be used by simd instructions. We add an additional garbage
 * row at the bottom so that we can safely read past the end of the texture
 * boundaries.
 */
static signed fast_brush_splotches
[NUM_BRUSH_SPLOTCHES][FB_SPLOTCH_DIM*(FB_SPLOTCH_DIM+1)];

void fast_brush_load(void) {
  unsigned splotch, i, x, y, sx, sy;
  const unsigned char* src, * variant;
  signed* dst;
  signed tmp[BRUSH_SPLOTCH_DIM*BRUSH_SPLOTCH_DIM];

  for (splotch = 0; splotch < NUM_BRUSH_SPLOTCHES; ++splotch) {
    src = brush_splotches[splotch];
    variant = brush_splotches[(splotch+1) % NUM_BRUSH_SPLOTCHES];
    dst = tmp;

    for (i = 0; i < BRUSH_SPLOTCH_DIM * BRUSH_SPLOTCH_DIM;
         ++i, ++src, ++variant, ++dst) {
      if (*src >= MAX_BRUSH_BRISTLES) {
        *dst = 255;
      } else {
        *dst = !!(*src - MAX_BRUSH_BRISTLES/2)
             + !!((*variant - MAX_BRUSH_BRISTLES/2) / 8)
             + (brush_splotches[rand() % NUM_BRUSH_SPLOTCHES][i] & 1);
      }
    }

    for (y = 0; y < FB_SPLOTCH_DIM; ++y) {
      for (x = 0; x < FB_SPLOTCH_DIM; ++x) {
        sx = x / (FB_SPLOTCH_DIM/BRUSH_SPLOTCH_DIM);
        sy = y / (FB_SPLOTCH_DIM/BRUSH_SPLOTCH_DIM);
        if ((rand() & 3) < (x & 3) && sx+1 < BRUSH_SPLOTCH_DIM)
          ++sx;
        if ((rand() & 3) < (y & 3) && sy+1 < BRUSH_SPLOTCH_DIM)
          ++sy;

        fast_brush_splotches[splotch][y*FB_SPLOTCH_DIM+x] =
          tmp[sy*BRUSH_SPLOTCH_DIM+sx];
      }
    }
  }
}

typedef struct {
  drawing_method meth;

  /**
   * The width of the texture, in pixels.
   */
  unsigned width;
  /**
   * The length of the texture, in pixels.
   */
  unsigned length;

  /**
   * The texture, as pallet indices. This is not written to during normal
   * operation, so it needs no particular alignment.
   */
  unsigned char texture[1];
} fast_brush;

static void fast_brush_draw_line(fast_brush_accum*restrict,
                                 const fast_brush*restrict,
                                 const vo3, zo_scaling_factor,
                                 const vo3, zo_scaling_factor);
static void fast_brush_draw_point(fast_brush_accum*restrict,
                                  const fast_brush*restrict,
                                  const vo3, zo_scaling_factor);
static void fast_brush_flush(fast_brush_accum*restrict,
                             const fast_brush*restrict);

drawing_method* fast_brush_new(const brush_spec* orig_brush,
                               coord max_width,
                               coord max_length,
                               unsigned brush_random_seed) {
  brush_spec brush;
  brush_accum baccum;
  canvas* tmp;
  canvas_pixel bluescale_ramp[256], blue;
  vo3 top, bottom;
  unsigned x, y;
  fast_brush* this;

  this = xmalloc(
    offsetof(fast_brush, texture) +
    max_width * max_length);

  for (blue = 0; blue <= 0xFF; ++blue)
    bluescale_ramp[blue] = blue;

  this->meth.draw_line  = (drawing_method_draw_line_t )fast_brush_draw_line;
  this->meth.draw_point = (drawing_method_draw_point_t)fast_brush_draw_point;
  this->meth.flush      = (drawing_method_flush_t     )fast_brush_flush;
  this->width = max_width;
  this->length = max_length;

  /* Allocate a temporary canvas into which to render the bluescale-index
   * version of the brush.
   */
  tmp = canvas_new(max_width, max_length);
  canvas_clear(tmp);
  /* Set everything to the maximum index initially */
  memset(tmp->px, ~0, sizeof(canvas_pixel) * tmp->pitch * tmp->h);

  /* Create a local copy of the input brush, with a bluescale pallet that we
   * can trivially convert to the indexed texture.
   */
  memcpy(&brush, orig_brush, sizeof(brush));
  brush.colours = bluescale_ramp;
  brush.num_colours = 256;
  brush.size = ZO_SCALING_FACTOR_MAX;
  brush_prep(&baccum, &brush, tmp, brush_random_seed);

  /* Draw the brush into the scratch canvas */
  top[0] = max_width / 2;
  /* Advance a bit into the canvas so that the initial splotch is shown
   * correctly.
   */
  top[1] = max_width / 6;
  top[2] = 0;
  bottom[0] = max_width / 2;
  bottom[1] = max_length;
  bottom[2] = 0;
  brush_draw_line(&baccum, &brush, top, ZO_SCALING_FACTOR_MAX,
                  bottom, ZO_SCALING_FACTOR_MAX);
  /* Don't flush so that we don't get a splotch at the end */

  /* Copy blue channel of result into our index table */
  for (y = 0; y < max_length; ++y)
    for (x = 0; x < max_width; ++x)
      this->texture[y*max_width + x] =
        tmp->px[canvas_offset(tmp, x, y)] & 0xFF;

  /* Release temporary resources and we're done */
  canvas_delete(tmp);
  return (drawing_method*)this;
}

void fast_brush_delete(drawing_method* this) {
  free(this);
}

void fast_brush_flush(fast_brush_accum*restrict accum,
                      const fast_brush*restrict this) {
  accum->distance = 0;
  accum->random = accum->random_seed;
}

void fast_brush_draw_point(fast_brush_accum*restrict accumptr,
                           const fast_brush*restrict this,
                           const vo3 where, zo_scaling_factor size_scale) {
  fast_brush_accum accum = *accumptr;
  signed size = zo_scale(accum.dst->logical_width, size_scale);
  signed sizemul;
  coord_offset ax0, ax1, ay0, ay1;
  coord_offset x0, x1, y0, y1;
  coord_offset x, y, tx, ty;
  coord z;
  unsigned texix, colourix;
  const signed*restrict texture;
  canvas_depth*restrict depth;
  canvas_pixel*restrict px;
  const simd4 v0123 = simd_init4(0,1,2,3);
  simd4 tx4, z4, depth4, num_colours4, colour4;
  simd4 depth_test4, colour_test4, pallet;
  simd4 texs;
  unsigned i;

  if (!size) return;
  sizemul = ZO_SCALING_FACTOR_MAX * FB_SPLOTCH_DIM / size;

  ax0 = where[0] - size/2;
  ax1 = ax0 + size;
  ay0 = where[1] - size/2;
  ay1 = ay0 + size;

  x0 = (ax0 >= 0? ax0 : 0);
  x1 = (ax1 <= (signed)accum.dst->w? ax1 : (signed)accum.dst->w);
  y0 = (ay0 >= 0? ay0 : 0);
  y1 = (ay1 <= (signed)accum.dst->h? ay1 : (signed)accum.dst->h);

  texix = lcgrand(&accumptr->random) % NUM_BRUSH_SPLOTCHES;
  texture = fast_brush_splotches[texix];

  z = where[2];
  z4 = simd_inits(z);
  num_colours4 = simd_inits(accum.num_colours);
  pallet = simd_initl(accum.num_colours > 0? accum.colours[0] : 0,
                      accum.num_colours > 1? accum.colours[1] : 0,
                      accum.num_colours > 2? accum.colours[2] : 0,
                      accum.num_colours > 3? accum.colours[3] : 0);

  for (y = y0; y < y1; ++y) {
    ty = (y-ay0) * sizemul / ZO_SCALING_FACTOR_MAX;
    depth = accum.dst->depth + canvas_offset(accum.dst, x0, y);
    px = accum.dst->px + canvas_offset(accum.dst, x0, y);
    texture = fast_brush_splotches[texix] + ty * FB_SPLOTCH_DIM;

    for (x = x0; x < x1 && (x & 3); ++x, ++depth, ++px) {
      /* one at a time */
      if (z >= *depth) continue;

      tx = (x - ax0) * sizemul / ZO_SCALING_FACTOR_MAX;

      colourix = texture[tx];

      if (colourix < accum.num_colours) {
        *px = accum.colours[colourix];
        *depth = z;
      }
    }

    for (; x+4 <= x1 && sizemul < 2*ZO_SCALING_FACTOR_MAX;
         x += 4, depth += 4, px += 4) {
      /* four at a time; requires that texture is not being scaled down by
       * more than a factor of 2.
       */
      depth4 = simd_of_aligned(depth);
      depth_test4 = simd_pairwise_lt(z4, depth4);

      /* In the majority of cases, either all four pixels fail the test, or all
       * four pass.
       */
      if (simd_all_false(depth_test4)) continue;

      tx = (x-ax0) * sizemul / ZO_SCALING_FACTOR_MAX;
      texs = simd_of_vo4(texture + tx);
      tx4 = simd_shra(simd_mulvs(v0123, sizemul), ZO_SCALING_FACTOR_BITS);
      colour4 = simd_shuffle(texs, tx4);
      colour_test4 = simd_pairwise_lt(colour4, num_colours4);
      if (simd_all_false(colour_test4)) continue;

      if (simd_all_true(depth_test4) && simd_all_true(colour_test4)) {
        simd_store_aligned(depth, z4);
        simd_store_aligned(px, simd_shuffle(pallet, colour4));
      } else {
        for (i = 0; i < 4; ++i) {
          if (simd_vs(depth_test4, i) && simd_vs(colour_test4, i)) {
            depth[i] = z;
            px[i] = simd_vs(pallet, simd_vs(colour4, i));
          }
        }
      }
    }

    for (; x < x1; ++x, ++depth, ++px) {
      /* one at a time */
      if (z >= *depth) continue;

      tx = (x - ax0) * sizemul / ZO_SCALING_FACTOR_MAX;

      colourix = texture[tx];

      if (colourix < accum.num_colours) {
        *px = accum.colours[colourix];
        *depth = z;
      }
    }
  }
}

#define INTERP_ZSCALE 64
typedef struct {
  fast_brush_accum*restrict accum;
  const fast_brush*restrict this;
  coord base_z;
} fast_brush_data;

static inline void fast_brush_pixel_shader(
  const fast_brush_data*restrict d,
  coord_offset x, coord_offset y,
  usimd8s interps)
{
  unsigned short tx = simd_vs(interps, 0), ty = simd_vs(interps, 1);
  unsigned z = simd_vs(interps, 2);

  unsigned char ix = d->this->texture[tx + ty*d->this->width];

  z *= INTERP_ZSCALE;
  z += d->base_z;

  if (ix < d->accum->num_colours)
    canvas_write(d->accum->dst, x, y, d->accum->colours[ix], z);
}

SHADE_TRIANGLE(fast_brush_triangle_shader, fast_brush_pixel_shader)

void fast_brush_draw_line(fast_brush_accum*restrict accum,
                          const fast_brush*restrict this,
                          const vo3 from, zo_scaling_factor from_size_scale,
                          const vo3 to, zo_scaling_factor to_size_scale) {
  fast_brush_data data = { accum, this, 0 };
  vo3 delta;
  coord_offset v00[2], v01[2], v10[2], v11[2];
  coord_offset xoff, yoff;
  usimd8s z00, z01, z10, z11;
  signed length;

  delta[0] = from[0] - to[0];
  delta[1] = from[1] - to[1];
  delta[2] = 0;
  length = omagnitude(delta);

  /* Nothing to do if zero-magnitude, or if length is greater than maximum. */
  if (!length || length > (signed)this->length) return;

  /* Ensure that this length can fit */
  if (accum->distance + length >= this->length)
    accum->distance = this->length - length;

  data.base_z = (from[2] < to[2]? from[2] : to[2]);
  simd_vs(z00,0) = 0;
  simd_vs(z00,1) = accum->distance;
  simd_vs(z00,2) = (from[2] - data.base_z) / INTERP_ZSCALE;
  simd_vs(z01,0) = this->width;
  simd_vs(z01,1) = accum->distance;
  simd_vs(z01,2) = (from[2] - data.base_z) / INTERP_ZSCALE;
  simd_vs(z10,0) = 0;
  simd_vs(z10,1) = accum->distance + length;
  simd_vs(z10,2) = (to[2] - data.base_z) / INTERP_ZSCALE;
  simd_vs(z11,0) = this->width;
  simd_vs(z11,1) = accum->distance + length;
  simd_vs(z11,2) = (to[2] - data.base_z) / INTERP_ZSCALE;

  accum->distance += length;

  xoff = - ((signed)(accum->dst->logical_width * delta[1])) / 2 / length;
  yoff = + ((signed)(accum->dst->logical_width * delta[0])) / 2 / length;
  v00[0] = from[0] - zo_scale(xoff, from_size_scale);
  v00[1] = from[1] - zo_scale(yoff, from_size_scale);
  v01[0] = from[0] + zo_scale(xoff, from_size_scale);
  v01[1] = from[1] + zo_scale(yoff, from_size_scale);
  v10[0] = to  [0] - zo_scale(xoff,   to_size_scale);
  v10[1] = to  [1] - zo_scale(yoff,   to_size_scale);
  v11[0] = to  [0] + zo_scale(xoff,   to_size_scale);
  v11[1] = to  [1] + zo_scale(yoff,   to_size_scale);
  fast_brush_triangle_shader(accum->dst,
                             v00, z00,
                             v01, z01,
                             v10, z10,
                             &data);
  fast_brush_triangle_shader(accum->dst,
                             v01, z01,
                             v10, z10,
                             v11, z11,
                             &data);
}
