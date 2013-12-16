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
#include "../graphics/canvas.h"
#include "../graphics/sybmap.h"
#include "../graphics/tscan.h"
#include "../graphics/perspective.h"
#include "../world/world.h"
#include "../world/terrain.h"
#include "terrain.h"

/* This shader is just for testing right now --- colour according to world
 * coordinates.
 */
typedef struct {
  coord_offset screen_z;
  vc3 world_coords;
} terrain_interp_data;

static void shade_terrain_pixel(canvas* dst,
                                coord_offset x, coord_offset y,
                                const coord_offset* vinterps) {
  const terrain_interp_data* interp = (const terrain_interp_data*)vinterps;
  canvas_write(dst, x, y,
               argb(255, interp->world_coords[0] / TILE_SZ,
                    interp->world_coords[1] / TILE_SZ,
                    interp->world_coords[2] / TILE_SZ),
               interp->screen_z);
}

SHADE_TRIANGLE(shade_terrain, shade_terrain_pixel, 4)

void render_terrain_tile(canvas* dst, sybmap* syb,
                         const basic_world* world,
                         const perspective* proj,
                         coord tx, coord tz) {
  vc3 world_coords[4];
  vo3 screen_coords[4];
  terrain_interp_data interp[4];
  int has_012 = 1, has_123 = 1;
  coord tx1 = (tx+1) & (world->xmax-1);
  coord tz1 = (tz+1) & (world->zmax-1);
  unsigned i;

  world_coords[0][0] = tx * TILE_SZ;
  world_coords[0][1] = world->tiles[basic_world_offset(world, tx, tz)]
    .elts[0].altitude * TILE_YMUL;
  world_coords[0][2] = tz * TILE_SZ;
  world_coords[1][0] = tx1 * TILE_SZ;
  world_coords[1][1] = world->tiles[basic_world_offset(world, tx1, tz)]
    .elts[0].altitude * TILE_YMUL;
  world_coords[1][2] = tz * TILE_SZ;
  world_coords[2][0] = tx * TILE_SZ;
  world_coords[2][1] = world->tiles[basic_world_offset(world, tx, tz1)]
    .elts[0].altitude * TILE_YMUL;
  world_coords[2][2] = tz1 * TILE_SZ;
  world_coords[3][0] = tx1 * TILE_SZ;
  world_coords[3][1] = world->tiles[basic_world_offset(world, tx1, tz1)]
    .elts[0].altitude * TILE_YMUL;
  world_coords[3][2] = tz1 * TILE_SZ;

  has_012 &= has_123 &= perspective_proj(screen_coords[1], world_coords[1],
                                         proj);
  has_012 &= has_123 &= perspective_proj(screen_coords[2], world_coords[2],
                                         proj);
  has_012 &= perspective_proj(screen_coords[0], world_coords[0], proj);
  has_123 &= perspective_proj(screen_coords[3], world_coords[3], proj);

  for (i = 0; i < 4; ++i) {
    interp[i].screen_z = screen_coords[i][2];
    memcpy(interp[i].world_coords, world_coords[i], sizeof(vc3));
  }

  if (has_012) {
    shade_terrain(dst,
                  screen_coords[0], (coord_offset*)&interp[0],
                  screen_coords[1], (coord_offset*)&interp[1],
                  screen_coords[2], (coord_offset*)&interp[2],
                  NULL);
    sybmap_put(syb, screen_coords[0], screen_coords[1], screen_coords[2]);
  }

  if (has_123) {
    shade_terrain(dst,
                  screen_coords[1], (coord_offset*)&interp[1],
                  screen_coords[2], (coord_offset*)&interp[2],
                  screen_coords[3], (coord_offset*)&interp[3],
                  NULL);
    sybmap_put(syb, screen_coords[1], screen_coords[2], screen_coords[3]);
  }
}
