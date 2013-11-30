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

#define SPLOTCH_DIM 64
#define NUM_SPLOTCHES 32

unsigned char splotches[NUM_SPLOTCHES][SPLOTCH_DIM*SPLOTCH_DIM];

static void generate_splotch(unsigned char splotch[SPLOTCH_DIM*SPLOTCH_DIM]) {
  unsigned char tmp[SPLOTCH_DIM*SPLOTCH_DIM];
  unsigned bristle, x, y, rad, which;
  signed px, py;
  angle ang;
  int done = 0;

  /* Initialise to empty */
  memset(splotch, ~0, sizeof(tmp));

  /* Seed each bristle randomly, with a radius proportional to the bristle's
   * distance from the centre.
   */
  for (bristle = 0; bristle < MAX_BRUSH_BRISTLES; ++bristle) {
    rad = 6 + abs(bristle - MAX_BRUSH_BRISTLES/2);
    do {
      /* Call rand twice to account for 15-bit PRNGs */
      ang = rand() ^ (rand() << 15);
      cossinms(&px, &py, ang, rad);
      px += SPLOTCH_DIM / 2;
      py += SPLOTCH_DIM / 2;
    } while (255 != splotch[px + py*SPLOTCH_DIM]);

    splotch[px + py*SPLOTCH_DIM] = bristle;
  }

  /* Each iteration, every pixel with no bristle has a 50% chance of taking a
   * value from one of its neighbours. If it does so, it chooses from one of
   * its neighbours at random. The iterations cease once any of the edge pixels
   * obtain a bristle.
   */
  do {
    memcpy(tmp, splotch, sizeof(tmp));

    for (y = 0; y < SPLOTCH_DIM; ++y) {
      for (x = 0; x < SPLOTCH_DIM; ++x) {
        if (255 == splotch[x + y*SPLOTCH_DIM] && !(rand()&1)) {
          which = rand() % 8;
          if (which >= 4) ++which; /* Skip self */
          px = x + which%3 - 1;
          py = y + which/3 - 1;

          if (px >= 0 && px < SPLOTCH_DIM &&
              py >= 0 && py < SPLOTCH_DIM) {
            splotch[x + y*SPLOTCH_DIM] = tmp[px + py*SPLOTCH_DIM];
            if ((0 == x || 0 == y || SPLOTCH_DIM == x+1 || SPLOTCH_DIM == y+1) &&
                255 != splotch[x + y*SPLOTCH_DIM])
              done = 1;
          }
        }
      }
    }
  } while (!done);
}

void brush_load(void) {
  unsigned splotch;

  for (splotch = 0; splotch < NUM_SPLOTCHES; ++splotch)
    generate_splotch(splotches[splotch]);
}

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
  spec->step = ZO_SCALING_FACTOR_MAX / 512;
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
  accum->has_endpoint = 0;
}

#ifdef PROFILE
static unsigned short accrand(brush_accum*)
__attribute__((noinline));
#endif
static
#ifndef PROFILE
inline
#endif
unsigned short accrand(brush_accum* accum) {
  /* Same LCG as glibc, probably a good choice */
  accum->random_state = accum->random_state * 1103515245 + 12345;
  return accum->random_state >> 16;
}

#ifdef PROFILE
static void advance_step(brush_accum*, const brush_spec*)
__attribute__((noinline));
#endif

static void advance_step(brush_accum* accum, const brush_spec* spec) {
  unsigned i, ii, di, id, weaken, strengthen;
  unsigned short rnda, rndb;

  for (i = 0; i < MAX_BRUSH_BRISTLES/2; ++i) {
    id = i;
    di = MAX_BRUSH_BRISTLES/2 - id;
    ii = MAX_BRUSH_BRISTLES-1 - i;
    rnda = accrand(accum);
    rndb = accrand(accum);

    weaken = (di*spec->outer_weakening_chance +
              id*spec->inner_weakening_chance) * 2 / MAX_BRUSH_BRISTLES;
    strengthen = (di*spec->outer_strengthening_chance +
                  id*spec->inner_strengthening_chance) * 2 / MAX_BRUSH_BRISTLES;

    if (rnda <= weaken) {
      ++accum->bristles[i];
    } else if (rnda <= weaken+strengthen) {
      if (accum->bristles[i])
        --accum->bristles[i];
    }
    if (rndb <= weaken) {
      ++accum->bristles[ii];
    } else if (rndb <= weaken+strengthen) {
      if (accum->bristles[ii])
        --accum->bristles[ii];
    }
  }
}

