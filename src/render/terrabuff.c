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
#define TEXTURE_REPETITION (16384/TEXSZ)

/**
 * Texture to use for drawing. This is actually column-major, since drawing
 * functions need to traverse it vertically. It is repeated TEXTURE_REPETITION
 * times so that the shader can blindly increment a pointer to it and never go
 * off the edge.
 */
static canvas_pixel texture[TEXSZ*TEXSZ*TEXTURE_REPETITION];

void terrabuff_init(void) {
  unsigned i;

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
                           /* Inverted parm order due to column-major order */
                           1, TEXSZ/4,
                           pallet, lenof(pallet));

  for (i = 1; i < TEXTURE_REPETITION; ++i)
    memcpy(texture + i*TEXSZ*TEXSZ, texture, TEXSZ*TEXSZ*sizeof(canvas_pixel));
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

  terrabuff_slice next_low, next_high;

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
  this->boundaries[0].low = 0;
  this->boundaries[1].high = (r-l) & (this->scap-1);
}

int terrabuff_next(terrabuff* this, terrabuff_slice* l, terrabuff_slice* r) {
  terrabuff_slice low = this->next_low;
  terrabuff_slice high = this->next_high;
  if (low > 2)
    low -= 2;
  else
    low = 0;

  this->scurr = low;
  *l = (low  + this->soff) & (this->scap-1);
  *r = (high + this->soff) & (this->scap-1);
  ++this->scan;

  this->boundaries[this->scan].low = low;
  this->boundaries[this->scan].high = high;
  this->next_low = low;
  this->next_high = high;

  return low + 4 < high;
}

void terrabuff_put(terrabuff* this, const vo3 where, coord_offset xmax) {
  /* Update boundaries */
  if (where[0] < 0) {
    this->next_low = this->scurr;
  } else if (where[0] < xmax) {
    this->next_high = this->scurr+3;
  }

  memcpy(this->points[this->scan*this->scap + this->scurr], where, sizeof(vo3));
  ++this->scurr;
}

typedef struct {
  coord_offset y, z;
} screen_yz;

#ifdef PROFILE
/* Prevent inlining of our static functions so that we can get profiling data
 * on each individual one.
 */
static void interpolate(screen_yz*restrict, vo3*restrict, coord_offset)
__attribute__((noinline));
static void interpolate_all(screen_yz*restrict, vo3*restrict,
                            unsigned, coord_offset)
__attribute__((noinline));
static void draw_line(canvas*restrict, const screen_yz*restrict,
                      unsigned, unsigned, signed)
__attribute__((noinline));
static void draw_line_with_thickness(canvas*restrict, const screen_yz*restrict,
                                     unsigned, unsigned, unsigned)
__attribute__((noinline));
static void draw_segments(canvas*restrict,
                          char*restrict,
                          const screen_yz*restrict,
                          const screen_yz*restrict,
                          unsigned)
__attribute__((noinline));
static void fill_area_between(canvas*restrict,
                              const screen_yz*restrict,
                              const screen_yz*restrict,
                              coord_offset)
__attribute__((noinline));
static void collapse_buffer(char*restrict,
                            screen_yz*restrict, const screen_yz*restrict,
                            unsigned)
__attribute__((noinline));
#endif /* PROFILE */

static void interpolate(screen_yz*restrict dst, vo3*restrict points,
                        coord_offset xmax) {
  /* Use a Catmull-Rom spline for Y, linear for Z */
  coord_offset x0 = points[1][0], x1 = points[2][0];
  coord_offset y0 = points[1][1], y1 = points[2][1];
  coord_offset z0 = points[1][2], z1 = points[2][2];
  coord_offset xl = (x0 >= 0? x0 : 0), xh = (x1 < xmax? x1 : xmax);
  coord_offset dx = x1 - x0;
  coord_offset x;
  coord_offset m0n, m1n;
  precise_fraction pidx, t1, t2, t3, m0d, m1d;
  fraction idx;

  if (!dx) {
    if (x0 >=0 && x0 < xmax) {
      dst[x0].y = y0;
      dst[x0].z = z0;
    }
    return;
  }

  /* Clamp y values to maximum range allowed by precise_fraction. */
  y0 = clamps(-32768, y0, +32767);
  y1 = clamps(-32768, y1, +32767);

  /* Since here we know that the X of 1 and 2 are inequal, we also know that
   * those of 0 and 2, as well as 1 and 3, are inequal.
   *
   * We calculate here the tangents m0 and m1 as rationals, ie, m0 = m0n/m0d.
   */
  m0d = precise_fraction_of(points[2][0] - points[0][0]);
  m1d = precise_fraction_of(points[3][0] - points[1][0]);
  m0n = clamps(-32768, points[2][1] - points[0][1], +32767);
  m1n = clamps(-32768, points[3][1] - points[1][1], +32767);

  pidx = precise_fraction_of(dx);
  idx = fraction_of(dx);
  for (x = xl; x <= xh; ++x) {
    t1 = (x-x0) * pidx;
    t2 = precise_fraction_fmul(t1, t1);
    t3 = precise_fraction_fmul(t2, t1);

    dst[x].y = precise_fraction_sred(
      + 2 * precise_fraction_smul(y0, t3)
      - 3 * precise_fraction_smul(y0, t2)
      + precise_fraction_sexp(y0)
      + precise_fraction_smul(
        precise_fraction_sred(
          +     precise_fraction_smul(m0n, t3)
          - 2 * precise_fraction_smul(m0n, t2)
          +     precise_fraction_smul(m0n, t1)), m0d)
      - 2 * precise_fraction_smul(y1, t3)
      + 3 * precise_fraction_smul(y1, t2)
      + precise_fraction_smul(
        precise_fraction_sred(
          + precise_fraction_smul(m1n, t3)
          - precise_fraction_smul(m1n, t2)), m1d));
    dst[x].z = fraction_smul((x-x0)*z1 + (x1-x)*z0, idx);
  }
}

