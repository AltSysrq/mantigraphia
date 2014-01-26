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

#include <stdlib.h>

#include "../alloc.h"
#include "../coords.h"
#include "../micromp.h"
#include "../graphics/canvas.h"
#include "../graphics/perspective.h"
#include "../world/basic-world.h"
#include "../world/terrain.h"
#include "context.h"
#include "terrabuff.h"
#include "terrain.h"
#include "basic-world.h"

#define SLICE_CAP 256
#define SCAN_CAP 128

/* Our rendering context needs one terrabuff per thread. */
RENDERING_CONTEXT_STRUCT(render_basic_world, terrabuff**)

void render_basic_world_context_ctor(rendering_context*restrict context) {
  terrabuff** buffers;
  unsigned i;

  buffers = xmalloc(sizeof(terrabuff*) * (ump_num_workers()+1));

  for (i = 0; i < ump_num_workers() + 1; ++i)
    buffers[i] = terrabuff_new(SLICE_CAP, SCAN_CAP);

  *render_basic_world_getm(context) = buffers;
}

void render_basic_world_context_dtor(rendering_context*restrict context) {
  unsigned i;
  terrabuff** buffers = *render_basic_world_get(context);

  for (i = 0; i < ump_num_workers() + 1; ++i)
    terrabuff_delete(buffers[i]);

  free(buffers);
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
                      terrabuff_slice slice,
                      coord_offset distance, coord_offset sample_len,
                      const basic_world*restrict world, unsigned char level,
                      const perspective* proj, unsigned xmax) {
  vc3 point;
  vo3 relative, projected;
  coord tx, tz;
  coord_offset sox, soz;
  unsigned long long altitude_sum = 0;
  unsigned red_sum = 0, green_sum = 0, blue_sum = 0;
  unsigned sample_cnt = 0;
  simd4 colour;
  int clamped = 0;

  point[0] = centre[0] - zo_sinms(slice_to_angle(slice), distance);
  point[2] = centre[2] - zo_cosms(slice_to_angle(slice), distance);
  tx = (point[0] >> level) & (world->xmax*TILE_SZ - 1);
  tz = (point[2] >> level) & (world->zmax*TILE_SZ - 1);
  sample_len >>= level;

  for (soz = -sample_len; soz <= sample_len; soz += TILE_SZ) {
    for (sox = -sample_len; sox <= sample_len; sox += TILE_SZ) {
      altitude_sum += terrain_base_y(world,
                                     (tx + sox) & (world->xmax*TILE_SZ - 1),
                                     (tz + soz) & (world->zmax*TILE_SZ - 1));
      colour = terrain_colour(world,
                              (tx + sox) & (world->xmax*TILE_SZ - 1),
                              (tz + soz) & (world->zmax*TILE_SZ - 1));
      red_sum   += simd_vs(colour, 0);
      green_sum += simd_vs(colour, 1);
      blue_sum  += simd_vs(colour, 2);
      ++sample_cnt;
    }
  }

  point[1] = altitude_sum / sample_cnt;

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

  terrabuff_put(dst, projected,
                argb(255, red_sum / sample_cnt, green_sum / sample_cnt,
                     blue_sum / sample_cnt),
                xmax);
}

static canvas* render_basic_world_terrain_dst;
static const basic_world*restrict render_basic_world_terrain_world;
static const rendering_context*restrict render_basic_world_terrain_context;
static void render_basic_world_terrain_subrange(unsigned, unsigned);
static ump_task render_basic_world_terrain_task = {
  render_basic_world_terrain_subrange,
  0, /* Dynamic (num workers) */
  0, /* Unused (synchronous) */
};

static void render_basic_world_terrain(canvas* dst,
                                       const basic_world*restrict world,
                                       const rendering_context*restrict context)
{
  terrabuff** buffers = *render_basic_world_get(context);
  unsigned i;

  render_basic_world_terrain_dst = dst;
  render_basic_world_terrain_world = world;
  render_basic_world_terrain_context = context;
  render_basic_world_terrain_task.num_divisions = 1 + ump_num_workers();
  ump_join();
  ump_run_sync(&render_basic_world_terrain_task);

  for (i = 1; i < 1 + ump_num_workers(); ++i)
    terrabuff_merge(buffers[0], buffers[i]);

  terrabuff_render(dst, buffers[0], context);
}

