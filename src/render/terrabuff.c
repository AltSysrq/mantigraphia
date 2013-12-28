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

#include "../coords.h"
#include "../alloc.h"
#include "../defs.h"
#include "../frac.h"
#include "../graphics/canvas.h"
#include "../graphics/linear-paint-tile.h"
#include "../graphics/perspective.h"
#include "context.h"
#include "terrabuff.h"

#define TEXSZ 256
#define TEXMASK (TEXSZ-1)

static canvas_pixel texture[TEXSZ*TEXSZ];

void terrabuff_init(void) {
  static const canvas_pixel pallet[] = {
    argb(255,255,255,255),
    argb(255,254,254,254),
    argb(255,252,252,252),
    argb(255,248,248,248),
    argb(255,240,240,240),
    argb(255,224,224,224),
    argb(255,192,192,192),
    argb(255,128,128,128),
    argb(255,64,64,64),
    argb(255,0,0,0),
  };

  linear_paint_tile_render(texture, TEXSZ, TEXSZ,
                           TEXSZ/4, 1,
                           pallet, lenof(pallet));
}

/**
 * Tracks the boundaries of relevant slices at a given scan. The low index is
 * the last point to the left of the canvas, whereas the right index is one
 * past the first point to the right of the drawable area.
 */
typedef struct {
  terrabuff_slice low, high;
} scan_boundary;

struct terrabuff_s {
  /**
   * The capacity of this buffer.
   */
  terrabuff_slice scap;
  /**
   * While performing a scan, the current slice being scanned.
   */
  terrabuff_slice scurr;
  /**
   * The offset of the slice number that the client believes it is
   * using. Internall, all slices are zero-based from this offset, allowing
   * point memory to always be contiguous.
   */
  terrabuff_slice soff;

  /**
   * The current scan index.
   */
  unsigned scan;
  /**
   * Boundaries calculated on each scan performed. Indexed by scan. Part of the
   * same memory allocation as the terrabuff itself.
   */
  scan_boundary*restrict boundaries;
  /**
   * Points written into the terrabuff. Indexed by (scan*scap+scurr). Part of
   * the same memory allocation as the terrabuff itself.
   */
  vo3*restrict points;
};

terrabuff* terrabuff_new(terrabuff_slice scap, unsigned scancap) {
  terrabuff* this = xmalloc(sizeof(terrabuff) +
                            sizeof(scan_boundary) * scancap +
                            sizeof(vo3) * scap * scancap);
  this->scap = scap;
  this->boundaries = (scan_boundary*)(this + 1);
  this->points = (vo3*)(this->boundaries + scancap);

  return this;
}

void terrabuff_delete(terrabuff* this) {
  free(this);
}

void terrabuff_clear(terrabuff* this, terrabuff_slice l, terrabuff_slice r) {
  this->scan = 0;
  this->scurr = 0;
  this->soff = l;
}

int terrabuff_next(terrabuff* this, terrabuff_slice* l, terrabuff_slice* r) {
  this->scurr = this->boundaries[this->scan].low;
  *l = (this->boundaries[this->scan].low  + this->soff) & (this->scap-1);
  *r = (this->boundaries[this->scan].high + this->soff) & (this->scap-1);
  ++this->scan;

  return this->boundaries[this->scan-1].low + 4 <
    this->boundaries[this->scan-1].high;
}

void terrabuff_put(terrabuff* this, const vo3 where, coord_offset xmax) {
  /* Update boundaries */
  if (where[0] < 0)
    this->boundaries[this->scan].low = (this->scurr-1) & (this->scap-1);
  else if (where[0] < xmax)
    this->boundaries[this->scan].high = (this->scurr+2) & (this->scap-1);

  memcpy(this->points[this->scan*this->scap + this->scurr], where, sizeof(vo3));
  ++this->scurr;
}

typedef struct {
  coord_offset y, z;
} screen_yz;

static void interpolate(screen_yz*restrict dst, vo3*restrict points,
                        coord_offset xmax) {
  /* TODO: Better interpolation than linear (ie, cubic) */
  coord_offset x0 = points[1][0], x1 = points[2][0];
  coord_offset y0 = points[1][1], y1 = points[2][1];
  coord_offset z0 = points[1][2], z1 = points[2][2];
  coord_offset xl = (x0 >= 0? x0 : 0), xh = (x1 < xmax? x1 : xmax);
  coord_offset dx = x1 - x0;
  coord_offset x;
  fraction idx;

  if (!dx) {
    if (x0 >=0 && x0 < xmax) {
      dst[x0].y = y0;
      dst[x0].z = z0;
    }
    return;
  }

  idx = fraction_of(dx);

  for (x = xl; x <= xh; ++x) {
    dst[x].y = fraction_smul(y0*(x1-x) + y1*(x-x0), idx);
    dst[x].z = fraction_smul(z0*(x1-x) + z1*(x0-x), idx);
  }
}

