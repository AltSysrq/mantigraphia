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
    0x496614, 0x374d0f, 0x24330a, 0x121a05,
    0x496614, 0x374d0f, 0x24330a, 0x121a05,
    0x645614, 0x4b400f, 0x322b0a, 0x191508,
    0x404864, 0x30364b, 0x202432, 0x101219,
  }, /* O */ {
    0xffffff, 0xc8c8c8, 0xbbbbbb, 0x989898,
    0x646464, 0x4b4b50, 0x323237, 0x19191e,
    0x496614, 0x302a0e, 0x501810, 0x280c08,
    0x496614, 0x304612, 0x505018, 0x28280c,
    0x645614, 0x4b400f, 0x322b0a, 0x191508,
    0x404864, 0x30364b, 0x202432, 0x101219,
  }, /* N */ {
    0xffffff, 0xc8c8c8, 0xbbbbbb, 0x989898,
    0x646464, 0x4b4b50, 0x323237, 0x19191e,
    0xffffff, 0x103c0c, 0x0c2808, 0x081404,
    0xffffff, 0xc8c8c8, 0x0c2808, 0x081404,
    0xffffff, 0xc8c8c8, 0xbbbbbb, 0x989898,
    0x8196b1, 0x607084, 0x404b58, 0x20252c,
  }, /* D */ {
    0xffffff, 0xc8c8c8, 0xbbbbbb, 0x989898,
    0x646464, 0x4b4b50, 0x323237, 0x19191e,
    0xffffff, 0xc8c8c8, 0xbbbbbb, 0x989898,
    0xffffff, 0xc8c8c8, 0xbbbbbb, 0x989898,
    0xffffff, 0xc8c8c8, 0xbbbbbb, 0x989898,
    0xc3e5ff, 0x92abbf, 0x61727f, 0x30393f,
  },
};

static const unsigned oak_leaf_palettes[10][8] = {
  /* M */ {
    0xffffff, 0xffffff, 0x003000, 0xffffff,
    0x144010, 0x144010, 0x103408, 0x103408,
  },
  /* A */ {
    0xffffff, 0xffffff, 0x003000, 0x003000,
    0x144010, 0x144010, 0x103408, 0x103408,
  },
  /* M */ {
    0x003000, 0x002000, 0x004800, 0x003000,
    0x1e6018, 0x144010, 0x184e0c, 0x103408,
  },
  /* J */ {
    0x003000, 0x003000, 0x004800, 0x004800,
    0x1e6018, 0x1e6018, 0x184e0c, 0x184e0c,
  },
  /* J */ {
    0x003000, 0x002000, 0x004800, 0x003000,
    0x1e6018, 0x144010, 0x184e0c, 0x103408,
  },
  /* A */ {
    0x002000, 0x002000, 0x003000, 0x003000,
    0x144010, 0x144010, 0x103408, 0x103408,
  },
  /* S */ {
    0x002000, 0x002000, 0x003000, 0x003000,
    0x144010, 0x144010, 0x103408, 0x103408,
  },
  /* O */ {
    0xa00000, 0xa00000, 0xa00000, 0xa02000,
    0xa04000, 0xa06000, 0xa08000, 0xa0a000,
  },
  /* N */ {
    0xffffff, 0xffffff, 0xffffff, 0xffffff,
    0x704020, 0x705030, 0x706040, 0x707050,
  },
  /* D */ {
    0xffffff, 0xffffff, 0xffffff, 0xffffff,
    0xffffff, 0xffffff, 0xffffff, 0xffffff,
  },
};

static const unsigned oak_trunk_palettes[10][10] = {
  /* M */ {
    0xffffff, 0xffffff, 0xffffff, 0x302020, 0x636357,
    0x303020, 0x302800, 0x303020, 0x302000, 0x303020,
  },
  /* A */ {
    0x000000, 0x000000, 0x201c00, 0x303020, 0x302000,
    0x303020, 0x302800, 0x303020, 0x302000, 0x303020,
  },
  /* M */ {
    0x000000, 0x000000, 0x201c00, 0x303020, 0x302000,
    0x303020, 0x302800, 0x303020, 0x302000, 0x303020,
  },
  /* J */ {
    0x000000, 0x000000, 0x201c00, 0x303020, 0x302000,
    0x303020, 0x302800, 0x303020, 0x302000, 0x303020,
  },
  /* J */ {
    0x000000, 0x000000, 0x201c00, 0x303020, 0x302000,
    0x303020, 0x302800, 0x303020, 0x302000, 0x303020,
  },
  /* A */ {
    0x000000, 0x000000, 0x201c00, 0x303020, 0x302000,
    0x303020, 0x302800, 0x303020, 0x302000, 0x303020,
  },
  /* S */ {
    0x000000, 0x000000, 0x201c00, 0x303020, 0x302000,
    0x303020, 0x302800, 0x303020, 0x302000, 0x303020,
  },
  /* O */ {
    0x000000, 0x000000, 0x201c00, 0x303020, 0x302000,
    0x303020, 0x302800, 0x303020, 0x302000, 0x303020,
  },
  /* N */ {
    0xffffff, 0xffffff, 0xffffff, 0x302020, 0x636357,
    0x303020, 0x302800, 0x303020, 0x302000, 0x303020,
  },
  /* D */ {
    0xffffff, 0xffffff, 0xffffff, 0x302020, 0xffffff,
    0x303020, 0x302800, 0x303020, 0x302000, 0x303020,
  },
};

