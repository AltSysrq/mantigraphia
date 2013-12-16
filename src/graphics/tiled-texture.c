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
#include "tiled-texture.h"

typedef struct {
  const tiled_texture*restrict tex;
  canvas* dst;
  unsigned resscale16;
} tt_info;

static inline void tiled_texture_shader(void* vinfo,
                                        coord_offset x,
                                        coord_offset y,
                                        const coord_offset* z) {
  const tt_info* info = vinfo;
  coord_offset ox, oy, tx, ty;

  ox = x +  info->tex->x_off;
  oy = y + info->tex->y_off;

  tx = zo_scale(ox, info->tex->rot_cos) - zo_scale(oy, info->tex->rot_sin);
  ty = zo_scale(oy, info->tex->rot_cos) + zo_scale(ox, info->tex->rot_sin);
  tx *= info->resscale16;
  tx >>= 16;
  ty *= info->resscale16;
  ty >>= 16;

  tx &= info->tex->w_mask;
  ty &= info->tex->h_mask;

  canvas_write_c(info->dst, x, y,
                 info->tex->texture[tx + ty*info->tex->pitch],
                 *z);
}

SHADE_TRIANGLE(shade_tiled_texture, tiled_texture_shader, 1)

void tiled_texture_fill(
  canvas*restrict dst,
  const tiled_texture*restrict tex,
  const vo3 a, const vo3 b, const vo3 c
) {
  tt_info info = {
    tex, dst,
    65536 * dst->w / tex->nominal_resolution
  };
  shade_tiled_texture(dst, a, a+2, b, b+2, c, c+2, &info);
}

void tiled_texture_fill_a(
  canvas*restrict dst,
  const coord_offset*restrict a,
  const coord_offset*restrict za,
  const coord_offset*restrict b,
  const coord_offset*restrict zb,
  const coord_offset*restrict c,
  const coord_offset*restrict zc,
  void* tex
) {
  vo3 va = { a[0], a[1], za[0] },
      vb = { b[0], b[1], zb[0] },
      vc = { c[0], c[1], zc[0] };
  tiled_texture_fill(dst, tex, va, vb, vc);
}
