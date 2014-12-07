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

#include "../coords.h"
#include "../simd.h"
#include "terrain-tilemap.h"
#include "terrain.h"

static inline coord_offset altitude(const terrain_tilemap* world,
                                    coord tx, coord tz) {
  return world->tiles[terrain_tilemap_offset(world, tx, tz)].altitude *
    TILE_YMUL;
}

coord terrain_base_y(const terrain_tilemap* world, coord wx, coord wz) {
  unsigned long long ox = wx % TILE_SZ, oz = wz % TILE_SZ;
  coord x = wx / TILE_SZ;
  coord z = wz / TILE_SZ;
  coord x2 = (x+1) & (world->xmax-1), z2 = (z+1) & (world->zmax-1);
  coord y00 = altitude(world, x, z),
        y01 = altitude(world, x, z2),
        y10 = altitude(world, x2, z),
        y11 = altitude(world, x2, z2);

  coord y0 = ((TILE_SZ-ox)*y00 + ox*y10) / TILE_SZ;
  coord y1 = ((TILE_SZ-ox)*y01 + ox*y11) / TILE_SZ;

  return ((TILE_SZ-oz)*y0 + oz*y1) / TILE_SZ;
}

coord terrain_graphical_y(const terrain_tilemap* world, coord wx, coord wz,
                          chronon t) {
  coord base = terrain_base_y(world, wx, wz);
  coord x = wx / TILE_SZ;
  coord z = wz / TILE_SZ;

  if (terrain_type_water ==
      world->tiles[terrain_tilemap_offset(world, x, z)].type >>
      TERRAIN_SHADOW_BITS) {
    return 3 * METRE / 2 + zo_cosms((wx+wz+t*65536/8)/16, METRE/2);
  } else {
    if (base < 2 * METRE)
      return 2 * METRE;
    else
      return base;
  }
}

static simd4 colour_of(const terrain_tilemap* world,
                       coord x, coord z,
                       const simd4* terrain_colours) {
  return terrain_colours[
    world->tiles[
      terrain_tilemap_offset(world, x, z)
    ].type];
}

simd4 terrain_colour(const terrain_tilemap* world,
                     coord wx, coord wz,
                     const simd4* terrain_colours) {
  signed ox = wx % TILE_SZ, oz = wz % TILE_SZ;
  coord x = wx / TILE_SZ;
  coord z = wz / TILE_SZ;
  coord x2 = (x+1) & (world->xmax-1), z2 = (z+1) & (world->zmax-1);
  simd4 c00 = colour_of(world, x, z, terrain_colours),
        c01 = colour_of(world, x, z2, terrain_colours),
        c10 = colour_of(world, x2, z, terrain_colours),
        c11 = colour_of(world, x2, z2, terrain_colours);
  simd4 c0, c1;

  c0 = simd_divvs(simd_addvv(simd_mulvs(c00, TILE_SZ-ox),
                             simd_mulvs(c10, ox)),
                  TILE_SZ);
  c1 = simd_divvs(simd_addvv(simd_mulvs(c01, TILE_SZ-ox),
                             simd_mulvs(c11, ox)),
                  TILE_SZ);
  return simd_divvs(simd_addvv(simd_mulvs(c0, TILE_SZ-oz),
                               simd_mulvs(c1, oz)),
                    TILE_SZ);
}

void terrain_basic_normal(vo3 dst, const terrain_tilemap* world,
                          coord tx, coord tz) {
  coord x2 = (tx+1) & (world->xmax-1), z2 = (tz+1) & (world->zmax-1);
  coord_offset dy0011, dy1001;

  /* We guestimate the "normal" by crossing the ((0,0),(1,1)) and ((1,0),(0,1))
   * vectors. This is rather inexact since bilinear interpolation occurs with
   * attributes determined from it, and since the square might not actually be
   * planar (in which case this normal is undefined anyway).
   *
   * Note that X and Z are constant for both vectors, so we only need to
   * calculate Y for each. This also means that the Y component of the cross
   * product is constant.
   */
  dy0011 = altitude(world, x2, z2) - altitude(world, tx, tz);
  dy1001 = altitude(world, tx, z2) - altitude(world, x2, tz);

  /* In order to avoid integer overflow, divide all components by TILE_SZ. This
   * works out nicely, since every term has a constant TILE_SZ factor
   * nominally.
   */
  dst[0] = dy0011 /* * TILE_SZ */ - /* TILE_SZ * */ dy1001;
  dst[1] = 2 * TILE_SZ /* * TILE_SZ */;
  dst[2] = /* TILE_SZ * */ dy1001 + dy0011 /* * TILE_SZ */;
}
