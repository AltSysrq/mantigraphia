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

#include <stdlib.h>

#include "../coords.h"
#include "../graphics/canvas.h"
#include "../graphics/perspective.h"
#include "../graphics/sybmap.h"
#include "../world/world.h"
#include "../world/terrain.h"
#include "terrain.h"
#include "world.h"

static inline coord altitude(const basic_world* world,
                             coord x, coord z) {
  return world->tiles[basic_world_offset(world, x, z)]
    .elts[0].altitude * TILE_YMUL;
}

static inline void extremau(coord*restrict min, coord*restrict max,
                            coord value) {
  if (value < *min) *min = value;
  if (value > *max) *max = value;
}
static inline void extremas(coord_offset*restrict min,
                            coord_offset*restrict max,
                            coord_offset value) {
  if (value < *min) *min = value;
  if (value > *max) *max = value;
}

/**
 * Renders one tile of the world at the given detail level. Returns whether the
 * tile could possibly be projected onto the screen.
 *
 * "Possibly projected" means that:
 *
 * - At least one vertex of the tile's bounding box could be projected via
 *   perspective_proj().
 * - At least one projected vertex had an X coordinate within the canvas
 *   boundaries.
 * - At least one projected vertex had a Y coordinate within the canvas
 *   boundaries.
 */
static int render_one_tile(
  canvas* dst, sybmap* test_coverage, sybmap* write_coverage,
  const basic_world*restrict world,
  const perspective* proj,
  coord tx0, coord tz0,
  coord cx, coord cz,
  unsigned char level)
{
  coord tx1 = ((tx0+1) & (world->xmax-1));
  coord tz1 = ((tz0+1) & (world->zmax-1));
  coord x0, x1, y0, y1, z0, z1;
  coord_offset scx0, scx1, scy0, scy1;
  vc3 point;
  vo3 ppoint;
  int any_projected = 0, has_screen_x = 0, has_screen_y;

  /* Don't draw if tile wraps around the torus */
  if (abs(torus_dist(cx-tx0, world->xmax)) >= (signed)(world->xmax/2-1) ||
      abs(torus_dist(cz-tz0, world->zmax)) >= (signed)(world->zmax/2-1))
    return 0;

  /* Calculate world bounding box */
  x0 = tx0 * TILE_SZ << level;
  x1 = tx1 * TILE_SZ << level;
  z0 = tz0 * TILE_SZ << level;
  z1 = tz1 * TILE_SZ << level;
  y0 = y1 = altitude(world, tx0, tz0);
  extremau(&y0, &y1, altitude(world, tx1, tz0));
  extremau(&y0, &y1, altitude(world, tx0, tz1));
  extremau(&y0, &y1, altitude(world, tx1, tz1));

  /* Test coordinates and calculate screen bounding box */
  scx0 = dst->w;
  scx1 = -1;
  scy0 = dst->h;
  scy1 = -1;
#define TEST(x,y,z)                                     \
  point[0] = x; point[1] = y; point[2] = z;             \
  if (perspective_proj(ppoint, point, proj)) {          \
    any_projected = 1;                                  \
    extremas(&scx0, &scx1, ppoint[0]);                  \
    extremas(&scy0, &scy1, ppoint[1]);                  \
  }
  TEST(x0, y0, z0);
  TEST(x0, y0, z1);
  TEST(x0, y1, z0);
  TEST(x0, y1, z1);
  TEST(x1, y0, z0);
  TEST(x1, y0, z1);
  TEST(x1, y1, z0);
  TEST(x1, y1, z1);
#undef TEST

  has_screen_x = (scx0 < (signed)dst->w && scx1 >= 0);
  has_screen_y = (scy0 < (signed)dst->h && scy1 >= 0);

  if (!any_projected || !has_screen_x || !has_screen_y)
    return 0;

  if (sybmap_test(test_coverage, scx0, scx1, scy0, scy1))
    render_terrain_tile(dst, write_coverage, world, proj, tx0, tz0,
                        tx0, tz0, level);
  return 1;
}

/**
 * World rendering is performed by splitting the world into four quadrants,
 * centred on the camera's location. Each quadrant is rendered independently,
 * moving in a cardinal direction (the major axis). Each point along this
 * cardinal direction is a segment, drawn as a series of tiles along the minor
 * axis. The minor axis is *not* capped to what is drawable, as sometimes
 * terrain will move out of the visible area only to rise back into it later.
 *
 * Segments are drawn in parallel at the same major axis value. Because of
 * this, we use two sybmaps: The one representing the state after drawing the
 * previous major axis value, and the one new data is written to.
 *
 * This attempts to render every tile in the world, subject to lod. This is
 * logarithmic with respect to each dimension being rendered.
 */
void basic_world_render(
  canvas* dst,
  sybmap* coverage[2],
  const basic_world*restrict world,
  const perspective* proj)
{
  coord cx = proj->camera[0] / TILE_SZ, cz = proj->camera[2] / TILE_SZ;
  coord_offset dist, minor;
  unsigned char level = 0;
  /* Render tile at camera position (not included in any segment) */
  render_one_tile(dst, coverage[0], coverage[1], world, proj,
                  cx, cz, cx, cz, 0);

  for (dist = 1; world && dist < (signed)(world->xmax / 2); ++dist) {
    sybmap_copy(coverage[0], coverage[1]);
    for (minor = -dist+1; minor <= dist; ++minor) {
#define C(x,lim) ((x) & (world->lim-1))
      render_one_tile(dst, coverage[0], coverage[1], world, proj,
                      C(cx + minor,xmax), C(cz - dist,zmax), cx, cz, level);
      render_one_tile(dst, coverage[0], coverage[1], world, proj,
                      C(cx + dist,xmax), C(cz + minor,zmax), cx, cz, level);
      render_one_tile(dst, coverage[0], coverage[1], world, proj,
                      C(cx - minor,xmax), C(cz + dist,zmax), cx, cz, level);
      render_one_tile(dst, coverage[0], coverage[1], world, proj,
                      C(cx - dist,xmax), C(cz - minor,zmax), cx, cz, level);
#undef C
    }

    if ((dist >> level) > 16) {
      world = SLIST_NEXT(world, next);
      dist /= 2;
      cx /= 2;
      cz /= 2;
      ++level;
    }
  }
}
