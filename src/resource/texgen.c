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

#include <string.h>
#include <stdlib.h>

#include "../math/rand.h"
#include "../math/coords.h"
#include "texgen.h"

static unsigned char tg_temp[TG_TEXSIZE * 3];

tg_tex8 tg_fill(unsigned char value) {
  memset(tg_temp, value, TG_TEXSIZE);
  return tg_temp;
}

tg_tex8 tg_uniform_noise(const unsigned char* raw_src, unsigned rnd) {
  unsigned i, n;
  unsigned char fallback[253];
  const unsigned char* src;

  if (!raw_src || !*raw_src) {
    for (i = 0; i < 253; ++i)
      fallback[i] = i + 1;
    src = fallback;
    n = 253;
  } else {
    src = raw_src;
    n = strlen((const char*)src);
  }

  for (i = 0; i < TG_TEXSIZE; ++i)
    tg_temp[i] = src[lcgrand(&rnd) % n];

  return tg_temp;
}

tg_tex8 tg_perlin_noise(unsigned freq, unsigned amp, unsigned seed) {
  unsigned values[TG_TEXSIZE], i;

  memset(values, 0, sizeof(values));
  perlin_noise_st(values, TG_TEXDIM, TG_TEXDIM, freq, amp, seed);
  for (i =  0; i < TG_TEXSIZE; ++i)
    tg_temp[i] = values[i];

  return tg_temp;
}

tg_tex8 tg_sum(tg_tex8 a, tg_tex8 b) {
  unsigned i;

  for (i = 0; i < TG_TEXSIZE; ++i)
    tg_temp[i] = a[i] + b[i];

  return tg_temp;
}

tg_tex8 tg_similarity(signed cx, signed cy, tg_tex8 control,
                      signed base) {
  signed px, py, dx, dy, d, val;

  for (py = 0; py < TG_TEXDIM; ++py) {
    for (px = 0; px < TG_TEXDIM; ++px) {
      dx = cx - px;
      dy = cy - py;
      d = isqrt(dx*dx + dy*dy);

      val = (unsigned)control[py*TG_TEXDIM + px];

      tg_temp[py*TG_TEXDIM + px] = umax(0, 255 - d - abs(base-val));
    }
  }

  return tg_temp;
}

tg_tex8 tg_max(tg_tex8 a, tg_tex8 b) {
  unsigned i;

  for (i = 0; i < TG_TEXSIZE; ++i)
    tg_temp[i] = umax(a[i], b[i]);

  return tg_temp;
}

tg_tex8 tg_min(tg_tex8 a, tg_tex8 b) {
  unsigned i;

  for (i = 0; i < TG_TEXSIZE; ++i)
    tg_temp[i] = umin(a[i], b[i]);

  return tg_temp;
}

tg_tex8 tg_stencil(tg_tex8 bottom, tg_tex8 top, tg_tex8 control,
                   tg_tex8 min, tg_tex8 max) {
  unsigned i;

  for (i = 0; i < TG_TEXSIZE; ++i)
    tg_temp[i] = control[i] >= min[i] && control[i] <= max[i]?
      top[i] : bottom[i];

  return tg_temp;
}

tg_tex8 tg_normalise(tg_tex8 in, unsigned char desired_min,
                     unsigned char desired_max) {
  unsigned multiplier, divisor;
  unsigned found_min = 255, found_max = 0;
  unsigned i, v;

  for (i = 0; i < TG_TEXSIZE; ++i) {
    if (in[i] < found_min) found_min = in[i];
    if (in[i] > found_max) found_max = in[i];
  }

  divisor = found_max - found_min + 1;
  multiplier = desired_max - desired_min + 1;

  for (i = 0; i < TG_TEXSIZE; ++i) {
    v = in[i];
    v -= found_min;
    v *= multiplier;
    v /= divisor;
    v += desired_min;
    tg_temp[i] = clampu(0, v, 255);
  }

  return tg_temp;
}

tg_texrgb tg_zip(tg_tex8 r, tg_tex8 g, tg_tex8 b) {
  unsigned i;

  for (i = 0; i < TG_TEXSIZE; ++i) {
    tg_temp[i*3 + 0] = r[i];
    tg_temp[i*3 + 1] = g[i];
    tg_temp[i*3 + 2] = b[i];
  }

  return tg_temp;
}

tg_texrgb tg_mipmap_maximum(unsigned dim, tg_texrgb in) {
  unsigned x, y, i, xo, yo, r, g, b;

  if (dim > TG_TEXDIM || (dim & 1)) return NULL;

  i = 0;

  for (y = 0; y < dim/2; ++y) {
    for (x = 0; x < dim/2; ++x) {
      for (yo = 0; yo < 2; ++yo) {
        for (xo = 0; xo < 2; ++xo) {
          if ((!xo && !yo) ||
              in[(y*2+yo) * dim * 3 + (x*2+xo) * 3 + 0] > r) {
            r = in[(y*2+yo) * dim * 3 + (x*2+xo) * 3 + 0];
            g = in[(y*2+yo) * dim * 3 + (x*2+xo) * 3 + 1];
            b = in[(y*2+yo) * dim * 3 + (x*2+xo) * 3 + 2];
          }
        }
      }

      tg_temp[i++] = r;
      tg_temp[i++] = g;
      tg_temp[i++] = b;
    }
  }

  return tg_temp;
}
