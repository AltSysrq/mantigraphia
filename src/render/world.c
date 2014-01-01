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
#include "../world/world.h"
#include "../world/terrain.h"
#include "context.h"
#include "terrabuff.h"
#include "terrain.h"
#include "world.h"

#define SLICE_CAP 256
#define SCAN_CAP 128

RENDERING_CONTEXT_STRUCT(render_basic_world, terrabuff*)

void render_basic_world_context_ctor(rendering_context*restrict context) {
  *render_basic_world_getm(context) = terrabuff_new(SLICE_CAP, SCAN_CAP);
}

void render_basic_world_context_dtor(rendering_context*restrict context) {
  terrabuff_delete(*render_basic_world_get(context));
}

static inline terrabuff_slice angle_to_slice(angle ang) {
  unsigned zx = ang;
  /* Invert direction, as slices run clockwise and angles counter-clockwise. */
  zx = 65536 - zx;
  /* Rescale */
  return zx * SLICE_CAP / 65536;
}

static inline angle slice_to_angle(terrabuff_slice slice) {
  unsigned zx = slice;
  zx = zx * 65536 / SLICE_CAP;
  return 65536 - zx;
}

static void put_point(terrabuff* dst, const vc3 centre,
                      terrabuff_slice slice, coord_offset distance,
                      const basic_world*restrict world, unsigned char level,
                      const perspective* proj, unsigned xmax) {
  vc3 point;
  vo3 relative, projected;
  coord tx, tz;
  int clamped = 0;

  point[0] = centre[0] - zo_sinms(slice_to_angle(slice), distance);
  point[2] = centre[2] - zo_cosms(slice_to_angle(slice), distance);
  tx = (point[0] >> level) & (world->xmax*TILE_SZ - 1);
  tz = (point[2] >> level) & (world->zmax*TILE_SZ - 1);
  point[1] = terrain_base_y(world, tx, tz);

  /* To ensure that every point can project, clamp relative Z coordinates to
   * the effective near clipping plane. This provides an acceptable
   * approximation of the 2D projection of "infinity in that direction".
   */
  perspective_xlate(relative, point, proj);
  if (relative[2] > proj->effective_near_clipping_plane-1) {
    relative[2] = proj->effective_near_clipping_plane-1;
    clamped = 1;
  }

  if (!perspective_proj_rel(projected, relative, proj))
    abort();

  if (clamped)
    projected[1] = 65536;

  terrabuff_put(dst, projected, xmax);
}

static void render_basic_world_terrain(
  canvas* dst,
  const basic_world*restrict world,
  const rendering_context*restrict context)
{
  const perspective*restrict proj =
    ((const rendering_context_invariant*)context)->proj;
  terrabuff* terra = *render_basic_world_get(context);
  unsigned scan = 0;
  terrabuff_slice smin, scurr, smax;
  unsigned char level = 0;
  coord_offset distance = 1 * METRE, distance_incr = 1 * METRE;

  /* Start by assuming 180 deg effective field. The terrabuff will give us
   * better boundaries after the first scan.
   */
  scurr = angle_to_slice(proj->yrot);
  smin = (scurr - SLICE_CAP/4) & (SLICE_CAP-1);
  smax = (scurr + SLICE_CAP/4) & (SLICE_CAP-1);

  terrabuff_clear(terra, smin, smax);
  while (scan < SCAN_CAP && world &&
         (distance >> level) / TILE_SZ < (signed)world->xmax/2) {
    for (scurr = smin; scurr != smax; scurr = (scurr+1) & (SLICE_CAP-1))
      put_point(terra, proj->camera, scurr, distance, world, level,
                proj, dst->w);

    if (!terrabuff_next(terra, &smin, &smax)) break;

    ++scan;
    distance += distance_incr;
    distance_incr += METRE/4;
    /* Reduce level so that arc-length is less than 2m */
    /* (assume pi=3...) */
    while (distance * 6 / SLICE_CAP > (2*METRE << level) && world) {
      ++level;
      world = SLIST_NEXT(world, next);
    }
  }

  terrabuff_render(dst, terra, context);
}

static void render_basic_world_terrain_features(
  canvas* dst,
  const basic_world*restrict world,
  const rendering_context*restrict context)
{
  const perspective*restrict proj =
    ((const rendering_context_invariant*)context)->proj;
  coord x, z, ctx, ctz;
  unsigned major, major_max, type;
  signed minor, minor_min;
  signed major_axis_x, major_axis_z;
  signed minor_axis_x, minor_axis_z;
  signed dx, dz;
  signed long long fov_lcos, fov_lsin, fov_rcos, fov_rsin;
  int has_rendered;

  ctx = proj->camera[0] / TILE_SZ;
  ctz = proj->camera[2] / TILE_SZ;
  /* Calculate vectors for the FOV relative to the current direction, so that
   * we can quickly test whether tiles are visible according to (X,Z) coords.
   */
  fov_lcos = zo_cos(-proj->yrot + proj->fov/2);
  fov_lsin = zo_sin(-proj->yrot + proj->fov/2);
  fov_rcos = zo_cos(-proj->yrot - proj->fov/2);
  fov_rsin = zo_sin(-proj->yrot - proj->fov/2);

  if (abs(proj->yrot_cos) > abs(proj->yrot_sin)) {
    major_axis_x = 0;
    major_axis_z = (proj->yrot_cos > 0? -1 : +1);
    minor_axis_x = 1;
    minor_axis_z = 0;
  } else {
    major_axis_z = 0;
    major_axis_x = (proj->yrot_sin > 0? -1 : +1);
    minor_axis_z = 1;
    minor_axis_x = 0;
  }

  if (world->xmax/2-1 < 512)
    major_max = world->xmax/2-1;
  else
    major_max = 512;

#pragma omp parallel private(minor,minor_min,has_rendered,dx,dz,x,z,type)
  {
    minor_min = -32;
#pragma omp for
    for (major = 0; major < major_max; ++major) {
      has_rendered = 0;
      for (minor = minor_min; minor < (signed)major_max; ++minor) {
        dx = major*major_axis_x + minor*minor_axis_x;
        dz = major*major_axis_z + minor*minor_axis_z;
        x = (ctx + dx) & (world->xmax-1);
        z = (ctz + dz) & (world->zmax-1);

        /* Check whether within fov */
        if (dx*fov_lcos + dz*fov_lsin <= 0 &&
            dx*fov_rcos + dz*fov_rsin >= 0) {
          has_rendered = 1;

          type = world->tiles[basic_world_offset(world, x, z)].elts[0].type;
          if (terrain_renderers[type])
            (*terrain_renderers[type])(
              dst, world, context, x, z);
        } else {
          if (has_rendered)
            break;
          else
            minor_min = minor;
        }
      }

      minor_min -= 16;
      if (minor_min < -(signed)major_max)
        minor_min = 1-(signed)major_max;
    }
  } /* end parallel */
}

void render_basic_world(canvas* dst,
                        const basic_world*restrict world,
                        const rendering_context*restrict context) {
  render_basic_world_terrain(dst, world, context);
  /*render_basic_world_terrain_features(dst, world, context);*/
}
