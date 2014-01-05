/*-
 * Copyright (c) 2013, 2014 Jason Lingle
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

#include <assert.h>
#include <string.h>

#include "../coords.h"
#include "../alloc.h"
#include "../defs.h"
#include "../frac.h"
#include "../micromp.h"
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
 * the pennultimate point to the left of the canvas, whereas the right index is
 * two past the first point to the right of the drawable area.
 */
typedef struct {
  terrabuff_slice low, high;
} scan_boundary;

typedef struct {
  vo3 where;
  canvas_pixel colour;
} scan_point;

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
  scan_point*restrict points;
};

terrabuff* terrabuff_new(terrabuff_slice scap, unsigned scancap) {
  terrabuff* this = xmalloc(sizeof(terrabuff) +
                            sizeof(scan_boundary) * scancap +
                            sizeof(scan_point) * scap * scancap);
  this->scap = scap;
  this->boundaries = (scan_boundary*)(this + 1);
  this->points = (scan_point*)(this->boundaries + scancap);

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
  this->boundaries[0].high = (r-l) & (this->scap-1);
}

int terrabuff_next(terrabuff* this, terrabuff_slice* l, terrabuff_slice* r) {
  terrabuff_slice low = this->next_low;
  terrabuff_slice high = this->next_high;
  if (low > 2)
    low -= 2;
  else
    low = 0;

  if (high > this->scap/2)
    high = this->scap/2;

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

void terrabuff_bounds_override(terrabuff* this,
                               terrabuff_slice l, terrabuff_slice h) {
  this->boundaries[this->scan].low  = (l - this->soff) & (this->scap-1);
  this->boundaries[this->scan].high = (h - this->soff) & (this->scap-1);
  this->scurr = this->boundaries[this->scan].low;
}

void terrabuff_cancel_scan(terrabuff* this) {
  --this->scan;
}

void terrabuff_put(terrabuff* this, const vo3 where, canvas_pixel colour,
                   coord_offset xmax) {
  /* Update boundaries */
  if (where[0] < 0) {
    this->next_low = this->scurr;
  } else if (where[0] < xmax) {
    this->next_high = this->scurr+3;
  }

  memcpy(this->points[this->scan*this->scap + this->scurr].where, where,
         sizeof(vo3));
  this->points[this->scan*this->scap + this->scurr].colour = colour;

  /* Nominally, X coordinates need to be strictly ascending. However, in
   * extreme cases, off-screen points sometimes move the other way. It is safe
   * to silently patch such circumstances, since the points don't contribute to
   * the display significantly.
   */
  if (this->scurr > this->boundaries[this->scan].low &&
      where[0] <= this->points[this->scan*this->scap + this->scurr-1].where[0])
    this->points[this->scan*this->scap + this->scurr].where[0] = 1 +
      this->points[this->scan*this->scap + this->scurr-1].where[0];

  ++this->scurr;
}

void terrabuff_merge(terrabuff*restrict this, const terrabuff*restrict that) {
  unsigned i, off;

  /* Merge shared scans */
  for (i = 0; i < this->scan && i < that->scan; ++i) {
    assert(this->boundaries[i].high == that->boundaries[i].low);
    off = i * this->scap;

    /* Copy slices in that higher than in this */
    memcpy(this->points + off + this->boundaries[i].high,
           that->points + off + that->boundaries[i].low,
           sizeof(scan_point) * (that->boundaries[i].high -
                                 that->boundaries[i].low));
    this->boundaries[i].high = that->boundaries[i].high;
  }

  /* Add scans solely from that */
  if (this->scan < that->scan) {
    memcpy(this->boundaries + this->scan,
           that->boundaries + this->scan,
           sizeof(scan_boundary) * (that->scan - this->scan));
    for (i = this->scan; i < that->scan; ++i)
      memcpy(this->points + i * this->scap + this->boundaries[i].low,
             that->points + i * this->scap + this->boundaries[i].low,
             sizeof(scan_point) * (this->boundaries[i].high -
                                   this->boundaries[i].low));
    this->scan = that->scan;
  }
}

typedef struct {
  coord_offset y, z;
  unsigned char colour_left[3], colour_right[3], colour_gradient;
} screen_yz;

#ifdef PROFILE
/* Prevent inlining of our static functions so that we can get profiling data
 * on each individual one.
 */
static void interpolate(screen_yz*restrict, scan_point*restrict,
                        coord_offset, coord_offset)
__attribute__((noinline));
static void interpolate_all(screen_yz*restrict, scan_point*restrict,
                            unsigned, coord_offset, coord_offset)
__attribute__((noinline));
static void draw_line(canvas*restrict, const screen_yz*restrict,
                      unsigned, unsigned, unsigned, signed)
__attribute__((noinline));
static void draw_line_with_thickness(canvas*restrict, const screen_yz*restrict,
                                     unsigned, unsigned, unsigned, unsigned)
__attribute__((noinline));
static void draw_segments(canvas*restrict,
                          char*restrict,
                          const screen_yz*restrict,
                          const screen_yz*restrict,
                          unsigned, unsigned, unsigned);
__attribute__((noinline));
static void fill_area_between(canvas*restrict,
                              const screen_yz*restrict,
                              const screen_yz*restrict,
                              coord_offset,
                              unsigned, unsigned)
__attribute__((noinline));
static void collapse_buffer(char*restrict,
                            screen_yz*restrict, const screen_yz*restrict,
                            unsigned)
__attribute__((noinline));
#endif /* PROFILE */

static void interpolate(screen_yz*restrict dst, scan_point*restrict points,
                        coord_offset xmin, coord_offset xmax) {
  /* Use a Catmull-Rom spline for Y, linear for Z.
   * Colours are not interpolated; each paint line abruptly switches from the
   * left to the right colour somwhere between the points.
   */
  coord_offset x0 = points[1].where[0], x1 = points[2].where[0];
  coord_offset y0 = points[1].where[1], y1 = points[2].where[1];
  coord_offset z0 = points[1].where[2], z1 = points[2].where[2];
  canvas_pixel c0 = points[1].colour,   c1 = points[2].colour;
  coord_offset xl = (x0 >= xmin? x0 : xmin), xh = (x1 < xmax? x1 : xmax);
  coord_offset dx = x1 - x0;
  coord_offset x;
  coord_offset m0n, m1n;
  precise_fraction pidx, t1, t2, t3, m0d, m1d;

  if (!dx) {
    if (x0 >= xmin && x0 <= xmax) {
      dst[x0-xmin].y = y0;
      dst[x0-xmin].z = z0;
    }
    return;
  }

  /* Clamp y values to maximum range allowed by precise_fraction.
   * Need an extra bit clamped off to avoid overflow below.
   */
  y0 = clamps(-16384, y0, +16383);
  y1 = clamps(-16384, y1, +16383);

  /* Since here we know that the X of 1 and 2 are inequal, we also know that
   * those of 0 and 2, as well as 1 and 3, are inequal.
   *
   * We calculate here the tangents m0 and m1 as rationals, ie, m0 = m0n/m0d.
   */
  m0d = precise_fraction_of(points[2].where[0] - points[0].where[0]);
  m1d = precise_fraction_of(points[3].where[0] - points[1].where[0]);
  m0n = clamps(-16384, points[2].where[1] - points[0].where[1], +16383);
  m1n = clamps(-16384, points[3].where[1] - points[1].where[1], +16383);

  pidx = precise_fraction_of(dx);
  for (x = xl; x <= xh; ++x) {
    t1 = (x-x0) * pidx;
    t2 = precise_fraction_fmul(t1, t1);
    t3 = precise_fraction_fmul(t2, t1);

    dst[x-xmin].y = precise_fraction_sred(
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
    dst[x-xmin].z = ((x-x0)*(long long)z1 + (x1-x)*(long long)z0) / dx;
    dst[x-xmin].colour_left[0] = get_red(c0);
    dst[x-xmin].colour_left[1] = get_green(c0);
    dst[x-xmin].colour_left[2] = get_blue(c0);
    dst[x-xmin].colour_right[0] = get_red(c1);
    dst[x-xmin].colour_right[1] = get_green(c1);
    dst[x-xmin].colour_right[2] = get_blue(c1);
    dst[x-xmin].colour_gradient = (x - x0) * 256 / dx;
  }
}

static void interpolate_all(screen_yz*restrict dst,
                            scan_point*restrict points,
                            unsigned num_points,
                            coord_offset xmin,
                            coord_offset xmax) {
  unsigned i;

  for (i = 1; i < num_points-2; ++i)
    if (points[i+1].where[0] >= xmin && points[i].where[0] <= xmax)
      interpolate(dst, points + (i-1), xmin, xmax);
}

#define LINE_COLOUR (argb(255,12,12,12))
static void draw_line(canvas*restrict dst,
                      const screen_yz*restrict along,
                      unsigned x0, unsigned x1,
                      unsigned xoff,
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
      off = canvas_offset(dst, x+xoff, y);
      dst->px[off] = LINE_COLOUR;
      dst->depth[off] = along[x].z;
    }
  }
}

