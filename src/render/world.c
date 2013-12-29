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

void render_basic_world(
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