static const unsigned cherry_leaf_palettes[10][8] = {
  /* M */ {
    0xffffff, 0xffffff, 0x003800, 0xffffff,
    0x145010, 0x143010, 0x104408, 0x104408,
  },
  /* A */ {
    0x184e0c, 0x103408, 0xffeeee, 0xffdddd,
    0xffcccc, 0xbb6666, 0xaa6655, 0xeebbbb,
  },
  /* M */ {
    0x003000, 0x002000, 0xb66666, 0xa66655,
    0x1e8018, 0x146010, 0x186e0c, 0x105408,
  },
  /* J */ {
    0x003000, 0x003000, 0x306020, 0x305000,
    0x1e8018, 0x1e8018, 0x186e0c, 0x186e0c,
  },
  /* J */ {
    0x003000, 0x002000, 0x004800, 0x204204,
    0x1e8018, 0x148010, 0x186e0c, 0x106408,
  },
  /* A */ {
    0x002000, 0x002000, 0x003000, 0x302000,
    0x144010, 0x144010, 0x103408, 0x103408,
  },
  /* S */ {
    0x002000, 0x002000, 0x204204, 0x302000,
    0x606010, 0x606010, 0x103408, 0x103408,
  },
  /* O */ {
    0x002000, 0x002000, 0x305204, 0x403000,
    0x707010, 0x707010, 0x303408, 0x303408,
  },
  /* N */ {
    0xffffff, 0xffffff, 0xffffff, 0xffffff,
    0x704020, 0x705030, 0x706040, 0x707050,
  },
  /* D */ {
    0xffffff, 0xffffff, 0xffffff, 0xffffff,
    0xffffff, 0xffffff, 0xffffff, 0xffffff,
  },
};

static const unsigned cherry_trunk_palettes[10][10] = {
  /* M */ {
    0xffffff, 0xffffff, 0xffffff, 0x645c5c, 0x7e7e78,
    0x64645c, 0x302800, 0x64645c, 0x302000, 0x64645c,
  },
  /* A */ {
    0x000000, 0x000000, 0x201c00, 0x64645c, 0x302000,
    0x64645c, 0x302800, 0x64645c, 0x302000, 0x64645c,
  },
  /* M */ {
    0x000000, 0x000000, 0x201c00, 0x64645c, 0x302000,
    0x64645c, 0x302800, 0x64645c, 0x302000, 0x64645c,
  },
  /* J */ {
    0x000000, 0x000000, 0x201c00, 0x64645c, 0x302000,
    0x64645c, 0x302800, 0x64645c, 0x302000, 0x64645c,
  },
  /* J */ {
    0x000000, 0x000000, 0x201c00, 0x64645c, 0x302000,
    0x64645c, 0x302800, 0x64645c, 0x302000, 0x64645c,
  },
  /* A */ {
    0x000000, 0x000000, 0x201c00, 0x64645c, 0x302000,
    0x64645c, 0x302800, 0x64645c, 0x302000, 0x64645c,
  },
  /* S */ {
    0x000000, 0x000000, 0x201c00, 0x64645c, 0x302000,
    0x64645c, 0x302800, 0x64645c, 0x302000, 0x64645c,
  },
  /* O */ {
    0x000000, 0x000000, 0x201c00, 0x64645c, 0x302000,
    0x64645c, 0x302800, 0x64645c, 0x302000, 0x64645c,
  },
  /* N */ {
    0xffffff, 0xffffff, 0xffffff, 0x645c5c, 0x7e7e78,
    0x64645c, 0x302800, 0x64645c, 0x302000, 0x64645c,
  },
  /* D */ {
    0xffffff, 0xffffff, 0xffffff, 0x645c5c, 0xffffff,
    0x64645c, 0x302800, 0x64645c, 0x302000, 0x64645c,
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

static void interpolate_px(canvas_pixel* dst,
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

    dst[i] = argb(255,r,g,b);
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
  unsigned ma = inv->month_integral+0;
  unsigned mb = inv->month_integral+1;
  interpolate_simd(this->terrain,
                   terrain_palettes[ma], terrain_palettes[mb],
                   lenof(this->terrain), inv->month_fraction);
  interpolate_px(this->oak_leaf,
                 oak_leaf_palettes[ma], oak_leaf_palettes[mb],
                 lenof(this->oak_leaf), inv->month_fraction);
  interpolate_px(this->oak_trunk,
                 oak_trunk_palettes[ma], oak_trunk_palettes[mb],
                 lenof(this->oak_trunk), inv->month_fraction);
  interpolate_px(this->cherry_leaf,
                 cherry_leaf_palettes[ma], cherry_leaf_palettes[mb],
                 lenof(this->cherry_leaf), inv->month_fraction);
  interpolate_px(this->cherry_trunk,
                 cherry_trunk_palettes[ma], cherry_trunk_palettes[mb],
                 lenof(this->cherry_trunk), inv->month_fraction);
}
