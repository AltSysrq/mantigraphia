/*-
 * Copyright (c) 2013, 2014, 2015 Jason Lingle
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

#include <stdio.h>
#include <string.h>

#include "../alloc.h"
#include "../micromp.h"
#include "../math/coords.h"
#include "../math/rand.h"
#include "../defs.h"
#include "../micromp.h"
#include "terrain-tilemap.h"
#include "terrain.h"
#include "env-vmap.h"
#include "generate.h"

static void initialise(terrain_tilemap*);
static void randomise(terrain_tilemap*, signed, mersenne_twister*);
static void rmp_up(terrain_tilemap* large, const terrain_tilemap* small,
                   signed level, mersenne_twister*);
static void generate_level(terrain_tilemap*, signed level, mersenne_twister*);
static void select_terrain(terrain_tilemap*, mersenne_twister*);
static void create_path_to_from(terrain_tilemap*, mersenne_twister*,
                                coord xto, coord zto,
                                coord xfrom, coord zfrom);
static void world_add_shadow_subregion(
  terrain_tilemap*, const env_vmap*,
  coord x0, coord z0, coord xs, coord zs,
  coord xmask, coord zmask);

void world_generate(terrain_tilemap* world, unsigned seed) {
  mersenne_twister twister;
  coord xs[5], zs[5], i, j;

  twister_seed(&twister, seed);

  generate_level(world, 0, &twister);
  select_terrain(world, &twister);

  xs[0] = zs[0] = 0;
  for (i = 1; i < lenof(xs); ++i) {
    xs[i] = (twist(&twister) >> 16) & (world->xmax-1);
    zs[i] = (twist(&twister) >> 16) & (world->zmax-1);
  }

  for (i = 0; i < lenof(xs)-1; ++i)
    for (j = i+1; j < lenof(xs); ++j)
      if (twist(&twister) & 1)
        create_path_to_from(world, &twister, xs[i], zs[i], xs[j], zs[j]);
      else
        create_path_to_from(world, &twister, xs[j], zs[j], xs[i], zs[i]);

  terrain_tilemap_calc_next(world);
}

static int above_perlin_threshold(const terrain_tilemap* world) {
  return world->xmax > 512 && world->zmax > 512;
}

static void generate_level(terrain_tilemap* world, signed level,
                           mersenne_twister* twister) {
  terrain_tilemap* next;

  printf("Generating level %d\n", level);

  next = SLIST_NEXT(world, next);
  if (next && above_perlin_threshold(world)) {
    initialise(world);
    generate_level(next, level+1, twister);
    rmp_up(world, next, level, twister);
  } else {
    randomise(world, level, twister);
  }
}

static void initialise(terrain_tilemap* world) {
  unsigned i, max;

  max = world->xmax * world->zmax;
  for (i = 0; i < max; ++i) {
    world->type[i] = terrain_type_grass << TERRAIN_SHADOW_BITS;
  }
}

static void randomise(terrain_tilemap* world,
                      signed level,
                      mersenne_twister* twister) {
  unsigned* hmap, hmap_size, i, freq, amp, altitude_reduction;

  hmap_size = sizeof(unsigned) * world->xmax * world->zmax;
  hmap = xmalloc(hmap_size);
  memset(hmap, 0, hmap_size);

  freq = 2;
  amp = 128*METRE / TILE_YMUL;
  altitude_reduction = amp * 6 / 10;

  do {
    perlin_noise(hmap, world->xmax, world->zmax, freq, amp, twist(twister));

    freq *= 2;
    amp /= 2;
  } while (amp && freq < world->xmax && freq < world->zmax);

  initialise(world);
  for (i = 0; i < world->xmax * world->zmax; ++i)
    if (hmap[i] < altitude_reduction)
      world->alt[i] = 0;
    else
      world->alt[i] = hmap[i] - altitude_reduction;

  free(hmap);
}

static inline signed altitude(const terrain_tilemap* world,
                              coord x, coord z) {
  signed s = world->alt[terrain_tilemap_offset(world, x, z)];

  return s & 0xFFFF;
}

static inline unsigned short perturb(signed base_altitude,
                                     signed level,
                                     mersenne_twister* twister) {
  if (level <= 1) return base_altitude;
  base_altitude -= 1 << level;
  base_altitude += twist(twister) & ((2 << (level-1)) - 1);
  if (base_altitude < 0) return 0;
  if (base_altitude > 32767) return 32767;
  return base_altitude;
}

static void rmp_up(terrain_tilemap* large,
                   const terrain_tilemap* small,
                   signed level,
                   mersenne_twister* twister) {
  coord sx0, sz0, lx0, lz0, sx1, sz1, lx1, lz1;
  signed sa00, sa01, sa10, sa11;

  for (sz0 = 0; sz0 < small->zmax; ++sz0) {
    sz1 = (sz0+1) & (small->zmax-1);
    lz0 = sz0 * 2;
    lz1 = lz0+1; /* won't ever wrap */
    for (sx0 = 0; sx0 < small->xmax; ++sx0) {
      sx1 = (sx0+1) & (small->xmax-1);
      lx0 = sx0 * 2;
      lx1 = lx0+1; /* won't ever wrap */

      sa00 = altitude(small, sx0, sz0);
      sa10 = altitude(small, sx1, sz0);
      sa01 = altitude(small, sx0, sz1);
      sa11 = altitude(small, sx1, sz1);

      /* Exactly-matching points never change */
      large->alt[terrain_tilemap_offset(large, lx0, lz0)] =
        perturb(sa00, level, twister);

      /* Other points get perturbed averages of what they are in-between. */
      large->alt[terrain_tilemap_offset(large, lx0, lz1)] =
        perturb((sa00+sa01)/2, level, twister);
      large->alt[terrain_tilemap_offset(large, lx1, lz0)] =
        perturb((sa00+sa10)/2, level, twister);
      large->alt[terrain_tilemap_offset(large, lx1, lz1)] =
        perturb((sa00+sa01+sa10+sa11)/4, level, twister);
    }
  }
}

