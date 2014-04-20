/*-
 * Copyright (c) 2013, 2014 Jason Lingle
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
#include "../coords.h"
#include "../rand.h"
#include "../defs.h"
#include "basic-world.h"
#include "terrain.h"
#include "grass-props.h"
#include "generate.h"

static void initialise(basic_world*);
static void randomise(basic_world*, signed, mersenne_twister*);
static void rmp_up(basic_world* large, const basic_world* small,
                   signed level, mersenne_twister*);
static void generate_level(basic_world*, signed level, mersenne_twister*);
static void select_terrain(basic_world*, mersenne_twister*);
static void create_path_to_from(basic_world*, mersenne_twister*,
                                coord xto, coord zto,
                                coord xfrom, coord zfrom);

void world_generate(basic_world* world, unsigned seed) {
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

  basic_world_calc_next(world);
}

static int above_perlin_threshold(const basic_world* world) {
  return world->xmax > 512 && world->zmax > 512;
}

static void generate_level(basic_world* world, signed level,
                           mersenne_twister* twister) {
  basic_world* next;

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

static void initialise(basic_world* world) {
  unsigned i, max;

  max = world->xmax * world->zmax;
  for (i = 0; i < max; ++i) {
    world->tiles[i].elts[0].type = terrain_type_grass << TERRAIN_SHADOW_BITS;
    world->tiles[i].elts[0].thickness = 0;
  }
}

static void randomise(basic_world* world,
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
      world->tiles[i].elts[0].altitude = 0;
    else
      world->tiles[i].elts[0].altitude = hmap[i] - altitude_reduction;

  free(hmap);
}

static inline signed altitude(const basic_world* world,
                              coord x, coord z) {
  signed s = world->tiles[basic_world_offset(world, x, z)]
    .elts[0].altitude;

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

static void rmp_up(basic_world* large,
                   const basic_world* small,
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
      large->tiles[basic_world_offset(large, lx0, lz0)]
        .elts[0].altitude = perturb(sa00, level, twister);

      /* Other points get perturbed averages of what they are in-between. */
      large->tiles[basic_world_offset(large, lx0, lz1)]
        .elts[0].altitude = perturb((sa00+sa01)/2, level, twister);
      large->tiles[basic_world_offset(large, lx1, lz0)]
        .elts[0].altitude = perturb((sa00+sa10)/2, level, twister);
      large->tiles[basic_world_offset(large, lx1, lz1)]
        .elts[0].altitude = perturb((sa00+sa01+sa10+sa11)/4, level, twister);
    }
  }
}

static void select_terrain(basic_world* world, mersenne_twister* twister) {
  unsigned x, z, i, dx, dz;
  coord_offset miny, maxy, y;

  for (z = 0; z < world->zmax; ++z) {
    for (x = 0; x < world->xmax; ++x) {
      i = basic_world_offset(world, x, z);

      if (world->tiles[i].elts[0].altitude <= 2*METRE / TILE_YMUL) {
        world->tiles[i].elts[0].type =
          terrain_type_water << TERRAIN_SHADOW_BITS;
      } else if (world->tiles[i].elts[0].altitude <= 4*METRE / TILE_YMUL) {
        world->tiles[i].elts[0].type =
          terrain_type_gravel << TERRAIN_SHADOW_BITS;
      /* Sometimes patches of snow, depending on altitude */
      } else if (((signed)(twist(twister)/2)) <
          world->tiles[i].elts[0].altitude * TILE_YMUL) {
        world->tiles[i].elts[0].type = terrain_type_snow << TERRAIN_SHADOW_BITS;
      } else {
        /* Stone if max dy*2 > dx, grass otherwise */
        miny = 32767 * TILE_YMUL;
        maxy = 0;
        for (dz = 0; dz < 2; ++dz) {
          for (dx = 0; dx < 2; ++dx) {
            y = world->tiles[basic_world_offset(world,
                                                (x+dx) & (world->xmax-1),
                                                (z+dz) & (world->zmax-1))]
              .elts[0].altitude * TILE_YMUL;
            if (y > maxy)
              maxy = y;
            if (y < miny)
              miny = y;
          }
        }

        if (maxy - miny > TILE_SZ/2)
          world->tiles[i].elts[0].type =
            terrain_type_stone << TERRAIN_SHADOW_BITS;
        else if (twist(twister) & 7)
          world->tiles[i].elts[0].type =
            terrain_type_bare_grass << TERRAIN_SHADOW_BITS;
        else
          world->tiles[i].elts[0].type =
            terrain_type_grass << TERRAIN_SHADOW_BITS;
      }
    }
  }
}

#define PATH_WIDTH 3
static void create_path_to_from(basic_world* world, mersenne_twister* twister,
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
              world->tiles[basic_world_offset(world, curr[0], curr[2])]
              .elts[0].type >> TERRAIN_SHADOW_BITS) {
            this_delta_y = -1;
          } else {
            this_delta_y = abs(
              world->tiles[basic_world_offset(world, curr[0], curr[2])]
              .elts[0].altitude -
              world->tiles[basic_world_offset(world, here[0], here[2])]
              .elts[0].altitude);
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
        switch (world->tiles[basic_world_offset(world, curr[0], curr[2])]
                .elts[0].type >> TERRAIN_SHADOW_BITS) {
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
          world->tiles[basic_world_offset(world, curr[0], curr[2])]
            .elts[0].type = TERRAIN_TYPE_PLACEHOLDER << TERRAIN_SHADOW_BITS;
        }
      }
    }
  }

  /* Convert placeholders to actual roads */
  for (i = 0; i < world->xmax * world->zmax; ++i)
    if (TERRAIN_TYPE_PLACEHOLDER ==
        world->tiles[i].elts[0].type >> TERRAIN_SHADOW_BITS)
      world->tiles[i].elts[0].type = terrain_type_road << TERRAIN_SHADOW_BITS;
}