static void interpolate_all(screen_yz*restrict dst,
                            vo3*restrict points,
                            unsigned num_points,
                            coord_offset xmax) {
  unsigned i;

  for (i = 1; i < num_points-1; ++i)
    interpolate(dst, points + (i-1), xmax);
}

#define LINE_COLOUR (argb(255,12,12,12))
static void draw_line(canvas*restrict dst,
                      const screen_yz*restrict along,
                      unsigned x0, unsigned x1,
                      signed yoff) {
  unsigned x, off;
  coord_offset y0, y1, yl, yh, y;

  for (x = x0; x < x1; ++x) {
    y0 = along[x].y + yoff;
    y1 = along[x+1].y + yoff;
    if (y0 < y1) {
      yl = (y0 > 0? y0 : 0);
      yh = (y1 < (signed)dst->h? y1 : (signed)dst->h-1);
    } else {
      yl = (y1 > 0? y0 : 0);
      yh = (y0 < (signed)dst->h? y0 : (signed)dst->h-1);
    }

    for (y = yl; y <= yh; ++y) {
      off = canvas_offset(dst, x, y);
      dst->px[off] = LINE_COLOUR;
      dst->depth[off] = along[x].z;
    }
  }
}

static void draw_line_with_thickness(canvas*restrict dst,
                                     const screen_yz*restrict along,
                                     unsigned x0, unsigned x1,
                                     unsigned thickness) {
  unsigned t;

  draw_line(dst, along, x0, x1, 0);

  for (t = 0; t < thickness && t < x1; ++t) {
    draw_line(dst, along, x0+t, x1-t, +(signed)t);
    draw_line(dst, along, x0+t, x1-t, -(signed)t);
  }
}

static void draw_segments(canvas*restrict dst,
                          const screen_yz*restrict front,
                          const screen_yz*restrict back,
                          unsigned thickness) {
  unsigned x0, x1;

  for (x0 = 0; x0 < dst->w; ++x0) {
    if (back[x0].y <= front[x0].y) {
      x1 = x0+1;
      while (x1 < dst->w && back[x1].y >= front[x0].y)
        ++x1;

      draw_line_with_thickness(dst, back, x0, x1, thickness);
      x0 = x1;
    }
  }
}

static void fill_area_between(canvas*restrict dst,
                              const screen_yz*restrict front,
                              const screen_yz*restrict back) {
  coord_offset y, y0, y1;
  unsigned x, off;

  for (x = 0; x < dst->w; ++x) {
    if (front[x].y < back[x].y) {
      y0 = front[x].y;
      y1 = back[x].y;
      if (y0 < 0) y0 = 0;
      if (y1 >= (signed)dst->h) y1 = (signed)dst->h-1;

      for (y = y0; y <= y1; ++y) {
        off = canvas_offset(dst, x, y);
        dst->px[off] = texture[
          TEXSZ*((y-front[x].y) & TEXMASK) +
          (x & TEXMASK)];
        dst->depth[off] = front[x].z;
      }
    }
  }
}

static void collapse_buffer(screen_yz*restrict dst,
                            const screen_yz*restrict src,
                            unsigned xmax) {
  unsigned x;

  for (x = 0; x < xmax; ++x) {
    if (src[x].y < dst[x].y) {
      dst[x].y = src[x].y;
      dst[x].z = src[x].z;
    }
  }
}

void terrabuff_render(canvas*restrict dst,
                      const terrabuff*restrict this,
                      const rendering_context*restrict ctxt) {
  const rendering_context_invariant*restrict context =
    (const rendering_context_invariant*restrict)ctxt;
  screen_yz lbuff_front[dst->w+1], lbuff_back[dst->w+1];
  unsigned scan, x, line_thickness;

  line_thickness = 1 + dst->h / 1024;

  /* Render from the bottom up. First, initialise the front-yz buffer to have
   * the minimum Y coordinate at all points, so that the bottom level never
   * interacts.
   *
   * Then, for each iteration, interpolate the new scan into the front
   * buffer. Fill the areas where the front buffer Y is less than (above) the
   * back buffer Y. Then draw line segments along the back buffer where its Y
   * is less than or equal to the front buffer. Finally, collapse the front
   * buffer into the back buffer by selecting lesser Y coordinates.
   *
   * When done, draw a line across the entire back buffer to add contrast to
   * the sky and to be consistent with how the rest of the drawing looks.
   */
  for (x = 0; x <= dst->w; ++x) {
    lbuff_back[x].y = 0x7FFFFFFF;
    lbuff_back[x].z = 0x7FFFFFFF;
  }
  for (scan = 0; scan < this->scan-1; ++scan) {
    interpolate_all(lbuff_front,
                    this->points + this->scap*scan + this->boundaries[scan].low,
                    this->boundaries[scan].high - this->boundaries[scan].low,
                    dst->w);

    fill_area_between(dst, lbuff_front, lbuff_back);
    draw_segments(dst, lbuff_front, lbuff_back, line_thickness);

    collapse_buffer(lbuff_back, lbuff_front, dst->w);
  }

  draw_line_with_thickness(dst, lbuff_back, 0, dst->w, line_thickness);
}
