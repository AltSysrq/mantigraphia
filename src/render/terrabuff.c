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

#include <glew.h>

#include "../math/coords.h"
#include "../alloc.h"
#include "../defs.h"
#include "../math/frac.h"
#include "../micromp.h"
#include "../graphics/canvas.h"
#include "../graphics/linear-paint-tile.h"
#include "../graphics/perspective.h"
#include "../gl/shaders.h"
#include "../gl/marshal.h"
#include "context.h"
#include "terrabuff.h"

#define TEXSZ 256
static GLuint texture;
static GLuint hmap;

/* Because multiple terrabuffs may be created on an ad-hoc basis, we can't give
 * each one its own slab group. But in practise there isn't ever more than one
 * terrabuff that actually gets rendered, so we can just make the uniform and
 * such global state.
 */
static shader_terrabuff_uniform terrabuff_uniform;
static glm_slab_group* glmsg;
static void terrabuff_activate(void*);
static void terrabuff_deactivate(void*);

void terrabuff_init(void) {
  canvas* tmp;

  static const canvas_pixel pallet[] = {
    argb(255,255,255,255),
    argb(255,64,64,64),
  };

  /* Allocate temporary heap space to store the ARGB texture */
  tmp = canvas_new(TEXSZ, TEXSZ);

  linear_paint_tile_render(tmp->px, TEXSZ, TEXSZ,
                           TEXSZ/4, 4,
                           pallet, lenof(pallet));
  texture = canvas_to_texture(tmp, 0);
  glBindTexture(GL_TEXTURE_2D, texture);

  /* Done with temporary */
  canvas_delete(tmp);

  glGenTextures(1, &hmap);

  glmsg = glm_slab_group_new(terrabuff_activate,
                             terrabuff_deactivate,
                             NULL,
                             shader_terrabuff_configure_vbo,
                             sizeof(shader_terrabuff_vertex));
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
  this->next_low = 0;
  this->next_high = (r-l) & (this->scap-1);
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
  unsigned i, j, off;

  /* Merge shared scans */
  for (i = 0; i < this->scan && i < that->scan; ++i) {
    assert(this->boundaries[i].high == that->boundaries[i].low);
    off = i * this->scap;

    /* Copy slices in that higher than in this */
    memcpy(this->points + off + this->boundaries[i].high,
           that->points + off + that->boundaries[i].low,
           sizeof(scan_point) * (that->boundaries[i].high -
                                 that->boundaries[i].low));
    /* Fix up incorrect X coordinates (see _put() for why) */
    for (j = this->boundaries[i].high; j < that->boundaries[i].high; ++j)
      if (j > this->boundaries[i].low &&
          this->points[off+j].where[0] <= this->points[off+j-1].where[0])
        this->points[off+j].where[0] = 1 + this->points[off+j-1].where[0];
    /* If this had no points, move low boundary to correspond to low boundary
     * of other.
     */
    if (this->boundaries[i].low == this->boundaries[i].high)
      this->boundaries[i].low = that->boundaries[i].low;

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
  unsigned short y;
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
static void collapse_buffer(screen_yz*restrict, const screen_yz*restrict,
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
  coord_offset xl = (x0 >= xmin? x0 : xmin), xh = (x1 < xmax? x1 : xmax);
  coord_offset dx = x1 - x0;
  coord_offset x, sy;
  coord_offset m0n, m1n;
  precise_fraction pidx, t1, t2, t3, m0d, m1d;

  if (!dx) {
    if (x0 >= xmin && x0 <= xmax) {
      dst[x0-xmin].y = (y0 > 0? y0 : 0);
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

    sy = precise_fraction_sred(
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

    if (sy >= 0 && sy < 65536)
      dst[x-xmin].y = sy;
    else if (sy >= 65536)
      dst[x-xmin].y = 65535;
    else
      dst[x-xmin].y = 0;
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

static void collapse_buffer(screen_yz*restrict dst,
                            const screen_yz*restrict src,
                            unsigned xmax) {
  unsigned x;

  for (x = 0; x < xmax; ++x)
    if (src[x].y < dst[x].y)
      dst[x].y = src[x].y;
}

#define RENDER_COL_W (4 * UMP_CACHE_LINE_SZ / sizeof(canvas_depth))
static terrabuff* terrabuff_this;
static screen_yz* terrabuff_interp;
static canvas* terrabuff_dst;
static unsigned terrabuff_interp_pitch;

static void do_interpolate(unsigned ordinal, unsigned pcount) {
  const canvas*restrict dst = terrabuff_dst;
  const terrabuff*restrict this = terrabuff_this;
  screen_yz initial_back_buffer[RENDER_COL_W+1];
  screen_yz* lbuff_front, * lbuff_back = initial_back_buffer;
  coord_offset xmin = ordinal * RENDER_COL_W;
  coord_offset xmax =
    (xmin + RENDER_COL_W < dst->w? xmin + RENDER_COL_W : dst->w);
  unsigned scan;

  if (xmin >= xmax) return;

  lbuff_front = terrabuff_interp + xmin;

  memset(lbuff_back, ~0, sizeof(initial_back_buffer));

  for (scan = 0; scan < this->scan; ++scan) {
    memset(lbuff_front, ~0, (xmax-xmin) * sizeof(screen_yz));
    interpolate_all(lbuff_front,
                    this->points + scan * this->scap +
                      this->boundaries[scan].low,
                    this->boundaries[scan].high -
                      this->boundaries[scan].low,
                    xmin, xmax-1);
    collapse_buffer(lbuff_front, lbuff_back, xmax - xmin);
    lbuff_back = lbuff_front;
    lbuff_front += terrabuff_interp_pitch;
  }
}

static void interp_to_gl(void* ignored) {
  glBindTexture(GL_TEXTURE_2D, hmap);
  glPixelStorei(GL_UNPACK_ROW_LENGTH, terrabuff_interp_pitch);
  glTexImage2D(GL_TEXTURE_2D, 0, GL_R16,
               terrabuff_dst->w, terrabuff_this->scan, 0,
               GL_RED, GL_UNSIGNED_SHORT, terrabuff_interp);
}

static void render_rectangle_between(glm_slab* slab,
                                     const scan_point*restrict l,
                                     const scan_point*restrict r,
                                     const screen_yz*restrict upper,
                                     const screen_yz*restrict lower,
                                     unsigned xmax,
                                     float hmap_y) {
  unsigned ymin = 65536, ymax = 0, mixing;
  signed x0, x1, x;
  shader_terrabuff_vertex* vertices;
  unsigned short* indices, base;
  unsigned i;

  x0 = (l->where[0] >= 0? l->where[0] : 0);
  x1 = (r->where[0] < (signed)xmax? r->where[0] : (signed)xmax);
  if (x0 >= x1) return;

  for (x = x0; x < x1; ++x) {
    if (upper[x].y < ymin) ymin = upper[x].y;
    if (lower[x].y > ymax) ymax = lower[x].y;
  }

  if (ymin < ymax)
    mixing = ymax - ymin;
  else
    mixing = 0;

  base = GLM_ALLOC(&vertices, &indices, slab, 4, 6);
  vertices[0].v[0] = l->where[0];
  vertices[0].v[1] = ymin;
  vertices[0].v[2] = l->where[2];
  vertices[1].v[0] = l->where[0];
  vertices[1].v[1] = ymax + terrabuff_uniform.line_thickness + mixing;
  vertices[1].v[2] = l->where[2];
  vertices[2].v[0] = r->where[0];
  vertices[2].v[1] = ymin;
  vertices[2].v[2] = r->where[2];
  vertices[3].v[0] = r->where[0];
  vertices[3].v[1] = ymax + terrabuff_uniform.line_thickness + mixing;
  vertices[3].v[2] = r->where[2];
  vertices[0].tc[0] = vertices[1].tc[0] = l->where[0] / (float)xmax;
  vertices[2].tc[0] = vertices[3].tc[0] = r->where[0] / (float)xmax;
  vertices[0].side[0] = vertices[1].side[0] = 0.0f;
  vertices[2].side[0] = vertices[3].side[0] = 1.0f;
  for (i = 0; i < 4; ++i) {
    vertices[i].tc[1] = hmap_y;
    canvas_pixel_to_gl4fv(vertices[i].colour, l->colour);
    canvas_pixel_to_gl4fv(vertices[i].sec_colour, r->colour);
  }
  indices[0] = base + 0;
  indices[1] = base + 1;
  indices[2] = base + 2;
  indices[3] = base + 1;
  indices[4] = base + 2;
  indices[5] = base + 3;
}

static ump_task terrabuff_interpolate_task = {
  do_interpolate,
  0, /* set dynamically */
  0, /* unused (sync) */
};

static void terrabuff_activate(void* ignored) {
  glBindTexture(GL_TEXTURE_2D, hmap);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP);
  glActiveTexture(GL_TEXTURE1);
  glBindTexture(GL_TEXTURE_2D, texture);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT);
  glActiveTexture(GL_TEXTURE0);
  glDepthFunc(GL_ALWAYS);
  shader_terrabuff_activate(&terrabuff_uniform);
}

static void terrabuff_deactivate(void* ignored) {
  glDepthFunc(GL_LESS);
  /* We can't safely free the interps here, as rendering might have fragmented
   * into multiple slabs. Just free them before the next time we allocate a new
   * group, instead.
   */
}

void terrabuff_render(canvas*restrict dst,
                      terrabuff*restrict this,
                      const rendering_context*restrict ctxt) {
  const rendering_context_invariant*restrict context =
    CTXTINV(ctxt);
  glm_slab* slab;
  unsigned scan, i;
  screen_yz* upper, * lower, initial_lower[dst->w];

  terrabuff_dst = dst;
  terrabuff_this = this;
  /* Assume canvas's pitch achieves the same alignment we'd want */
  terrabuff_interp_pitch = dst->pitch;
  /* Free the previous frame's interps if present */
  if (terrabuff_interp) free(terrabuff_interp);
  terrabuff_interp = xmalloc(dst->pitch * this->scan * sizeof(screen_yz));
  terrabuff_uniform.hmap = 0;
  terrabuff_uniform.tex = 1;
  terrabuff_uniform.ty_below = 1.0f / this->scan;
  terrabuff_uniform.line_thickness = dst->w / 386;
  terrabuff_uniform.screen_size[0] = dst->w;
  terrabuff_uniform.screen_size[1] = dst->h;
  terrabuff_uniform.xoff = (-(signed)dst->w) * 314159 / 200000 *
    context->long_yrot / context->proj->fov;

  terrabuff_interpolate_task.num_divisions =
    (dst->w + RENDER_COL_W - 1) / RENDER_COL_W * RENDER_COL_W;
  ump_run_sync(&terrabuff_interpolate_task);
  glm_do(interp_to_gl, NULL);

  memset(initial_lower, ~0, sizeof(initial_lower));
  lower = initial_lower;
  upper = terrabuff_interp;

  slab = glm_slab_get(glmsg);
  for (scan = 0; scan < this->scan; ++scan) {
    for (i = this->boundaries[scan].low;
         i+1 < this->boundaries[scan].high; ++i) {
      render_rectangle_between(
        slab,
        this->points + scan*this->scap + i,
        this->points + scan*this->scap + i + 1,
        upper, lower, dst->w, (scan+0.9f) / (float)this->scan);
    }

    lower = upper;
    upper += dst->pitch;
  }

  /* Add line over final scan */
  scan = this->scan - 1;
  for (i = this->boundaries[scan].low;
       i+1 < this->boundaries[scan].high; ++i) {
    render_rectangle_between(
      slab,
      this->points + scan*this->scap + i,
      this->points + scan*this->scap + i + 1,
      /* The texture is clamped, so we can just pass an arbitrarily large value
       * to ensure that the shader sees only the top.
       */
      lower, lower, dst->w, 2.0f);
  }

  glm_finish_thread();
}