void grass_generate(world_prop* props, unsigned count,
                    const basic_world* world, unsigned seed) {
  mersenne_twister twister;
  unsigned i, j, max;
  coord x, z, wx, wz;
  unsigned* seasonal_flower_distributions[NUM_GRASS_SEASONAL_FLOWER_TYPES];

  twister_seed(&twister, seed);

  seasonal_flower_distributions[0] = zxmalloc(
    NUM_GRASS_SEASONAL_FLOWER_TYPES * sizeof(unsigned) *
    world->xmax * world->zmax);
  for (i = 0; i < NUM_GRASS_SEASONAL_FLOWER_TYPES; ++i) {
    seasonal_flower_distributions[i] =
      seasonal_flower_distributions[0] +
      i * world->xmax * world->zmax;

    perlin_noise(seasonal_flower_distributions[i], world->xmax, world->zmax,
                 world->xmax / 64, 256, twist(&twister));
    perlin_noise(seasonal_flower_distributions[i], world->xmax, world->zmax,
                 world->xmax / 32, 96, twist(&twister));
    perlin_noise(seasonal_flower_distributions[i], world->xmax, world->zmax,
                 world->xmax / 24, 16, twist(&twister));
  }

  for (i = 0; i < count; ++i) {
    do {
      x = twist(&twister) & (world->xmax*TILE_SZ - 1);
      z = twist(&twister) & (world->zmax*TILE_SZ - 1);
      wx = x / TILE_SZ;
      wz = z / TILE_SZ;

      /* Can only place grass in non-bare grass tiles */
    } while (terrain_type_grass !=
             world->tiles[basic_world_offset(world, wx, wz)].elts[0].type
             >> TERRAIN_SHADOW_BITS);

    props[i].x = x;
    props[i].z = z;
    /* 20% at random (uniform distribution) are wildflowers */
    props[i].type = 1 + twist(&twister) % NUM_GRASS_WILDFLOWER_TYPES;
    if (twist(&twister) % 5) {
      /* Otherwise, the type is the one with the greatest distribution value in
       * this tile
       */
      max = 0;
      for (j = 0; j < NUM_GRASS_SEASONAL_FLOWER_TYPES; ++j) {
        if (seasonal_flower_distributions[j][
              wx + wz*world->xmax] > max) {
          props[i].type = 1 + NUM_GRASS_WILDFLOWER_TYPES + j;
          max = seasonal_flower_distributions[j][wx + wz*world->xmax];
        }
      }
    }

    props[i].variant = twist(&twister);
    props[i].yrot = twist(&twister);
  }

  free(seasonal_flower_distributions[0]);
}

static int may_place_tree_at(const basic_world* world,
                             unsigned offset,
                             mersenne_twister* twister) {
  switch (world->tiles[offset].elts[0].type >> TERRAIN_SHADOW_BITS) {
  case terrain_type_road:
  case terrain_type_water:
  case terrain_type_gravel:
  case terrain_type_stone: return 0;
  case terrain_type_snow: return twist(twister) & 1;
  default: return 1;
  }
}

static void update_shadows(basic_world* world,
                           world_prop* props,
                           unsigned count,
                           signed radius);

void trees_generate(world_prop* props, unsigned count,
                    basic_world* world,
                    unsigned distrib_seed, unsigned pos_seed) {
  mersenne_twister twister;
  unsigned i;
  coord x, z, wx, wz;
  unsigned off;
  unsigned* density, density_sz;

  density_sz = sizeof(unsigned)*world->xmax*world->zmax;
  density = xmalloc(density_sz);
  memset(density, 0, density_sz);
  perlin_noise(density, world->xmax, world->zmax,
               world->xmax / 256, 0xC000, distrib_seed+1);
  perlin_noise(density, world->xmax, world->zmax,
               world->xmax / 32, 0x4000, distrib_seed+2);

  twister_seed(&twister, pos_seed);

  for (i = 0; i < count; ++i) {
    do {
      x = twist(&twister) & (world->xmax*TILE_SZ - 1);
      z = twist(&twister) & (world->zmax*TILE_SZ - 1);
      wx = x / TILE_SZ;
      wz = z / TILE_SZ;
      off = basic_world_offset(world, wx, wz);
    } while (!may_place_tree_at(world, off, &twister) ||
             twist(&twister) > density[off] * density[off]);

    props[i].x = x;
    props[i].z = z;
    /* TODO: Distribute different trees differently */
    props[i].type = 1 + (twist(&twister) & 1);
    props[i].variant = twist(&twister);
    props[i].yrot = twist(&twister);
  }

  free(density);

  update_shadows(world, props, count, 10);
  basic_world_calc_next(world);
}

static void update_shadows(basic_world* world,
                           world_prop* props,
                           unsigned count,
                           signed radius) {
  coord x, z;
  coord_offset xo, zo;
  unsigned i, off;

  for (i = 0; i < count; ++i) {
    for (zo = -radius; zo <= +radius; ++zo) {
      for (xo = -radius; xo <= +radius; ++xo) {
        if (fisqrt(xo*xo + zo*zo) <= (unsigned)radius) {
          x = props[i].x / TILE_SZ + xo;
          z = props[i].z / TILE_SZ + zo;
          x &= (world->xmax - 1);
          z &= (world->zmax - 1);
          off = basic_world_offset(world, x, z);
          if (3 != (world->tiles[off].elts[0].type & 3))
            ++world->tiles[off].elts[0].type;
        }
      }
    }
  }
}
