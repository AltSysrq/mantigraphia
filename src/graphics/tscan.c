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

#include "tscan.h"

void shade_triangle(canvas*restrict dst,
                    const coord_offset*restrict a,
                    usimd8s za,
                    const coord_offset*restrict b,
                    usimd8s zb,
                    const coord_offset*restrict c,
                    usimd8s zc,
                    partial_triangle_shader upper,
                    partial_triangle_shader lower,
                    void* userdata) {
  const coord_offset* tmp;
  coord_offset dy, yo, midx, x0, x1;
  usimd8s z0, zmid, z1, ztmp;

#define SWAP(a,b,tmp) tmp=a, a=b, b=tmp
  /* Sort a,b,c into ascending order by Y axis */
  if (a[1] > b[1] || a[1] > c[1]) {
    if (b[1] < c[1]) {
      SWAP(a,b,tmp);
      SWAP(za,zb,ztmp);
    } else {
      SWAP(a,c,tmp);
      SWAP(za,zc,ztmp);
    }
  }

  if (b[1] > c[1]) {
    SWAP(b,c,tmp);
    SWAP(zb,zc,ztmp);
  }
#undef SWAP

  /* Calculate the "midpoint", ie, where the triangle can be split to be passed
   * to the uaxis and laxis functions.
   */
  dy = c[1] - a[1];
  if (!dy) return; /* degenerate triangle, nothing to do anyway */
  yo = b[1] - a[1];
  midx = (yo*c[0] + (dy-yo)*a[0])/dy;
  zmid = simd_uadd8s(
    simd_umulhi8s(simd_umullo8s(simd_initsu8s(yo),
                                simd_ufrac8s(dy)),
                  zc),
    simd_umulhi8s(simd_umullo8s(simd_initsu8s(dy-yo),
                                simd_ufrac8s(dy)),
                  za));

  if (midx < b[0]) {
    x0 = midx;
    z0 = zmid;
    x1 = b[0];
    z1 = zb;
  } else {
    x0 = b[0];
    z0 = zb;
    x1 = midx;
    z1 = zmid;
  }

  (*upper)(dst,
           a[1], b[1],
           a[0], x0, x1,
           za, z0, z1,
           userdata);
  (*lower)(dst,
           c[1], b[1],
           c[0], x0, x1,
           zc, z0, z1,
           userdata);
}
