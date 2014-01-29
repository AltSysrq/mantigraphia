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
#include "canvas.h"
#include "abstract.h"
#include "brush.h"
#include "tscan.h"
#include "fast-brush.h"

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

void fast_brush_draw_point(fast_brush_accum*restrict accum,
                           const fast_brush*restrict this,
                           const vo3 where, zo_scaling_factor size_scale) {
  /* TODO */
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
  unsigned length;

  delta[0] = from[0] - to[0];
  delta[1] = from[1] - to[1];
  delta[2] = from[2] - to[2];
  length = omagnitude(delta);

  /* Nothing to do if zero-magnitude */
  if (!length) return;

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

  xoff = - accum->dst->logical_width * delta[1] / 2 / length;
  yoff = + accum->dst->logical_width * delta[0] / 2 / length;
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
