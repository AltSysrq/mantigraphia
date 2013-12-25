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