static void select_terrain(terrain_tilemap* world, mersenne_twister* twister) {
  unsigned x, z, i, dx, dz;
  coord_offset miny, maxy, y;

  for (z = 0; z < world->zmax; ++z) {
    for (x = 0; x < world->xmax; ++x) {
      i = terrain_tilemap_offset(world, x, z);

      if (world->alt[i] <= 2*METRE / TILE_YMUL) {
        world->type[i] =
          terrain_type_water << TERRAIN_SHADOW_BITS;
      } else if (world->alt[i] <= 4*METRE / TILE_YMUL) {
        world->type[i] =
          terrain_type_gravel << TERRAIN_SHADOW_BITS;
      /* Sometimes patches of snow, depending on altitude */
      } else if (((signed)(twist(twister)/2)) <
          world->alt[i] * TILE_YMUL) {
        world->type[i] = terrain_type_snow << TERRAIN_SHADOW_BITS;
      } else {
        /* Stone if max dy*2 > dx, grass otherwise */
        miny = 32767 * TILE_YMUL;
        maxy = 0;
        for (dz = 0; dz < 2; ++dz) {
          for (dx = 0; dx < 2; ++dx) {
            y = world->alt[terrain_tilemap_offset(
                world, (x+dx) & (world->xmax-1), (z+dz) & (world->zmax-1))]
              * TILE_YMUL;
            if (y > maxy)
              maxy = y;
            if (y < miny)
              miny = y;
          }
        }

        if (maxy - miny > TILE_SZ/2)
          world->type[i] =
            terrain_type_stone << TERRAIN_SHADOW_BITS;
        else if (twist(twister) & 7)
          world->type[i] =
            terrain_type_bare_grass << TERRAIN_SHADOW_BITS;
        else
          world->type[i] =
            terrain_type_grass << TERRAIN_SHADOW_BITS;
      }
    }
  }
}

#define PATH_WIDTH 3
static void create_path_to_from(terrain_tilemap* world, mersenne_twister* twister,
                                coord xto, coord zto,
                                coord xfrom, coord zfrom) {
  const vc3 to = { xto, 0, zto };
  vc3 here = { xfrom, 0, zfrom }, curr = {0,0,0}, best;
  vo3 distv;
  coord current_distance, this_distance;
  coord_offset ox, oz;
  unsigned i;
  /* These are absolute values, but -1 is used for road intersection so that
   * joining another road always beats using new terrain.
   */
  signed minimum_delta_y, this_delta_y;

  while (here[0] != to[0] || here[2] != to[2]) {
    vc3dist(distv, to, here, world->xmax, world->zmax);
    current_distance = omagnitude(distv);
    minimum_delta_y = 0x7fffffff;

    for (oz = -PATH_WIDTH; oz < PATH_WIDTH; ++oz) {
      for (ox = -PATH_WIDTH; ox < PATH_WIDTH; ++ox) {
        if (!ox && !oz) continue;

        curr[0] = (here[0] + ox) & (world->xmax-1);
        curr[2] = (here[2] + oz) & (world->zmax-1);

        if (curr[0] == to[0] && curr[2] == to[2]) {
          best[0] = to[0];
          best[1] = to[1];
          best[2] = to[2];
          goto fill_point;
        }

        vc3dist(distv, to, curr, world->xmax, world->zmax);
        this_distance = omagnitude(distv);

        if (this_distance < current_distance) {
          /* If the tile is already has road, treat it as if it has a delta-y
           * of zero. This means roads prefer to join together, thus preventing
           * certain weirdness like two nearly parallel roads only a very short
           * distance apart, or even constantly intersecting each other.
           */
          if (terrain_type_road ==
              world->type[terrain_tilemap_offset(world, curr[0], curr[2])]
                  >> TERRAIN_SHADOW_BITS) {
            this_delta_y = -1;
          } else {
            this_delta_y = abs(
              world->alt[terrain_tilemap_offset(world, curr[0], curr[2])] -
              world->alt[terrain_tilemap_offset(world, here[0], here[2])]);
            this_delta_y /= fisqrt(ox*ox + oz*oz);
          }

          if (this_delta_y < minimum_delta_y) {
            minimum_delta_y = this_delta_y;
            memcpy(best, curr, sizeof(curr));
          }
        }
      }
    }

    fill_point:
    memcpy(here, best, sizeof(here));
    for (oz = -PATH_WIDTH; oz < PATH_WIDTH; ++oz) {
      for (ox = -PATH_WIDTH; ox < PATH_WIDTH; ++ox) {
        curr[0] = (here[0] + ox) & (world->xmax-1);
        curr[2] = (here[2] + oz) & (world->zmax-1);
        switch (world->type[terrain_tilemap_offset(world, curr[0], curr[2])]
                    >> TERRAIN_SHADOW_BITS) {
        case terrain_type_water:
        case terrain_type_gravel:
        case terrain_type_stone:
        case terrain_type_road:
          /* Can't put road here; if a road was already here, leave it alone so
           * that path intersection detection works.
           */
          break;

        default:
          /* Don't write a road immediately so that we won't detect the path we
           * are currently creating as if it were one to join with.
           */
          world->type[terrain_tilemap_offset(world, curr[0], curr[2])] =
            TERRAIN_TYPE_PLACEHOLDER << TERRAIN_SHADOW_BITS;
        }
      }
    }
  }

  /* Convert placeholders to actual roads */
  for (i = 0; i < world->xmax * world->zmax; ++i)
    if (TERRAIN_TYPE_PLACEHOLDER ==
        world->type[i] >> TERRAIN_SHADOW_BITS)
      world->type[i] = terrain_type_road << TERRAIN_SHADOW_BITS;
}

