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

#include "defs.h"
#include "coords.h"
#include "micromp.h"
#include "rand.h"

/* See
   http://en.wikipedia.org/wiki/Mersenne_twister
   for Mersenne Twister algorithm.
 */

void twister_seed(mersenne_twister* twister, unsigned seed) {
  unsigned val, i;
  twister->ix = 0;

  val = twister->state[0] = seed;

  for (i = 1; i < lenof(twister->state); ++i)
    twister->state[i] = val = 0x6C078965 * (val ^ (val >> 30)) + i;
}

static void twister_next(mersenne_twister* twister) {
  unsigned i, j, k, y;
  for (i = 0; i < lenof(twister->state); ++i) {
    j = (i+1) % lenof(twister->state);
    k = (i+397) % lenof(twister->state);

    y = (twister->state[i] & 0x80000000)
      + (twister->state[j] & 0x7FFFFFFF);
    twister->state[i] = twister->state[k] ^ (y >> 1);
    if (y & 1)
      twister->state[i] ^= 0x9908B0DF;
  }
}

unsigned twist(mersenne_twister* twister) {
  unsigned y;

  if (!twister->ix) twister_next(twister);

  y = twister->state[twister->ix++];
  twister->ix %= lenof(twister->state);

  y ^= y >> 11;
  y ^= (y <<  7) & 0x9D2C5680;
  y ^= (y << 15) & 0xEFC60000;
  y ^= y >> 18;
  return y;
}

static unsigned to_amplitude(signed in, unsigned amp) {
  signed long long sll = in;
  sll *= amp/2;
  sll /= ZO_SCALING_FACTOR_MAX * ZO_SCALING_FACTOR_MAX * 2;
  sll += amp/2;
  /* Rounding errors (primarily in generating the vectors) cause sll to
   * sometimes be slightly out of range.
   */
  if (sll < 0)    return 0;
  if (sll >= amp) return amp;
  else            return sll;
}

static signed perlin_dot(const signed short* vectors,
                         unsigned gx, unsigned gy,
                         unsigned gw, unsigned gh,
                         signed vx, signed vy) {
  return
    vx * vectors[gy*gw*2 + gx*2 + 0] +
    vy * vectors[gy*gw*2 + gx*2 + 1];
}

static signed ease(signed long long t_num, signed long long t_denom,
                   signed long long from, signed long long to) {
  signed long long nt_num = t_denom - t_num;
  return
    + 3*nt_num*nt_num*from / (t_denom*t_denom)
    - 2*nt_num*nt_num*nt_num*from / (t_denom*t_denom*t_denom)
    + 3*t_num*t_num*to / (t_denom*t_denom)
    - 2*t_num*t_num*t_num*to / (t_denom*t_denom*t_denom)
    ;
}

static signed perlin_point(unsigned x, unsigned y,
                           unsigned xwl, unsigned ywl,
                           unsigned gw, unsigned gh,
                           const signed short* vectors) {
  unsigned gx0 = x / xwl, gy0 = y / ywl;
  unsigned gx1 = (gx0+1) % gw, gy1 = (gy0+1) % gh;
  signed dx0 = - (x % xwl), dy0 = - (y % ywl);
  signed dx1 = xwl + dx0, dy1 = ywl + dy0;
  signed dot00, dot01, dot10, dot11;
  /* Rescale d* to -16384..+16384 */
  dx0 = dx0 * ZO_SCALING_FACTOR_MAX / (signed)xwl;
  dx1 = dx1 * ZO_SCALING_FACTOR_MAX / (signed)xwl;
  dy0 = dy0 * ZO_SCALING_FACTOR_MAX / (signed)ywl;
  dy1 = dy1 * ZO_SCALING_FACTOR_MAX / (signed)ywl;

  dot00 = perlin_dot(vectors, gx0, gy0, gw, gh, dx0, dy0);
  dot01 = perlin_dot(vectors, gx0, gy1, gw, gh, dx0, dy1);
  dot10 = perlin_dot(vectors, gx1, gy0, gw, gh, dx1, dy0);
  dot11 = perlin_dot(vectors, gx1, gy1, gw, gh, dx1, dy1);

  return ease(-dx0, 16384,
              ease(-dy0, 16384, dot00, dot01),
              ease(-dy0, 16384, dot10, dot11));
}

void perlin_noise(unsigned* dst, unsigned w, unsigned h,
                  unsigned freq, unsigned amp,
                  unsigned seed) {
  signed short vectors[freq*freq*2];
  unsigned xwl = w / freq, ywl = h / freq;
  unsigned x, y;
  angle ang;

  for (x = 0; x < lenof(vectors); x += 2) {
    ang = lcgrand(&seed);
    vectors[x+0] = zo_cos(ang);
    vectors[x+1] = zo_sin(ang);
  }

  for (y = 0; y < h; ++y)
    for (x = 0; x < w; ++x)
      dst[y*w+x] += to_amplitude(
        perlin_point(x, y, xwl, ywl, freq, freq, vectors), amp);
}
