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
 *   boundaries
 */
static int render_one_tile(
  canvas* dst, sybmap* coverage,
  const basic_world*restrict world,
  const perspective* proj,
  coord tx0, coord tz0,
  coord_offset logical_tx, coord_offset logical_tz,
  unsigned char level)
{
  coord tx1 = ((tx0+1) & (world->xmax-1));
  coord tz1 = ((tz0+1) & (world->zmax-1));
  coord x0, x1, y0, y1, z0, z1;
  coord_offset scx0, scx1, scy0, scy1;
  vc3 point;
  vo3 ppoint;
  int any_projected = 0, has_screen_x = 0;

  /* Calculate world bounding box */
  x0 = (logical_tx+0) * TILE_SZ << level;
  x1 = (logical_tx+1) * TILE_SZ << level;
  z0 = (logical_tz+0) * TILE_SZ << level;
  z1 = (logical_tz+1) * TILE_SZ << level;
  y0 = y1 = altitude(world, tx0, tz0);
  extremau(&y0, &y1, altitude(world, tx1, tz0));
  extremau(&y0, &y1, altitude(world, tx0, tz1));
  extremau(&y0, &y1, altitude(world, tx1, tz1));

  /* Test coordinates and calculate screen bounding box */
  scx0 = dst->w;
  scx1 = 0;
  scy0 = dst->h;
  scy1 = 0;
#define TEST(x,y,z)                                     \
  point[0] = x; point[1] = y; point[2] = z;             \
  if (perspective_proj(ppoint, point, proj)) {          \
    any_projected = 1;                                  \
    if (ppoint[0] >= 0 && ppoint[0] < (signed)dst->w)   \
      has_screen_x = 1;                                 \
    extremas(&scx0, &scx1, point[0]);                   \
    extremas(&scy0, &scy1, point[1]);                   \
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

  if (!any_projected || !has_screen_x)
    return 0;

  if (sybmap_test(coverage, scx0, scx1, scy0, scy1))
    render_terrain_tile(dst, coverage, world, proj, tx0, tz0,
                        logical_tx, logical_tz, level);
  return level <= 0;
}

/**
 * World rendering is performed by splitting the world into four quadrants,
 * centred on the camera's location. Each quadrant is rendered independently,
 * moving in a cardinal direction (the major axis). Each point along this
 * cardinal direction is a segment, drawn as a series of tiles along the minor
 * axis. Each time a segment is drawn, the minor axis is capped to the bounds
 * that were actually on the screen, but expands one unit in each direction as
 * the major axis moves out.
 */

#define RENDER_SEGMENT(expr_render, min, max, i)        \
  /* Find the first renderable tile */                  \
  for (i = min; i <= max; ++i) {                        \
    if (expr_render) {                                  \
      min = i;                                          \
      break;                                            \
    }                                                   \
  }                                                     \
  if (i > max) min = i;                                 \
  /* Render tiles until boundary is reached */          \
  while (i <= max) {                                    \
    if (!expr_render) {                                 \
      max = i-1;                                        \
      break;                                            \
    }                                                   \
    ++i;                                                \
  }

#define REDUCE_LEVEL                                      \
  while (world && ((major >> level) > 128))               \
    world = SLIST_NEXT(world, next), ++level, major /= 2, \
      minor_min /= 2, minor_max /= 2, cx /= 2, cz /= 2

#define SEGMENT_RENDERER(name, coord_calc_x, coord_calc_z)             \
  static void name(canvas* dst,                                        \
                   sybmap* coverage,                                   \
                   const basic_world*restrict world,                   \
                   const perspective* proj) {                          \
    coord major = 1;                                                   \
    coord_offset minor, minor_min = 0, minor_max = 1;                  \
    coord cx = proj->camera[0] / TILE_SZ;                              \
    coord cz = proj->camera[2] / TILE_SZ;                              \
    unsigned char level = 0;                                           \
    REDUCE_LEVEL;                                                      \
    while (world) {                                                    \
      RENDER_SEGMENT(render_one_tile(                                  \
                       dst, coverage, world, proj,                     \
                       coord_calc_x(major,minor,cx) &                  \
                       (world->xmax-1),                                \
                       coord_calc_z(major,minor,cz) &                  \
                       (world->zmax-1),                                \
                       coord_calc_x(major,minor,cx),                   \
                       coord_calc_z(major,minor,cz),                   \
                       level), minor_min, minor_max,                   \
                     minor);                                           \
      if (minor_min > minor_max) break;                                \
      --minor_min, ++minor_max, ++major;                               \
      REDUCE_LEVEL;                                                    \
    }                                                                  \
  }

static inline coord_offset sr_pmin(coord major, coord_offset minor,
                                   coord base) {
  return base + minor;
}

static inline coord_offset sr_nmin(coord major, coord_offset minor,
                                   coord base) {
  return base - minor;
}

static inline coord_offset sr_pmaj(coord major, coord_offset minor,
                                   coord base) {
  return base + major;
}

static inline coord_offset sr_nmaj(coord major, coord_offset minor,
                                   coord base) {
  return base - minor;
}

SEGMENT_RENDERER(render_segment_north, sr_pmin, sr_nmaj)
SEGMENT_RENDERER(render_segment_east , sr_pmaj, sr_pmin)
SEGMENT_RENDERER(render_segment_south, sr_nmin, sr_pmaj)
SEGMENT_RENDERER(render_segment_west , sr_nmaj, sr_nmin)

void basic_world_render_faster(
  canvas* dst,
  sybmap* coverage[4],
  const basic_world*restrict world,
  const perspective* proj)
{
  unsigned i;

  /* Render tile at camera position (not included in any segment) */
  render_one_tile(dst, coverage[0], world, proj,
                  proj->camera[0] / TILE_SZ, proj->camera[2] / TILE_SZ,
                  proj->camera[0] / TILE_SZ, proj->camera[2] / TILE_SZ,
                  0);

  for (i = 0; i < 3; ++i)
    sybmap_copy(coverage[i+1], coverage[0]);

  render_segment_north(dst, coverage[0], world, proj);
  render_segment_east (dst, coverage[1], world, proj);
  render_segment_south(dst, coverage[2], world, proj);
  render_segment_west (dst, coverage[3], world, proj);
}

void basic_world_render(
  canvas* dst,
  sybmap* coverage[4],
  const basic_world*restrict world,
  const perspective* proj)
{
  coord x, z;

  for (z = 0; z < world->zmax; ++z) {
    for (x = 0; x < world->xmax/2; ++x) {
      render_terrain_tile(dst, coverage[0], world, proj,
                          x, z, x, z, 0);
    }
  }
}