static void draw_line_with_thickness(canvas*restrict dst,
                                     const screen_yz*restrict along,
                                     unsigned x0, unsigned x1,
                                     unsigned xoff,
                                     unsigned thickness) {
  unsigned t;

  draw_line(dst, along, x0, x1, xoff, 0);

  for (t = 0; t < thickness; ++t) {
    draw_line(dst, along, x0, x1, xoff, +(signed)t);
    draw_line(dst, along, x0, x1, xoff, -(signed)t);
  }
}

static void draw_segments(canvas*restrict dst,
                          char*restrict line_points,
                          const screen_yz*restrict front,
                          const screen_yz*restrict back,
                          unsigned xmin, unsigned xmax,
                          unsigned thickness) {
  unsigned x0, x1;

  for (x0 = 0; x0 < xmax-xmin; ++x0) {
    if (back[x0].y <= front[x0].y && !line_points[x0]) {
      line_points[x0] = 1;
      x1 = x0+1;
      while (x1 < xmax-xmin && back[x1].y <= front[x1].y && !line_points[x1]) {
        line_points[x1] = 1;
        ++x1;
      }

      if (x1 >= xmax-xmin)
        x1 = xmax-xmin-1;

      if (x1 > x0)
        draw_line_with_thickness(dst, back, x0, x1, xmin, thickness);
      x0 = x1;
      line_points[x1] = 1;
    }
  }
}