static void render_basic_world_terrain_subrange(unsigned ix, unsigned count) {
  canvas* dst = render_basic_world_terrain_dst;
  const basic_world*restrict world = render_basic_world_terrain_world;
  const rendering_context*restrict context = render_basic_world_terrain_context;

  const perspective*restrict proj =
    ((const rendering_context_invariant*)context)->proj;
  terrabuff* terra = render_basic_world_get(context)[0][ix];
  unsigned scan = 0;
  terrabuff_slice smin, scurr, smax;
  terrabuff_slice absolute_smin, absolute_smax, absolute_range;
  terrabuff_slice local_smin, local_smax;
  unsigned char level = 0;
  coord_offset distance = 1 * METRE, distance_incr = 1 * METRE;

  /* Start by assuming 180 deg effective field. The terrabuff will give us
   * better boundaries after the first scan.
   */
  scurr = angle_to_slice(proj->yrot);
  absolute_smin = (scurr - SLICE_CAP/4) & (SLICE_CAP-1);
  absolute_smax = (scurr + SLICE_CAP/4) & (SLICE_CAP-1);
  absolute_range = (absolute_smax - absolute_smin) & (SLICE_CAP-1);

  /* Limit the current thread to the slices to which it is dedicated. Work is
   * distributed quadratically, since threads in the centre will generally
   * render long ranges, whereas those on the edges will terminate early.
   */
  local_smin = (absolute_smin + ix * absolute_range / count) & (SLICE_CAP-1);
  local_smax = (absolute_smin + (ix+1)*absolute_range / count) & (SLICE_CAP-1);

  terrabuff_clear(terra, absolute_smin, absolute_smax);
  smin = local_smin;
  smax = local_smax;

  while (scan < SCAN_CAP && world &&
         (distance >> level) / TILE_SZ < (signed)world->xmax/2) {
    terrabuff_bounds_override(terra, smin, smax);

    for (scurr = smin; scurr != smax; scurr = (scurr+1) & (SLICE_CAP-1))
      put_point(terra, proj->camera, scurr, distance, distance_incr,
                world, level, proj, dst->w);

    if (!terrabuff_next(terra, &smin, &smax)) break;

    /* Clamp smin and smax to local constraints. Local minimum and maximum are
     * forced unless this thread is on the edge.
     */
    if (ix)
      smin = local_smin;
    if (ix+1 != count)
      smax = local_smax;

    /* Stop if smin > smax */
    if (((smax - smin) & (SLICE_CAP-1)) > SLICE_CAP/2 || smax == smin) {
      terrabuff_cancel_scan(terra);
      break;
    }

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
}

#define MAJOR_MAX 512

static void render_basic_world_terrain_features_slice(
  canvas* dst,
  const basic_world*restrict world,
  const rendering_context*restrict context,
  coord_offset xmin, coord_offset xmax)
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
  signed long long left_bound, right_bound;

  ctx = proj->camera[0] / TILE_SZ;
  ctz = proj->camera[2] / TILE_SZ;
  /* Calculate vectors for the FOV relative to the current direction, so that
   * we can quickly test whether tiles are visible according to (X,Z) coords.
   */
  left_bound = ((signed)dst->w/2) - xmin;
  /* Would be fov/2, but we need to give some wiggle room for rounding errors
   * when calculating the projection.
   */
  left_bound *= 0xFFFF & (signed)(proj->fov*78/128);
  left_bound /= dst->w;
  left_bound -= proj->yrot;
  right_bound = ((signed)dst->w/2) - xmax;
  right_bound *= 0xFFFF & (signed)(proj->fov*78/128);
  right_bound /= dst->w;
  right_bound -= proj->yrot;
  fov_lcos = zo_cos(left_bound);
  fov_lsin = zo_sin(left_bound);
  fov_rcos = zo_cos(right_bound);
  fov_rsin = zo_sin(right_bound);

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

  if (world->xmax/2-1 < MAJOR_MAX)
    major_max = world->xmax/2-1;
  else
    major_max = MAJOR_MAX;

  minor_min = -32;
  for (major = 0; major < major_max; ++major) {
    has_rendered = 0;
    for (minor = minor_min; minor < (signed)major_max; ++minor) {
      dx = major*major_axis_x + minor*minor_axis_x;
      dz = major*major_axis_z + minor*minor_axis_z;
      x = (ctx + dx) & (world->xmax-1);
      z = (ctz + dz) & (world->zmax-1);

      /* Check whether within fov */
      if (zo_scale(dx,fov_lcos) + zo_scale(dz,fov_lsin) <= 0 &&
          zo_scale(dx,fov_rcos) + zo_scale(dz,fov_rsin) >= 0) {
        has_rendered = 1;

        type = world->tiles[basic_world_offset(world, x, z)].elts[0].type;
        if (terrain_renderers[type])
          (*terrain_renderers[type])(dst, world, context, x, z);

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
}

static void render_basic_world_terrain_features_slice_ump(unsigned ordinal,
                                                          unsigned count);

static ump_task render_basic_world_terrain_features_task = {
  render_basic_world_terrain_features_slice_ump,
  0, /* Set to number of workers */
  0, /* Unused (Synchronous) */
};
static canvas*restrict render_basic_world_terrain_features_dst;
static const basic_world*restrict render_basic_world_terrain_features_world;
static const rendering_context*restrict
    render_basic_world_terrain_features_context;

static void render_basic_world_terrain_features_slice_ump(unsigned ordinal,
                                                          unsigned count) {
  canvas*restrict dst = render_basic_world_terrain_features_dst;

  render_basic_world_terrain_features_slice(
    dst,
    render_basic_world_terrain_features_world,
    render_basic_world_terrain_features_context,
    ordinal * dst->w / count,
    (ordinal+1) * dst->w / count);
}

static void render_basic_world_terrain_features(
  canvas*restrict dst,
  const basic_world*restrict world,
  const rendering_context*restrict context)
{
  render_basic_world_terrain_features_task.num_divisions = ump_num_workers()+1;
  render_basic_world_terrain_features_dst = dst;
  render_basic_world_terrain_features_world = world;
  render_basic_world_terrain_features_context = context;
  ump_join();
  ump_run_sync(&render_basic_world_terrain_features_task);
}

void render_basic_world(canvas* dst,
                        const basic_world*restrict world,
                        const rendering_context*restrict context) {
  render_basic_world_terrain(dst, world, context);
  render_basic_world_terrain_features(dst, world, context);
}