static void draw_splotch(brush_accum*restrict accum,
                         const brush_spec*restrict spec,
                         const vo3 where,
                         zo_scaling_factor rot_cos,
                         zo_scaling_factor rot_sin,
                         signed xdiam,
                         signed ydiam,
                         unsigned max_bristle) {
  const unsigned char* splotch = splotches[accrand(accum) % NUM_SPLOTCHES];
  unsigned bristle, colour;
  signed ixdiam16 = SPLOTCH_DIM * 65536 / xdiam;
  signed iydiam16 = SPLOTCH_DIM * 65536 / ydiam;
  coord_offset x, y, cx, cy, tx, ty, sx, sy;

  for (y = 0; y < ydiam; ++y) {
    sy = y * iydiam16 >> 16;
    for (x = 0; x < xdiam; ++x) {
      sx = x * ixdiam16 >> 16;

      bristle = splotch[sx + sy*SPLOTCH_DIM];
      if (bristle >= max_bristle) continue; /* no bristle */

      colour = accum->bristles[bristle];
      if (colour > spec->num_colours) continue; /* dead bristle */

      colour += accrand(accum) & spec->noise;
      if (colour > spec->num_colours) continue; /* dead bristle (noise) */

      cx = (x - xdiam/2);
      cy = (y - ydiam/2);

      tx = zo_scale(cx, rot_cos) - zo_scale(cy, rot_sin);
      ty = zo_scale(cy, rot_cos) + zo_scale(cx, rot_sin);
      tx += where[0];
      ty += where[1];

      canvas_write_c(accum->dst, tx, ty,
                     spec->colours[colour], where[2]);
    }
  }
}

void brush_draw_point(brush_accum*restrict accum,
                      const brush_spec*restrict spec,
                      const vo3 where, zo_scaling_factor weight) {
  draw_splotch(accum, spec, where,
               ZO_SCALING_FACTOR_MAX, 0,
               accum->basic_size, accum->basic_size,
               zo_scale(spec->bristles, weight));
  advance_step(accum, spec);
}

static void draw_line_endpoint(brush_accum*restrict accum,
                               const brush_spec*restrict spec,
                               const brush_accum_point* point) {
  signed dist = isqrt(point->dx * point->dx + point->dy * point->dy);
  zo_scaling_factor cos = ZO_SCALING_FACTOR_MAX * point->dx / dist;
  zo_scaling_factor sin = ZO_SCALING_FACTOR_MAX * point->dy / dist;

  draw_splotch(accum, spec, point->where,
               cos, sin,
               point->thickness / 3, point->thickness,
               point->num_bristles);
}

void brush_draw_line(brush_accum*restrict accum,
                     const brush_spec*restrict spec,
                     const vo3 from, zo_scaling_factor from_weight,
                     const vo3 to, zo_scaling_factor to_weight) {
  coord_offset lx, ly, dist, t, x, y, z, bx, by, thick;
  coord_offset lxd16, lyd16;
  unsigned this_step = 0, prev_step = 0, thickf, thickt;
  unsigned i, bix;
  unsigned colour;
  unsigned char thickness_to_bristle[accum->basic_size];
  unsigned short noise;
  brush_accum_point startpoint;

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
  if (!dist) return; /* nothing to draw */
  lxd16 = 65536 * lx / dist;
  lyd16 = 65536 * ly / dist;

  /* If there is no previous endpoint, or it did not start here, draw it now */
  if (accum->has_endpoint &&
      memcmp(accum->prev_endpoint.where, from, sizeof(vo3))) {
    draw_line_endpoint(accum, spec, &accum->prev_endpoint);
    accum->has_endpoint = 0;
  }

  /* If we are not connecting another endpoint, start with a splotch */
  if (!accum->has_endpoint) {
    memcpy(startpoint.where, from, sizeof(vo3));
    startpoint.dx = to[0] - from[0];
    startpoint.dy = to[1] - from[1];
    startpoint.thickness = thickf;
    startpoint.num_bristles = zo_scale(spec->bristles, from_weight);
    draw_line_endpoint(accum, spec, &startpoint);
  }

  /* Record our endpoint */
  memcpy(accum->prev_endpoint.where, to, sizeof(vo3));
  accum->prev_endpoint.dx = from[0] - to[0];
  accum->prev_endpoint.dy = from[1] - to[1];
  accum->prev_endpoint.thickness = thickt;
  accum->prev_endpoint.num_bristles = zo_scale(spec->bristles, to_weight);
  accum->has_endpoint = 1;

  for (i = 0; i <= (unsigned)dist; ++i) {
    this_step = zo_scale(i, accum->step_size);
    if (this_step != prev_step) {
      prev_step = this_step;
      advance_step(accum, spec);
    }

    bx = (i*to[0] + (dist-i)*from[0]) / dist;
    by = (i*to[1] + (dist-i)*from[1]) / dist;
    z  = (i*to[2] + (dist-i)*from[2]) / dist;
    thick = (i*thickt + (dist-i)*thickf) / dist;
    noise = accrand(accum);

    for (t = 0; t < thick; ++t) {
      bix = thickness_to_bristle[t + (accum->basic_size - thick)/2];

      colour = (unsigned /* zero extend */) accum->bristles[bix];
      colour += noise & spec->noise;
      noise = (noise >> 1) | (noise << 15);
      if (colour < spec->num_colours) {
        x = bx - ((t-thick/2) * lyd16 >> 16);
        y = by + ((t-thick/2) * lxd16 >> 16);

        canvas_write_c(accum->dst, x, y, spec->colours[colour], z);
        canvas_write_c(accum->dst, x+1, y+0, spec->colours[colour], z);
      }
    }
  }
}

void brush_flush(brush_accum* accum, const brush_spec* spec) {
  if (accum->has_endpoint)
    draw_line_endpoint(accum, spec, &accum->prev_endpoint);

  memcpy(accum->bristles, spec->init_bristles, sizeof(accum->bristles));
  accum->has_endpoint = 0;
}
