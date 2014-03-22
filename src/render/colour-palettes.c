/*-
 * Copyright (c) 2014 Jason Lingle
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

#include "../defs.h"
#include "../simd.h"
#include "../frac.h"
#include "context.h"
#include "colour-palettes.h"

static const unsigned terrain_palettes[10][6*4] = {
  /*
    snow0     snow1     snow2     snow3
    stone0    stone1    stone2    stone3
    grass0    grass1    grass2    grass3
    bgrass0   bgrass1   bgrass2   bgrass3
    gravel0   gravel1   gravel2   gravel3
    water0    water1    water2    water3
  */
  /* M */ {
    0xffffff, 0xc8c8c8, 0xbbbbbb, 0x989898,
    0x646464, 0x4b4b50, 0x323237, 0x19191e,
    0x185010, 0x103c0c, 0xbbbbbb, 0x989898,
    0x185010, 0xc8c8c8, 0xbbbbbb, 0x989898,
    0xffffff, 0xc8c8c8, 0xbbbbbb, 0x989898,
    0x404864, 0x30364b, 0x202432, 0x101219,
  }, /* A */ {
    0xffffff, 0xc8c8c8, 0xbbbbbb, 0x989898,
    0x646464, 0x4b4b50, 0x323237, 0x19191e,
    0x185010, 0x103c0c, 0x0c2808, 0x989898,
    0x185010, 0x103c0c, 0xbbbbbb, 0x989898,
    0x624800, 0x4d3800, 0x332500, 0x1a1300,
    0x404864, 0x30364b, 0x202432, 0x101219,
  }, /* M */ {
    0xffffff, 0xc8c8c8, 0xbbbbbb, 0x989898,
    0x646464, 0x4b4b50, 0x323237, 0x19191e,
    0x185010, 0x103c0c, 0x0c2808, 0x081404,
    0x185010, 0x103c0c, 0x0c2808, 0x989898,
    0x645614, 0x4b400f, 0x322b0a, 0x191508,
    0x404864, 0x30364b, 0x202432, 0x101219,
  }, /* J */ {
    0xffffff, 0xc8c8c8, 0xbbbbbb, 0x989898,
    0x646464, 0x4b4b50, 0x323237, 0x19191e,
    0x185010, 0x103c0c, 0x0c2808, 0x081404,
    0x13400d, 0x10300c, 0x0c2808, 0x081404,
    0x645614, 0x4b400f, 0x322b0a, 0x191508,
    0x404864, 0x30364b, 0x202432, 0x101219,
  }, /* J */ {
    0xffffff, 0xc8c8c8, 0xbbbbbb, 0x989898,
    0x646464, 0x4b4b50, 0x323237, 0x19191e,
    0x13400d, 0x10300c, 0x0c2808, 0x081404,
    0x13400d, 0x10300c, 0x0c2808, 0x081404,
    0x645614, 0x4b400f, 0x322b0a, 0x191508,
    0x404864, 0x30364b, 0x202432, 0x101219,
  }, /* A */ {
    0xffffff, 0xc8c8c8, 0xbbbbbb, 0x989898,
    0x646464, 0x4b4b50, 0x323237, 0x19191e,
    0x496614, 0x374d0f, 0x24330a, 0x121a05,
    0x496614, 0x374d0f, 0x24330a, 0x121a05,
    0x645614, 0x4b400f, 0x322b0a, 0x191508,
    0x404864, 0x30364b, 0x202432, 0x101219,
  }, /* S */ {
    0xffffff, 0xc8c8c8, 0xbbbbbb, 0x989898,
    0x646464, 0x4b4b50, 0x323237, 0x19191e,
    0x185010, 0x103c0c, 0x0c2808, 0x081404,
    0x185010, 0x103c0c, 0x0c2808, 0x081404,
    0x645614, 0x4b400f, 0x322b0a, 0x191508,
    0x404864, 0x30364b, 0x202432, 0x101219,
  }, /* O */ {
    0xffffff, 0xc8c8c8, 0xbbbbbb, 0x989898,
    0x646464, 0x4b4b50, 0x323237, 0x19191e,
    0x185010, 0x103c0c, 0x0c2808, 0x081404,
    0x185010, 0x103c0c, 0x0c2808, 0x081404,
    0x645614, 0x4b400f, 0x322b0a, 0x191508,
    0x404864, 0x30364b, 0x202432, 0x101219,
  }, /* N */ {
    0xffffff, 0xc8c8c8, 0xbbbbbb, 0x989898,
    0x646464, 0x4b4b50, 0x323237, 0x19191e,
    0x185010, 0x103c0c, 0x0c2808, 0x081404,
    0x185010, 0x103c0c, 0x0c2808, 0x081404,
    0x645614, 0x4b400f, 0x322b0a, 0x191508,
    0x404864, 0x30364b, 0x202432, 0x101219,
  }, /* D */ {
    0xffffff, 0xc8c8c8, 0xbbbbbb, 0x989898,
    0x646464, 0x4b4b50, 0x323237, 0x19191e,
    0x185010, 0x103c0c, 0x0c2808, 0x081404,
    0x185010, 0x103c0c, 0x0c2808, 0x081404,
    0x645614, 0x4b400f, 0x322b0a, 0x191508,
    0x404864, 0x30364b, 0x202432, 0x101219,
  },
};

static void interpolate_simd(simd4* dst,
                             const unsigned* src1, const unsigned* src2,
                             unsigned n, fraction p) {
  unsigned i, r, g, b;

  for (i = 0; i < n; ++i) {
    r = fraction_umul((src1[i] & 0xFF0000) >> 16, fraction_of(1) - p)
      + fraction_umul((src2[i] & 0xFF0000) >> 16, p);
    g = fraction_umul((src1[i] & 0x00FF00) >>  8, fraction_of(1) - p)
      + fraction_umul((src2[i] & 0x00FF00) >>  8, p);
    b = fraction_umul((src1[i] & 0x0000FF) >>  0, fraction_of(1) - p)
      + fraction_umul((src2[i] & 0x0000FF) >>  0, p);

    dst[i] = simd_initl(r,g,b,0);
  }
}

/* Since we need simd4-alignment for the inner struct, we need to add some
 * space so we can adjust the pointer as necessary.
 */
struct colour_palettes_with_padding {
  simd4 padding;
  colour_palettes palettes;
};

static inline void* align_to_simd4(const void* vinput) {
  char* input = (void*)vinput;
  unsigned long long as_int;
  unsigned misalignment;

  as_int = (unsigned long long)input;
  misalignment = as_int & (sizeof(simd4) - 1);
  if (misalignment)
    input += sizeof(simd4) - misalignment;

  return input;
}

RENDERING_CONTEXT_STRUCT(colour_palettes, struct colour_palettes_with_padding)
const colour_palettes* get_colour_palettes(
  const rendering_context*restrict context)
{
  return align_to_simd4(colour_palettes_get(context));
}

void colour_palettes_context_set(rendering_context*restrict context) {
  colour_palettes* this = align_to_simd4(colour_palettes_getm(context));
  const rendering_context_invariant*restrict inv = CTXTINV(context);
  interpolate_simd(this->terrain,
                   terrain_palettes[inv->month_integral+0],
                   terrain_palettes[inv->month_integral+1],
                   lenof(this->terrain),
                   inv->month_fraction);
}
