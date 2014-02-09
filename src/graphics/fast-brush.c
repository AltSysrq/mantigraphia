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

static unsigned char fast_brush_splotches
[NUM_BRUSH_SPLOTCHES][BRUSH_SPLOTCH_DIM*BRUSH_SPLOTCH_DIM];

void fast_brush_load(void) {
  unsigned splotch, i;
  const unsigned char* src, * variant;
  unsigned char* dst;

  for (splotch = 0; splotch < NUM_BRUSH_SPLOTCHES; ++splotch) {
    src = brush_splotches[splotch];
    variant = brush_splotches[(splotch+1) % NUM_BRUSH_SPLOTCHES];
    dst = fast_brush_splotches[splotch];

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
  top[1] = 0;
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
  const unsigned char*restrict texture;
  canvas_depth*restrict depth;
  canvas_pixel*restrict px;
  simd4 x4, z4, tx4, depth4, num_colours4, colour4;
  simd4 depth_test4, colour_test4, pallet;
  unsigned i;

  if (!size) return;
  sizemul = ZO_SCALING_FACTOR_MAX * BRUSH_SPLOTCH_DIM / size;

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
    texture = fast_brush_splotches[texix] + ty * BRUSH_SPLOTCH_DIM;

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

    for (; x+4 <= x1; x += 4, depth += 4, px += 4) {
      /* four at a time */
      x4 = simd_initl(x+0-ax0, x+1-ax0, x+2-ax0, x+3-ax0);
      depth4 = simd_of_aligned(depth);
      depth_test4 = simd_pairwise_lt(z4, depth4);

      /* In the majority of cases, either all four pixels fail the test, or all
       * four pass.
       */
      if (simd_all_false(depth_test4)) continue;

      tx4 = simd_shra(simd_mulvs(x4, sizemul), ZO_SCALING_FACTOR_BITS);
      colour4 = simd_initl(texture[simd_vs(tx4, 0)],
                           texture[simd_vs(tx4, 1)],
                           texture[simd_vs(tx4, 2)],
                           texture[simd_vs(tx4, 3)]);
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

typedef struct {
  fast_brush_accum*restrict accum;
  const fast_brush*restrict this;
} fast_brush_data;

typedef struct {
  coord_offset tx, ty, z;
} fast_brush_interps;

static inline void fast_brush_pixel_shader(
  const fast_brush_data*restrict d,
  coord_offset x, coord_offset y,
  const coord_offset*restrict vi)
{
  const fast_brush_interps*restrict i = (const fast_brush_interps*)vi;
  unsigned char ix = d->this->texture[i->tx + i->ty*d->this->width];
  if (ix < d->accum->num_colours)
    canvas_write(d->accum->dst, x, y, d->accum->colours[ix], i->z);
}

SHADE_TRIANGLE(fast_brush_triangle_shader, fast_brush_pixel_shader,
               sizeof(fast_brush_interps) / sizeof(coord_offset))

void fast_brush_draw_line(fast_brush_accum*restrict accum,
                          const fast_brush*restrict this,
                          const vo3 from, zo_scaling_factor from_size_scale,
                          const vo3 to, zo_scaling_factor to_size_scale) {
  fast_brush_data data = { accum, this };
  vo3 delta;
  coord_offset v00[2], v01[2], v10[2], v11[2];
  coord_offset xoff, yoff;
  fast_brush_interps z00, z01, z10, z11;
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

  z00.tx = 0;
  z00.ty = accum->distance;
  z00.z = from[2];
  z01.tx = this->width;
  z01.ty = accum->distance;
  z01.z = from[2];
  z10.tx = 0;
  z10.ty = accum->distance + length;
  z10.z = to[2];
  z11.tx = this->width;
  z11.ty = accum->distance + length;
  z11.z = to[2];

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
                             v00, (const coord_offset*)&z00,
                             v01, (const coord_offset*)&z01,
                             v10, (const coord_offset*)&z10,
                             &data);
  fast_brush_triangle_shader(accum->dst,
                             v01, (const coord_offset*)&z01,
                             v10, (const coord_offset*)&z10,
                             v11, (const coord_offset*)&z11,
                             &data);
}
