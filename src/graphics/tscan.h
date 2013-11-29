/*
  Copyright (c) 2013 Jason Lingle
  All rights reserved.

  Redistribution and use in source and binary forms, with or without
  modification, are permitted provided that the following conditions
  are met:
  1. Redistributions of source code must retain the above copyright
     notice, this list of conditions and the following disclaimer.
  2. Redistributions in binary form must reproduce the above copyright
     notice, this list of conditions and the following disclaimer in the
     documentation and/or other materials provided with the distribution.
  3. Neither the name of the author nor the names of its contributors
     may be used to endorse or promote products derived from this software
     without specific prior written permission.

     THIS SOFTWARE IS PROVIDED BY THE AUTHORS AND CONTRIBUTORS ``AS IS'' AND
     ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
     IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
     ARE DISCLAIMED.  IN NO EVENT SHALL THE AUTHORS OR CONTRIBUTORS BE LIABLE
     FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
     DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
     OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
     HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
     LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
     OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
     SUCH DAMAGE.
*/
#ifndef GRAPHICS_TSCAN_H_
#define GRAPHICS_TSCAN_H_

#include "../coords.h"
#include "canvas.h"

/**
 * A triangle-shader is a function applied to every pixel of a triangle being
 * scanned onto a canvas. It is only called for pixels within the
 * canvas. Interps contains the interpolated values requested by the caller of
 * the shading fucntion.
 */
typedef void (*triangle_shader)(void* userdata,
                                coord_offset x, coord_offset y,
                                const coord_offset* interps);

/**
 * Shades an above-axis-aligned triangle using the given shader. The triangle
 * must have an axis-alinged base, the third point lying at or above the
 * base. (y0,xt) is the third point; (y1,xb0) and (y1,xb1) are the base
 * points. xb0 must be less than or equal to xb1.
 *
 * Values in each z array are linearly interpolated (according to screen space)
 * between the vertices. The length of these arrays is given by nz.
 */
static
#ifndef PROFILE
inline
#endif
void shade_uaxis_triangle(canvas*restrict dst,
                          coord_offset y0, coord_offset y1,
                          coord_offset xt, coord_offset xb0, coord_offset xb1,
                          unsigned nz,
                          const coord_offset*restrict zt,
                          const coord_offset*restrict zb0,
                          const coord_offset*restrict zb1,
                          triangle_shader shader, void* userdata) {
  coord_offset x, xl, xh, dx, xo, y, dy, yo, z[nz], zl[nz], zh[nz];
  unsigned i;

  dy = y1 - y0;
  for (y = (y0 > 0? y0 : 0); y < y1 && y < dst->h; ++y) {
    yo = y - y0;
    xl = ((dy-yo)*xt + yo*xb0)/dy;
    xh = ((dy-yo)*xt + yo*xb1)/dy;
    for (i = 0; i < nz; ++i) {
      zl[i] = ((dy-yo)*zt[i] + yo*zb0[i])/dy;
      zh[i] = ((dy-yo)*zt[i] + yo*zb1[i])/dy;
    }
    dx = xh-xl;
    for (x = (xl > 0? xl : 0); x < xh && x < dst->w; ++x) {
      xo = x-xl;
      for (i = 0; i < nz; ++i)
        z[i] = ((dx-xo)*zl[i] + xo*zh[i])/dx;
      (*shader)(userdata, x, y, z);
    }
  }
}

#endif /* GRAPHICS_TSCAN_H_ */
