/*-
 * Copyright (c) 2013 Jason Lingle
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
#include "world.h"
#include "terrain.h"

static inline coord_offset altitude(const basic_world* world,
                                    coord tx, coord tz) {
  return world->tiles[basic_world_offset(world, tx, tz)].elts[0].altitude *
    TILE_YMUL;
}

coord terrain_base_y(const basic_world* world, coord wx, coord wz) {
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

void terrain_basic_normal(vo3 dst, const basic_world* world,
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