#define SHADOW_RADIUS 3
#define SUBREGION_SIZE 128

static void world_add_shadow_subregion(
  terrain_tilemap* world, const env_vmap* vmap,
  coord x0, coord z0, coord xs, coord zs,
  coord xmask, coord zmask
) {
  unsigned char weight[zs+2*SHADOW_RADIUS][xs+2*SHADOW_RADIUS];
  coord_offset xo, zo, xso, zso;
  coord x, y, z;
  unsigned shade;

  memset(weight, 0, sizeof(weight));

  for (zo = -SHADOW_RADIUS; zo < (signed)zs + SHADOW_RADIUS; ++zo) {
    z = (z0 + zo) & zmask;
    for (xo = -SHADOW_RADIUS; xo < (signed)xs + SHADOW_RADIUS; ++xo) {
      x = (x0 + xo) & xmask;
      for (y = 0; y < ENV_VMAP_H; ++y)
        if (vmap->voxels[env_vmap_offset(vmap, x, y, z)])
          ++weight[zo+SHADOW_RADIUS][xo+SHADOW_RADIUS];
    }
  }

  for (zo = 0; zo < (signed)zs; ++zo) {
    for (xo = 0; xo < (signed)xs; ++xo) {
      shade = 0;

      for (zso = -SHADOW_RADIUS; zso < SHADOW_RADIUS; ++zso)
        for (xso = -SHADOW_RADIUS; xso < SHADOW_RADIUS; ++xso)
          shade += 65536 * weight[zo+zso+SHADOW_RADIUS][xo+xso+SHADOW_RADIUS] /
            (1 + abs(zso) + abs(xso));

      shade /= 65536;
      if (shade > 3) shade = 3;
      world->type[terrain_tilemap_offset(
          world, (x0 + xo) & xmask, (z0 + zo) & zmask)] |= shade;
    }
  }
}

static void world_add_shadow_impl(unsigned row, unsigned ignored);

static terrain_tilemap* world_add_shadow_world;
static const env_vmap* world_add_shadow_vmap;
static ump_task world_add_shadow_task = {
  world_add_shadow_impl,
  0, /* set dynamically */
  0 /* sync */
};

static void world_add_shadow_impl(unsigned row, unsigned ignored) {
  terrain_tilemap* world = world_add_shadow_world;
  const env_vmap* vmap = world_add_shadow_vmap;

  unsigned col;

  for (col = 0; col < world->xmax / SUBREGION_SIZE; ++col)
    world_add_shadow_subregion(world, vmap,
                               col * SUBREGION_SIZE, row * SUBREGION_SIZE,
                               SUBREGION_SIZE, SUBREGION_SIZE,
                               world->xmax - 1, world->zmax - 1);
}

void world_add_shadow(terrain_tilemap* world, const env_vmap* vmap) {
  world_add_shadow_world = world;
  world_add_shadow_vmap = vmap;
  world_add_shadow_task.num_divisions = world->zmax / SUBREGION_SIZE;
  ump_run_sync(&world_add_shadow_task);

  terrain_tilemap_calc_next(world);
}
