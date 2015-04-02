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
#include "../micromp.h"
#include "../math/coords.h"
#include "../math/rand.h"
#include "terrain-tilemap.h"
#include "terrain.h"
#include "env-vmap.h"
#include "flower-map.h"
#include "nfa-turtle-vmap-painter.h"
#include "world-object-distributor.h"

#define MAX_ELEMENTS 16

typedef enum {
  wodet_ntvp,
  wodet_flower
} wod_element_type;

typedef struct {
  wod_element_type type;
  union {
    struct {
      unsigned nfa, w, h, max_iterations;
    } ntvp;
    struct {
      flower_type type;
      flower_height minh, hrange;
    } flower;
  } v;
} wod_element;

static const terrain_tilemap* wod_terrain;
static flower_map* wod_flowers;
static mersenne_twister wod_twister;
static unsigned* wod_distribution;
static coord wod_min_altitude, wod_max_altitude;
static char wod_permitted_terrain[0x40];
static wod_element wod_elements[MAX_ELEMENTS];
static unsigned wod_num_elements;

void wod_init(const terrain_tilemap* terrain, flower_map* flowers,
              unsigned seed) {
  wod_terrain = terrain;
  wod_flowers = flowers;
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

unsigned wod_add_flower(flower_type type, coord h0, coord h1) {
  if (wod_num_elements >= MAX_ELEMENTS)
    return 0;

  h0 /= FLOWER_HEIGHT_UNIT;
  h1 /= FLOWER_HEIGHT_UNIT;
  if (!h0 || h1 <= h0 || h0 > 0xFF || h1 > 0xFF) return 0;

  wod_elements[wod_num_elements].type = wodet_flower;
  wod_elements[wod_num_elements].v.flower.type = type;
  wod_elements[wod_num_elements].v.flower.minh = h0;
  wod_elements[wod_num_elements].v.flower.hrange = h1 - h0;
  ++wod_num_elements;
  return 1;
}

static unsigned long long wod_distribute_subregion(
  unsigned max_instances, unsigned threshold,
  coord x0, coord z0, coord xmask, coord zmask,
  mersenne_twister* twister
) {
  unsigned long long cost = max_instances;
  unsigned attempt, subsample, subsamples = 0, x, z, w, h, off, type;

  for (attempt = 0; attempt < max_instances; ++attempt) {
    type = twist(twister) % wod_num_elements;
    switch (wod_elements[type].type) {
    case wodet_ntvp:
      subsamples = 1;
      break;
    case wodet_flower:
      subsamples = 32;
      /* If not light-weight, charge the caller for mixing types */
      /* (If light-weight, this increment is ultimately ignored) */
      cost += 31;
      break;
    }

    for (subsample = 0; subsample < subsamples; ++subsample) {
      x = x0 + (twist(twister) & xmask);
      z = z0 + (twist(twister) & zmask);
      off = terrain_tilemap_offset(wod_terrain, x, z);

      if (!wod_permitted_terrain[wod_terrain->type[off] >> TERRAIN_SHADOW_BITS]
          ||  wod_terrain->alt[off] * TILE_YMUL < wod_min_altitude
          ||  wod_terrain->alt[off] * TILE_YMUL > wod_max_altitude
          ||  wod_distribution[z*wod_terrain->xmax + x] < threshold)
        continue;

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

      case wodet_flower:
        h = wod_elements[type].v.flower.minh +
          twist(twister) % wod_elements[type].v.flower.hrange;
        flower_map_put(wod_flowers, wod_elements[type].v.flower.type, h,
                       x * TILE_SZ + (twist(twister) % TILE_SZ),
                       z * TILE_SZ + (twist(twister) % TILE_SZ));
        break;
      }
    }
  }

  return cost;
}

/**
 * This needs to be a multiple of FLOWER_FHIVE_SIZE for thread-safety.
 */
#define SUBREGION_SIZE 64

static unsigned long long wod_distribute_serial(
  unsigned max_instances, unsigned threshold
) {
  unsigned long long cost = 0;
  unsigned x, z, nx, nz;

  /* Even when operating serially, subdivide for better cache performance. */
  nx = wod_terrain->xmax / SUBREGION_SIZE;
  nz = wod_terrain->zmax / SUBREGION_SIZE;
  for (z = 0; z < nz; ++z)
    for (x = 0; x < nx; ++x)
      cost += wod_distribute_subregion(max_instances / nx / nz, threshold,
                                       x * SUBREGION_SIZE, z * SUBREGION_SIZE,
                                       SUBREGION_SIZE-1, SUBREGION_SIZE-1,
                                       &wod_twister);

  return cost;
}

static unsigned wod_distribute_ump_max_instances;
static unsigned wod_distribute_ump_threshold;
static unsigned* wod_distribute_ump_twister_seeds;

static void wod_distribute_in_ump(unsigned ordinal, unsigned total) {
  mersenne_twister twister;
  unsigned x, z, nx, nz;

  twister_seed(&twister, wod_distribute_ump_twister_seeds[ordinal]);

  nx = wod_terrain->xmax / SUBREGION_SIZE;
  nz = wod_terrain->zmax / SUBREGION_SIZE;
  z = ordinal;

  for (x = 0; x < nx; ++x) {
    wod_distribute_subregion(wod_distribute_ump_max_instances / nx / nz,
                             wod_distribute_ump_threshold,
                             x * SUBREGION_SIZE, z * SUBREGION_SIZE,
                             SUBREGION_SIZE-1, SUBREGION_SIZE-1,
                             &twister);
  }
}

static ump_task wod_distribute_ump_task = {
  wod_distribute_in_ump,
  /* set dynamically */ 0,
  /* sync */ 0
};

static unsigned long long wod_distribute_parallel(
  unsigned max_instances, unsigned threshold
) {
  unsigned nz, i;

  /* Split into rows of subregions and process them in parallel */
  nz = wod_terrain->zmax / SUBREGION_SIZE;
  wod_distribute_ump_max_instances = max_instances;
  wod_distribute_ump_threshold = threshold;
  wod_distribute_ump_task.num_divisions = nz;
  /* Generate a seed for each row so it can use its own private twister */
  wod_distribute_ump_twister_seeds = xmalloc(sizeof(unsigned) * nz);
  for (i = 0; i < nz; ++i)
    wod_distribute_ump_twister_seeds[i] = twist(&wod_twister);

  ump_run_sync(&wod_distribute_ump_task);
  free(wod_distribute_ump_twister_seeds);

  /* All lightweight elements are cost 1 */
  return max_instances;
}

/**
 * Returns whether the current set of types is lightweight; ie, can be
 * parallelised.
 *
 * All lightweight types have cost 1 when processed in lightweight mode.
 */
static int wod_is_lightweight(void) {
  unsigned i;

  for (i = 0; i < wod_num_elements; ++i) {
    switch (wod_elements[i].type) {
    case wodet_ntvp:
      /* May trigger uMP calls */
      return 0;

    default: /* lightweight */ break;
    }
  }

  return 1;
}

unsigned wod_distribute(unsigned max_instances, unsigned threshold) {
  unsigned long long cost;

  if (!wod_num_elements)
    return 0;

  if (wod_is_lightweight())
    cost = wod_distribute_parallel(max_instances, threshold);
  else
    cost = wod_distribute_serial(max_instances, threshold);

  return cost > 0xFFFFFFFFLL? ~0u : cost;
}
