/*-
 * Copyright (c) 2015 Jason Lingle
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

#include "../math/coords.h"
#include "../math/frac.h"
#include "voxel-tex-interp.h"

void voxel_tex_demux(unsigned char*restrict control_rg,
                     unsigned char*restrict selector_r,
                     const unsigned char*restrict src_rgb,
                     unsigned src_len_px) {
  while (src_len_px--) {
    selector_r[0] = src_rgb[0];
    control_rg[0] = src_rgb[1];
    control_rg[1] = src_rgb[2];
    selector_r += 1;
    control_rg += 2;
    src_rgb += 3;
  }
}

static void get_interp_pos(unsigned*restrict p0, fraction*restrict p0w,
                           unsigned*restrict p1, fraction*restrict p1w,
                           unsigned pmax,
                           unsigned value) {
  unsigned interval = 256 / pmax;

  *p0 = value / interval;
  *p1 = umin(pmax-1, *p0 + 1);
  *p1w = fraction_of2(value - (value / interval * interval), interval);
  *p0w = fraction_of(1) - *p1w;
}

static void interpolate(unsigned char* restrict dst_rgba,
                        const unsigned char*restrict a_rgba,
                        fraction aw,
                        const unsigned char*restrict b_rgba,
                        fraction bw,
                        const unsigned char*restrict c_rgba,
                        fraction cw,
                        const unsigned char*restrict d_rgba,
                        fraction dw) {
  *dst_rgba = fraction_umul(*a_rgba, aw)
            + fraction_umul(*b_rgba, bw)
            + fraction_umul(*c_rgba, cw)
            + fraction_umul(*d_rgba, dw);
}

static void interpolate4(unsigned char* restrict dst_rgba,
                         const unsigned char*restrict a_rgba,
                         fraction aw,
                         const unsigned char*restrict b_rgba,
                         fraction bw,
                         const unsigned char*restrict c_rgba,
                         fraction cw,
                         const unsigned char*restrict d_rgba,
                         fraction dw) {
  unsigned i;

  for (i = 0; i < 4; ++i)
    interpolate(dst_rgba+i, a_rgba+i, aw, b_rgba+i, bw,
                c_rgba+i, cw, d_rgba+i, dw);
}

void voxel_tex_apply_palette(unsigned char*restrict dst_rgba,
                             const unsigned char*restrict src_r,
                             unsigned src_len_px,
                             const unsigned char*restrict palette_rgba,
                             unsigned palette_w_px,
                             unsigned palette_h_px,
                             fraction t) {
  unsigned t0, t1, s0, s1, t0off, t1off;
  fraction t0w, t1w, s0w, s1w;

  get_interp_pos(&t0, &t0w, &t1, &t1w, palette_h_px, fraction_umul(255, t));
  t0off = t0 * palette_w_px * 4;
  t1off = t1 * palette_w_px * 4;

  while (src_len_px--) {
    get_interp_pos(&s0, &s0w, &s1, &s1w, palette_w_px, *src_r);
    interpolate4(dst_rgba,
                 palette_rgba + t0off + 4 * s0, fraction_umul(t0w, s0w),
                 palette_rgba + t0off + 4 * s1, fraction_umul(t0w, s1w),
                 palette_rgba + t1off + 4 * s0, fraction_umul(t1w, s0w),
                 palette_rgba + t1off + 4 * s1, fraction_umul(t1w, s1w));

    dst_rgba += 4;
    src_r += 1;
  }
}
