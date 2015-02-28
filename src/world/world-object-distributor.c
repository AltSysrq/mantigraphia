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

#include "../alloc.h"
#include "../defs.h"
#include "../math/coords.h"
#include "../math/rand.h"
#include "terrain-tilemap.h"
#include "terrain.h"
#include "env-vmap.h"
#include "nfa-turtle-vmap-painter.h"
#include "world-object-distributor.h"

#define MAX_ELEMENTS 16

typedef enum {
  wodet_ntvp
} wod_element_type;

typedef struct {
  wod_element_type type;
  union {
    struct {
      unsigned nfa, w, h, max_iterations;
    } ntvp;
  } v;
} wod_element;

static const terrain_tilemap* wod_terrain;
static const env_vmap* wod_vmap;
static mersenne_twister wod_twister;
static unsigned* wod_distribution;
static coord wod_min_altitude, wod_max_altitude;
static char wod_permitted_terrain[0x40];
static wod_element wod_elements[MAX_ELEMENTS];
static unsigned wod_num_elements;

void wod_init(const terrain_tilemap* terrain, const env_vmap* vmap,
              unsigned seed) {
  wod_terrain = terrain;
  wod_vmap = vmap;
  twister_seed(&wod_twister, seed);

  if (wod_distribution)
    free(wod_distribution);
  wod_distribution = xmalloc(sizeof(unsigned) * terrain->xmax * terrain->zmax);

  wod_clear();
}

void wod_clear(void) {
  memset(wod_distribution, 0,
         sizeof(unsigned) * wod_terrain->xmax * wod_terrain->zmax);
  wod_min_altitude = 0;
  wod_max_altitude = ~0u;
  memset(wod_permitted_terrain, 0, sizeof(wod_permitted_terrain));
  wod_num_elements = 0;
}

void wod_add_perlin(unsigned wavelength, unsigned amp) {
  unsigned freq = wod_terrain->xmax / wavelength;

  if (freq < 2 || freq > wod_terrain->zmax/2 || freq > wod_terrain->xmax/2)
    return;

  perlin_noise(wod_distribution, wod_terrain->zmax, wod_terrain->xmax,
               freq, amp, twist(&wod_twister));
}

void wod_permit_terrain_type(unsigned type) {
  if (type < lenof(wod_permitted_terrain))
    wod_permitted_terrain[type] = 1;
}

void wod_restrict_altitude(coord min, coord max) {
  wod_min_altitude = min;
  wod_max_altitude = max;
}

unsigned wod_add_ntvp(unsigned nfa, unsigned w, unsigned h,
                      unsigned max_iterations) {
  if (wod_num_elements >= MAX_ELEMENTS)
    return 0;

  wod_elements[wod_num_elements].type = wodet_ntvp;
  wod_elements[wod_num_elements].v.ntvp.nfa = nfa;
  wod_elements[wod_num_elements].v.ntvp.w = w;
  wod_elements[wod_num_elements].v.ntvp.h = h;
  wod_elements[wod_num_elements].v.ntvp.max_iterations = max_iterations;
  ++wod_num_elements;
  return 1;
}

unsigned wod_distribute(unsigned max_instances, unsigned threshold) {
  unsigned long long cost = max_instances;
  unsigned attempt, x, z, w, h, off, type;

  if (!wod_num_elements)
    return 0;

  for (attempt = 0; attempt < max_instances; ++attempt) {
    x = twist(&wod_twister) & (wod_terrain->xmax - 1);
    z = twist(&wod_twister) & (wod_terrain->zmax - 1);
    off = terrain_tilemap_offset(wod_terrain, x, z);

    if (!wod_permitted_terrain[wod_terrain->type[off] >> TERRAIN_SHADOW_BITS]
    ||  wod_terrain->alt[off] * TILE_YMUL < wod_min_altitude
    ||  wod_terrain->alt[off] * TILE_YMUL > wod_max_altitude
    ||  wod_distribution[z*wod_terrain->xmax + x] < threshold)
      continue;

    type = twist(&wod_twister) % wod_num_elements;

    switch (wod_elements[type].type) {
    case wodet_ntvp:
      w = wod_elements[type].v.ntvp.w;
      h = wod_elements[type].v.ntvp.h;
      cost += ntvp_paint(wod_elements[type].v.ntvp.nfa,
                         x, 0, z,
                         (x - w/2) & (wod_terrain->xmax - 1),
                         (z - h/2) & (wod_terrain->zmax - 1),
                         w, h,
                         wod_elements[type].v.ntvp.max_iterations);
      break;
    }
  }

  if (cost > 0xFFFFFFFFLL)
    return ~0u;
  else
    return cost;
}