static void interpolate_all(screen_yz*restrict dst,
                            vo3*restrict points,
                            unsigned num_points,
                            coord_offset xmax) {
  unsigned i;

  for (i = 1; i < num_points-2 && points[i][0] < xmax; ++i)
    if (points[i+1][0] >= 0)
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
                          char*restrict line_points,
                          const screen_yz*restrict front,
                          const screen_yz*restrict back,
                          unsigned thickness) {
  unsigned x0, x1;

  for (x0 = 0; x0 < dst->w; ++x0) {
    if (back[x0].y <= front[x0].y && !line_points[x0]) {
      line_points[x0] = 1;
      x1 = x0+1;
      while (x1 < dst->w && back[x1].y <= front[x1].y && !line_points[x1]) {
        line_points[x1] = 1;
        ++x1;
      }

      draw_line_with_thickness(dst, back, x0, x1, thickness);
      x0 = x1;
      line_points[x1] = 1;
    }
  }
}

static void fill_area_between(canvas*restrict dst,
                              const screen_yz*restrict front,
                              const screen_yz*restrict back,
                              coord_offset tex_x_off) {
  coord_offset y, y0, y1;
  unsigned x;
  register unsigned w = dst->w;
  register canvas_pixel*restrict px;
  register const canvas_pixel*restrict tex;
  register unsigned*restrict depth;

  for (x = 0; x < dst->w; ++x) {
    if (front[x].y < back[x].y) {
      y0 = front[x].y;
      y1 = back[x].y;
      if (y0 < 0) y0 = 0;
      if (y1 >= (signed)dst->h) y1 = (signed)dst->h-1;

      px = dst->px + canvas_offset(dst, x, y0);
      depth = dst->depth + canvas_offset(dst, x, y0);
      tex = texture + ((y0-front[x].y) & TEXMASK) +
        TEXSZ*((x+tex_x_off) & TEXMASK);

      for (y = y0; y <= y1; ++y) {
        *px = *tex;
        *depth = front[x].z;

        px += w;
        depth += w;
        ++tex;
      }
    }
  }
}

static void collapse_buffer(char* line_points,
                            screen_yz*restrict dst,
                            const screen_yz*restrict src,
                            unsigned xmax) {
  unsigned x;

  for (x = 0; x < xmax; ++x) {
    if (src[x].y < dst[x].y) {
      dst[x].y = src[x].y;
      dst[x].z = src[x].z;
      line_points[x] = 0;
    }
  }
}

void terrabuff_render(canvas*restrict dst,
                      const terrabuff*restrict this,
                      const rendering_context*restrict ctxt) {
  const rendering_context_invariant*restrict context =
    (const rendering_context_invariant*restrict)ctxt;
  screen_yz lbuff_front[dst->w+1], lbuff_back[dst->w+1];
  /* Track points (X coordinates) that already have lines. This way, we can
   * avoid drawing the same line segment over and over.
   */
  char line_points[dst->w];
  unsigned scan, x, line_thickness;
  coord_offset texture_x_offset;

  texture_x_offset = (-(signed)dst->w) * 314159 / 200000 *
    context->long_yrot / context->proj->fov;
  line_thickness = 1 + dst->h / 1024;
  memset(line_points, 0, dst->w);

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

    fill_area_between(dst, lbuff_front, lbuff_back,
                      texture_x_offset + 31*scan*scan + 75*scan);
    draw_segments(dst, line_points, lbuff_front, lbuff_back, line_thickness);

    collapse_buffer(line_points, lbuff_back, lbuff_front, dst->w);
  }

  draw_line_with_thickness(dst, lbuff_back, 0, dst->w, line_thickness);
}
