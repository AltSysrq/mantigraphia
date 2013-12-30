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

#include "../rand.h"
#include "../graphics/canvas.h"
#include "../graphics/pencil.h"
#include "../graphics/perspective.h"
#include "../graphics/dm-proj.h"
#include "../world/terrain.h"
#include "context.h"
#include "terrain.h"

static void render_grass(canvas*restrict, const basic_world*restrict,
                         const rendering_context*restrict,
                         coord tx, coord tz);

void (*const terrain_renderers[4])(
  canvas*restrict, const basic_world*restrict,
  const rendering_context*restrict,
  coord tx, coord tz) =
{
  NULL, NULL, render_grass, NULL,
};

static void render_grass(canvas*restrict dst, const basic_world*restrict world,
                         const rendering_context*restrict context,
                         coord tx, coord tz) {
  const perspective*restrict proj =
    ((const rendering_context_invariant*)context)->proj;
  unsigned i, rand;
  pencil_spec pencil;
  dm_proj penproj;
  vc3 from, to;
  coord base_x, base_z;
  coord dist = abs(torus_dist(tx*TILE_SZ - proj->camera[0],
                              TILE_SZ*world->xmax))
             + abs(torus_dist(tz*TILE_SZ - proj->camera[2],
                              TILE_SZ*world->zmax));

  pencil_init(&pencil);
  pencil.colour = argb(255, 32, 140, 24);
  pencil.thickness = ZO_SCALING_FACTOR_MAX / 639;

  dm_init(&penproj);
  penproj.delegate = (drawing_method*)&pencil;
  penproj.proj = proj;
  penproj.nominal_weight = ZO_SCALING_FACTOR_MAX;
  penproj.near_clipping = 1;
  penproj.near_max = METRE/4;
  penproj.far_max = 10*METRE;
  penproj.far_clipping = 20*METRE;

  rand = chaos_of(chaos_accum(chaos_accum(0, tx), tz));
  base_x = tx * TILE_SZ;
  base_z = tz * TILE_SZ;

  for (i = 0; i < 64 / (1+dist/TILE_SZ/4); ++i) {
    from[0] = base_x + (lcgrand(&rand) & (TILE_SZ-1));
    from[2] = base_z + (lcgrand(&rand) & (TILE_SZ-1));
    from[1] = terrain_base_y(world, from[0], from[2]);
    to[0] = from[0];
    to[1] = from[1] + 128*MILLIMETRE + (lcgrand(&rand) % (128*MILLIMETRE));
    to[2] = from[2];

    dm_proj_draw_line(dst, &penproj,
                      from, ZO_SCALING_FACTOR_MAX,
                      to, ZO_SCALING_FACTOR_MAX);
  }

  dm_proj_flush(dst, &penproj);
}