static inline unsigned char mod8(unsigned char a, unsigned char b) {
  unsigned short prod = a;
  prod *= b;
  return prod >> 8;
}

static inline canvas_pixel modulate(canvas_pixel raw,
                                    unsigned char r,
                                    unsigned char g,
                                    unsigned char b) {
  return argb(get_alpha(r),
              mod8(get_red(raw), r),
              mod8(get_green(raw), g),
              mod8(get_blue(raw), b));
}

static void fill_area_between(canvas*restrict dst,
                              const screen_yz*restrict front,
                              const screen_yz*restrict back,
                              coord_offset tex_x_off,
                              unsigned x0, unsigned x1) {
  coord_offset y, y0, y1;
  unsigned x;
  const unsigned char*restrict colour;
  register unsigned w = dst->w;
  register canvas_pixel*restrict px;
  register const canvas_pixel*restrict tex;
  register unsigned*restrict depth;

  for (x = 0; x < x1-x0; ++x) {
    if (front[x].y < back[x].y) {
      y0 = front[x].y;
      y1 = back[x].y;
      if (y0 < 0) y0 = 0;
      if (y1 >= (signed)dst->h) y1 = (signed)dst->h-1;

      px = dst->px + canvas_offset(dst, x + x0, y0);
      depth = dst->depth + canvas_offset(dst, x + x0, y0);
      tex = texture + ((y0-front[x].y) & TEXMASK) +
        TEXSZ*((x+tex_x_off) & TEXMASK);

      for (y = y0; y <= y1; ++y) {
        if (front[x].colour_gradient + (0xF & *tex)*8 < 128)
          colour = front[x].colour_left;
        else
          colour = front[x].colour_right;

        *px = modulate(*tex, colour[0], colour[1], colour[2]);
        *depth = front[x].z + 65536;

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

#define RENDER_COL_W (4 * UMP_CACHE_LINE_SZ / sizeof(canvas_depth))
static canvas*restrict render_dst;
static terrabuff*restrict render_this;
static const rendering_context*restrict render_ctxt;
static void terrabuff_render_column(unsigned,unsigned);
static ump_task terrabuff_render_task = {
  terrabuff_render_column,
  0, /* Set dynamically */
  0  /* Adjusted dynamically */
};

void terrabuff_render(canvas*restrict dst,
                      terrabuff*restrict this,
                      const rendering_context*restrict ctxt) {
  render_dst = dst;
  render_this = this;
  render_ctxt = ctxt;

  terrabuff_render_task.num_divisions =
    (dst->w + RENDER_COL_W - 1) / RENDER_COL_W;
  if (terrabuff_render_task.num_divisions <
      terrabuff_render_task.divisions_for_master)
    terrabuff_render_task.divisions_for_master =
      terrabuff_render_task.num_divisions;

  ump_join();
  ump_run_async(&terrabuff_render_task);
}

static void terrabuff_render_column(unsigned ordinal, unsigned pcount) {
  canvas*restrict dst = render_dst;
  terrabuff*restrict this = render_this;
  coord_offset xmin = ordinal * RENDER_COL_W;
  coord_offset xmax =
    (xmin + RENDER_COL_W < dst->w? xmin + RENDER_COL_W : dst->w);
  const rendering_context_invariant*restrict context =
    (const rendering_context_invariant*restrict)render_ctxt;
  screen_yz lbuff_front[RENDER_COL_W+1], lbuff_back[RENDER_COL_W+1];
  /* Track points (X coordinates) that already have lines. This way, we can
   * avoid drawing the same line segment over and over.
   */
  char line_points[RENDER_COL_W+1];
  unsigned scan, x, line_thickness;
  coord_offset texture_x_offset;

  if (xmin >= xmax) return;

  texture_x_offset = (-(signed)dst->w) * 314159 / 200000 *
    context->long_yrot / context->proj->fov + xmin;
  line_thickness = 1 + dst->h / 1024;
  memset(line_points, 0, sizeof(line_points));

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
  for (x = 0; x < lenof(lbuff_front); ++x) {
    lbuff_back[x].y = 0x7FFFFFFF;
    lbuff_back[x].z = 0x7FFFFFFF;
    /* Also initialise the front buffer, since some off-screen circumstances
     * would cause it to be left uninitialised.
     */
    lbuff_front[x].y = 0x7FFFFFFF;
    lbuff_front[x].z = 0x7FFFFFFF;
  }
  for (scan = 0; scan < this->scan; ++scan) {
    interpolate_all(lbuff_front,
                    this->points + this->scap*scan + this->boundaries[scan].low,
                    this->boundaries[scan].high - this->boundaries[scan].low,
                    xmin, xmax);

    fill_area_between(dst, lbuff_front, lbuff_back,
                      texture_x_offset + 31*scan*scan + 75*scan,
                      xmin, xmax);
    draw_segments(dst, line_points, lbuff_front, lbuff_back,
                  xmin, xmax, line_thickness);

    collapse_buffer(line_points, lbuff_back, lbuff_front, xmax - xmin + 1);
  }

  draw_line_with_thickness(dst, lbuff_back, 0, xmax - xmin, xmin,
                           line_thickness);
}
