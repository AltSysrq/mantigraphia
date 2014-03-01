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
#include "basic-world.h"
#include "terrain.h"
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

  twister_seed(&twister, seed);

  generate_level(world, 0, &twister);
  select_terrain(world, &twister);
  create_path_to_from(world, &twister,
                      0, 0, world->xmax/2, world->zmax/2);

  basic_world_calc_next(world);
}

static void generate_level(basic_world* world, signed level,
                           mersenne_twister* twister) {
  basic_world* next;

  printf("Generating level %d\n", level);

  next = SLIST_NEXT(world, next);
  if (next) {
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
  unsigned i, max;

  max = world->xmax * world->zmax;
  for (i = 0; i < max; ++i) {
    world->tiles[i].elts[0].type = terrain_type_grass << TERRAIN_SHADOW_BITS;
    world->tiles[i].elts[0].thickness = 0;
    world->tiles[i].elts[0].altitude =
      twist(twister) & (4*(1 << level) - 1);
  }
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

static void create_path_to_from(basic_world* world, mersenne_twister* twister,
                                coord xto, coord zto,
                                coord xfrom, coord zfrom) {
  /* TODO */
}

void grass_generate(world_prop* props, unsigned count,
                    const basic_world* world, unsigned seed) {
  mersenne_twister twister;
  unsigned i;
  coord x, z, wx, wz;

  twister_seed(&twister, seed);

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
    props[i].type = 1;
    props[i].variant = twist(&twister);
    props[i].yrot = twist(&twister);
  }
}

static int may_place_tree_at(const basic_world* world,
                             unsigned offset,
                             mersenne_twister* twister) {
  switch (world->tiles[offset].elts[0].type >> TERRAIN_SHADOW_BITS) {
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
    props[i].type = 1;
    props[i].variant = twist(&twister);
    props[i].yrot = twist(&twister);
  }

  free(density);

  update_shadows(world, props, count, 10);
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
