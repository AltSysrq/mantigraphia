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
#include "../simd.h"
#include "../defs.h"
#include "canvas.h"

/**
 * A pixel-shader is a function applied to every pixel of a triangle being
 * scanned onto a canvas. It is only called for pixels within the
 * canvas. Interps contains the interpolated values requested by the caller of
 * the shading fucntion.
 */
typedef void (*pixel_shader)(void* userdata,
                             coord_offset x, coord_offset y,
                             usimd8s);

/**
 * A triangle-shader is a function which shades triangles. They are usually
 * defined with the SHADE_TRIANGLE macro.
 */
typedef void (*triangle_shader)(canvas*restrict,
                                const coord_offset*restrict,
                                usimd8s,
                                const coord_offset*restrict,
                                usimd8s,
                                const coord_offset*restrict,
                                usimd8s,
                                void* userdata);
/**
 * Defines a function named <name> which can be used to shade arbitrary
 * triangles. The function <shader> will be called for each pixel.
 *
 * a, b, and c are the vertices of the triangle, and must be at least two
 * elements long. za, zb, and zc are the attributes to be interpolated.
 */
#define SHADE_TRIANGLE(name, shader)                                    \
  SHADE_UAXIS_TRIANGLE(_GLUE(name,_uaxis), shader)                      \
  SHADE_LAXIS_TRIANGLE(_GLUE(name,_laxis), shader)                      \
  static void name(canvas*restrict dst,                                 \
                   const coord_offset*restrict a,                       \
                   usimd8s za,                                          \
                   const coord_offset*restrict b,                       \
                   usimd8s zb,                                          \
                   const coord_offset*restrict c,                       \
                   usimd8s zc,                                          \
                   void* userdata) {                                    \
    shade_triangle(dst, a, za, b, zb, c, zc,                            \
                   _GLUE(name,_uaxis), _GLUE(name,_laxis),              \
                   userdata);                                           \
  }

#define SHADE__AXIS_TRIANGLE(name, shader, prim, sec, iy0, iy1)         \
  static void name(canvas*restrict dst,                                 \
                   coord_offset y0, coord_offset y1,                    \
                   coord_offset xt,                                     \
                   coord_offset xb0, coord_offset xb1,                  \
                   usimd8s zt,                                          \
                   usimd8s zb0,                                         \
                   usimd8s zb1,                                         \
                   void* userdata) {                                    \
    coord_offset x, xl, xh, dx, y, dy;                                  \
    usimd8s xo8, yo8, dx8, dy8, zl, zh, z;                              \
    const usimd8s one = simd_initsu8s(1);                               \
    signed long long yo;                                                \
    fraction idy;                                                       \
    usimd8s idx8, idy8;                                                 \
                                                                        \
    dy = iy1 - iy0;                                                     \
    if (!dy) return; /* degenerate */                                   \
    idy = fraction_of(dy);                                              \
    dy8 = simd_initsu8s(dy);                                            \
    idy8 = simd_ufrac8s(dy);                                            \
    for (y = (iy0 > 0? iy0 : 0); y <= iy1 && y < (signed)dst->h; ++y) { \
      yo = y - iy0;                                                     \
      yo8 = simd_initsu8s(yo);                                          \
      xl = fraction_smul((dy-yo)*prim(x,0) + yo*sec(x,0), idy);         \
      xh = fraction_smul((dy-yo)*prim(x,1) + yo*sec(x,1), idy)+1;       \
      zl = simd_uadd8s(simd_umulhi8s(simd_umullo8s(simd_usub8s(dy8,yo8),\
                                                   idy8),               \
                                     prim(z,0)),                        \
                       simd_umulhi8s(simd_umullo8s(yo8, idy8),          \
                                     sec(z,0)));                        \
      zh = simd_uadd8s(simd_umulhi8s(simd_umullo8s(simd_usub8s(dy8,yo8),\
                                                   idy8),               \
                                     prim(z,1)),                        \
                       simd_umulhi8s(simd_umullo8s(yo8, idy8),          \
                                     sec(z,1)));                        \
      dx = xh-xl;                                                       \
      if (!dx) continue; /* nothing to draw here */                     \
      idx8 = simd_ufrac8s(dx);                                          \
      dx8 = simd_initsu8s(dx);                                          \
      xo8 = simd_initsu8s(xl > 0? 0 : -xl);                             \
      for (x = (xl > 0? xl : 0); x <= xh && x < (signed)dst->w; ++x) {  \
        z = simd_uadd8s(simd_umulhi8s(simd_umullo8s(simd_usub8s(dx8,    \
                                                                 xo8),  \
                                                     idx8),             \
                                      zl),                              \
                        simd_umulhi8s(simd_umullo8s(xo8, idx8), zh));   \
        shader(userdata, x, y, z);                                      \
        xo8 = simd_uadd8s(xo8, one);                                    \
      }                                                                 \
    }                                                                   \
  }

/**
 * Defines a function named <name> which shades an above-axis-aligned triangle
 * using the given shader. The triangle must have an axis-alinged base, the
 * third point lying at or above the base. (y0,xt) is the third point; (y1,xb0)
 * and (y1,xb1) are the base points. xb0 must be less than or equal to xb1.
 *
 * Values in each z vector are linearly interpolated (according to screen
 * space) between the vertices.
 */
#define SHADE_UAXIS_TRIANGLE(name, shader)                              \
  SHADE__AXIS_TRIANGLE(name, shader,                                    \
                       TSCAN_TIP_VAR, TSCAN_BORD_VAR, y0, y1)

/**
 * Like SHADE_UAXIS_TRIANGLE, but the tip must be at or below the base.
 */
#define SHADE_LAXIS_TRIANGLE(name, shader)                              \
  SHADE__AXIS_TRIANGLE(name, shader,                                    \
                       TSCAN_BORD_VAR, TSCAN_TIP_VAR, y1, y0)

#define TSCAN_TIP_VAR(var,off) var##t
#define TSCAN_BORD_VAR(var,off) var##b##off

typedef void (*partial_triangle_shader)(
  canvas*restrict,
  coord_offset, coord_offset,
  coord_offset, coord_offset, coord_offset,
  usimd8s, usimd8s, usimd8s,
  void*);

void shade_triangle(canvas*restrict,
                    const coord_offset*restrict,
                    usimd8s,
                    const coord_offset*restrict,
                    usimd8s,
                    const coord_offset*restrict,
                    usimd8s,
                    partial_triangle_shader upper,
                    partial_triangle_shader lower,
                    void* userdata);

#endif /* GRAPHICS_TSCAN_H_ */
