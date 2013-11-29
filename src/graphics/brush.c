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
#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include <string.h>
#include <stdlib.h>

#include "../coords.h"
#include "canvas.h"
#include "abstract.h"
#include "brush.h"

void brush_init(brush_spec* spec) {
  spec->meth.draw_point = (drawing_method_draw_point_t)brush_draw_point;
  spec->meth.draw_line = (drawing_method_draw_line_t)brush_draw_line;
  spec->meth.flush = (drawing_method_flush_t)brush_flush;

  spec->bristles = 32;
  spec->inner_strengthening_chance = 3680;
  spec->outer_strengthening_chance = 0;
  spec->inner_weakening_chance = 0;
  spec->outer_weakening_chance = 4000;
  spec->noise = 0x1;
  spec->size = ZO_SCALING_FACTOR_MAX / 32;
  spec->step = ZO_SCALING_FACTOR_MAX / 1024;
  memset(spec->init_bristles, 0, sizeof(spec->init_bristles));
}

void brush_prep(brush_accum* accum, const brush_spec* spec,
                canvas* dst, unsigned random_seed) {
  unsigned px_per_step = zo_scale(dst->w, spec->step);

  memcpy(accum->bristles, spec->init_bristles, sizeof(accum->bristles));
  accum->dst = dst;
  accum->random_state = random_seed;
  accum->basic_size = zo_scale(dst->w, spec->size);
  if (!px_per_step) px_per_step = 1;
  accum->step_size = ZO_SCALING_FACTOR_MAX / px_per_step;
}

static inline unsigned short accrand(brush_accum* accum) {
  /* Same LCG as glibc, probably a good choice */
  accum->random_state = accum->random_state * 1103515245 + 12345;
  return accum->random_state >> 16;
}

static void advance_step(brush_accum* accum, const brush_spec* spec) {
  unsigned i, di, id, weaken, strengthen;
  unsigned short rnd;

  for (i = 0; i < MAX_BRUSH_BRISTLES; ++i) {
    di = abs(i - MAX_BRUSH_BRISTLES/2);
    id = MAX_BRUSH_BRISTLES/2 - di;
    rnd = accrand(accum);

    weaken = (di*spec->outer_weakening_chance +
              id*spec->inner_weakening_chance) * 2 / MAX_BRUSH_BRISTLES;
    strengthen = (di*spec->outer_strengthening_chance +
                  id*spec->inner_strengthening_chance) * 2 / MAX_BRUSH_BRISTLES;

    if (rnd <= weaken) {
      if (accum->bristles[i] < 255)
        ++accum->bristles[i];
    } else if (rnd <= weaken+strengthen) {
      if (accum->bristles[i])
        --accum->bristles[i];
    }
  }
}

void brush_draw_point(brush_accum* accum, const brush_spec* spec,
                      const vo3 where, zo_scaling_factor weight) {
  /* TODO */
}

void brush_draw_line(brush_accum* accum, const brush_spec* spec,
                     const vo3 from, zo_scaling_factor from_weight,
                     const vo3 to, zo_scaling_factor to_weight) {
  coord_offset lx, ly, px, py, dist, i, t, x, y, z, bx, by, thick;
  unsigned this_step = 0, prev_step = 0, thickf, thickt;
  unsigned bix;
  signed colour;
  unsigned char thickness_to_bristle[accum->basic_size];

  thickf = zo_scale(accum->basic_size, from_weight);
  thickt = zo_scale(accum->basic_size, to_weight);
  if (!thickf) thickf = 1;
  if (!thickt) thickt = 1;

  for (i = 0; i < accum->basic_size; ++i)
    thickness_to_bristle[i] =
      MAX_BRUSH_BRISTLES/2 - spec->bristles/2 +
      i * spec->bristles / accum->basic_size;

  lx = from[0] - to[0];
  ly = from[1] - to[1];
  dist = isqrt(lx*lx + ly*ly);

  if (abs(lx) >= abs(ly)) {
    py = 0;
    px = 1;
  } else {
    py = 1;
    px = 0;
  }

  if (dist) for (i = 0; i <= dist; ++i) {
    this_step = zo_scale(i, accum->step_size);
    if (this_step != prev_step) {
      prev_step = this_step;
      advance_step(accum, spec);
    }

    bx = (i*to[0] + (dist-i)*from[0]) / dist;
    by = (i*to[1] + (dist-i)*from[1]) / dist;
    z  = (i*to[2] + (dist-i)*from[2]) / dist;
    thick = (i*thickt + (dist-i)*thickf) / dist;

    for (t = 0; t < thick; ++t) {
      bix = thickness_to_bristle[t + (accum->basic_size - thick)/2];

      colour = (unsigned /* zero extend */) accum->bristles[bix];
      colour += accrand(accum) & spec->noise;
      if (colour < spec->num_colours) {
        x = bx - (t-thick/2)*ly/dist;
        y = by + (t-thick/2)*lx/dist;

        canvas_write_c(accum->dst, x, y, spec->colours[colour], z);
        canvas_write_c(accum->dst, x+px, y+py, spec->colours[colour], z);
      }
    }
  }
}

void brush_flush(brush_accum* accum, const brush_spec* spec) {
  memcpy(accum->bristles, spec->init_bristles, sizeof(accum->bristles));
}
