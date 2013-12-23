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
#ifndef GRAPHICS_TSCAN_H_
#define GRAPHICS_TSCAN_H_

#include "../coords.h"
#include "../frac.h"
#include "canvas.h"
#include "../defs.h"

/**
 * A pixel-shader is a function applied to every pixel of a triangle being
 * scanned onto a canvas. It is only called for pixels within the
 * canvas. Interps contains the interpolated values requested by the caller of
 * the shading fucntion.
 */
typedef void (*pixel_shader)(void* userdata,
                             coord_offset x, coord_offset y,
                             const coord_offset* interps);

/**
 * A triangle-shader is a function which shades triangles. They are usually
 * defined with the SHADE_TRIANGLE macro.
 */
typedef void (*triangle_shader)(canvas*restrict,
                                const coord_offset*restrict,
                                const coord_offset*restrict,
                                const coord_offset*restrict,
                                const coord_offset*restrict,
                                const coord_offset*restrict,
                                const coord_offset*restrict,
                                void* userdata);
/**
 * Defines a function named <name> which can be used to shade arbitrary
 * triangles. The function <shader> will be called for each pixel. nz
 * coord_offsets will be interpolated linearly between vertices.
 *
 * a, b, and c are the vertices of the triangle, and must be at least two
 * elements long. za, zb, and zc are the attributes to be interpolated.
 */
#define SHADE_TRIANGLE(name, shader, nz)                                \
  SHADE_UAXIS_TRIANGLE(_GLUE(name,_uaxis), shader, nz)                  \
  SHADE_LAXIS_TRIANGLE(_GLUE(name,_laxis), shader, nz)                  \
  static void name(canvas*restrict dst,                                 \
                   const coord_offset*restrict a,                       \
                   const coord_offset*restrict za,                      \
                   const coord_offset*restrict b,                       \
                   const coord_offset*restrict zb,                      \
                   const coord_offset*restrict c,                       \
                   const coord_offset*restrict zc,                      \
                   void* userdata) {                                    \
    shade_triangle(dst, a, za, b, zb, c, zc, nz,                        \
                   _GLUE(name,_uaxis), _GLUE(name,_laxis),              \
                   userdata);                                           \
  }

/**
 * Defines a function named <name> which shades an above-axis-aligned triangle
 * using the given shader. The triangle must have an axis-alinged base, the
 * third point lying at or above the base. (y0,xt) is the third point; (y1,xb0)
 * and (y1,xb1) are the base points. xb0 must be less than or equal to xb1.
 *
 * Values in each z array are linearly interpolated (according to screen space)
 * between the vertices. The length of these arrays is given by nz.
 */
#define SHADE_UAXIS_TRIANGLE(name, shader, nz)                          \
  static void name(canvas*restrict dst,                                 \
                   coord_offset y0, coord_offset y1,                    \
                   coord_offset xt,                                     \
                   coord_offset xb0, coord_offset xb1,                  \
                   const coord_offset*restrict zt,                      \
                   const coord_offset*restrict zb0,                     \
                   const coord_offset*restrict zb1,                     \
                   void* userdata) {                                    \
    coord_offset x, xl, xh, dx, y, dy, z[nz], zl[nz], zh[nz];           \
    signed long long xo, yo;                                            \
    fraction idy, idx;                                                  \
    unsigned i;                                                         \
                                                                        \
    dy = y1 - y0;                                                       \
    if (!dy) return; /* degenerate */                                   \
    idy = fraction_of(dy);                                              \
    for (y = (y0 > 0? y0 : 0); y <= y1 && y < (signed)dst->h; ++y) {    \
      yo = y - y0;                                                      \
      xl = fraction_smul((dy-yo)*xt + yo*xb0, idy);                     \
      xh = fraction_smul((dy-yo)*xt + yo*xb1, idy)+1;                   \
      for (i = 0; i < nz; ++i) {                                        \
        zl[i] = fraction_smul((dy-yo)*zt[i] + yo*zb0[i], idy);          \
        zh[i] = fraction_smul((dy-yo)*zt[i] + yo*zb1[i], idy);          \
      }                                                                 \
      dx = xh-xl;                                                       \
      if (!dx) continue; /* nothing to draw */                          \
      idx = fraction_of(dx);                                            \
      for (x = (xl > 0? xl : 0); x <= xh && x < (signed)dst->w; ++x) {  \
        xo = x-xl;                                                      \
        for (i = 0; i < nz; ++i)                                        \
          z[i] = fraction_smul((dx-xo)*zl[i] + xo*zh[i], idx);          \
        shader(userdata, x, y, z);                                      \
      }                                                                 \
    }                                                                   \
  }

/**
 * Like SHADE_UAXIS_TRIANGLE, but the tip must be at or below the base.
 */
#define SHADE_LAXIS_TRIANGLE(name, shader, nz)                          \
  static void name(canvas*restrict dst,                                 \
                   coord_offset y0, coord_offset y1,                    \
                   coord_offset xt,                                     \
                   coord_offset xb0, coord_offset xb1,                  \
                   const coord_offset*restrict zt,                      \
                   const coord_offset*restrict zb0,                     \
                   const coord_offset*restrict zb1,                     \
                   void* userdata) {                                    \
    coord_offset x, xl, xh, dx, y, dy, z[nz], zl[nz], zh[nz];           \
    signed long long xo, yo;                                            \
    fraction idx, idy;                                                  \
    unsigned i;                                                         \
                                                                        \
    dy = y0 - y1;                                                       \
    if (!dy) return; /* degenerate */                                   \
    idy = fraction_of(dy);                                              \
    for (y = (y1 > 0? y1 : 0); y <= y0 && y < (signed)dst->h; ++y) {    \
      yo = y - y1;                                                      \
      xl = fraction_smul((dy-yo)*xb0 + yo*xt, idy);                     \
      xh = fraction_smul((dy-yo)*xb1 + yo*xt, idy)+1;                   \
      for (i = 0; i < nz; ++i) {                                        \
        zl[i] = fraction_smul((dy-yo)*zb0[i] + yo*zt[i], idy);          \
        zh[i] = fraction_smul((dy-yo)*zb1[i] + yo*zt[i], idy);          \
      }                                                                 \
      dx = xh-xl;                                                       \
      if (!dx) continue; /* nothing to draw here */                     \
      idx = fraction_of(dx);                                            \
      for (x = (xl > 0? xl : 0); x <= xh && x < (signed)dst->w; ++x) {  \
        xo = x-xl;                                                      \
        for (i = 0; i < nz; ++i)                                        \
          z[i] = fraction_smul((dx-xo)*zl[i] + xo*zh[i], idx);          \
        shader(userdata, x, y, z);                                      \
      }                                                                 \
    }                                                                   \
  }

typedef void (*partial_triangle_shader)(
  canvas*restrict,
  coord_offset, coord_offset,
  coord_offset, coord_offset, coord_offset,
  const coord_offset*restrict,
  const coord_offset*restrict,
  const coord_offset*restrict,
  void*);

void shade_triangle(canvas*restrict,
                    const coord_offset*restrict,
                    const coord_offset*restrict,
                    const coord_offset*restrict,
                    const coord_offset*restrict,
                    const coord_offset*restrict,
                    const coord_offset*restrict,
                    unsigned nz,
                    partial_triangle_shader upper,
                    partial_triangle_shader lower,
                    void* userdata);

#endif /* GRAPHICS_TSCAN_H_ */
