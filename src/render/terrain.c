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
#include "../defs.h"
#include "../graphics/canvas.h"
#include "../graphics/sybmap.h"
#include "../graphics/tscan.h"
#include "../graphics/perspective.h"
#include "../graphics/linear-paint-tile.h"
#include "../world/world.h"
#include "../world/terrain.h"
#include "context.h"
#include "terrain.h"

#define TEXSZ 256
#define TEXMASK (TEXSZ-1)

typedef struct {
  vo3 colours[4];
} terrain_context;
RENDERING_CONTEXT_STRUCT(render_terrain,terrain_context)

static canvas_pixel texture[TEXSZ*TEXSZ];

void render_terrain_init(void) {
  static const canvas_pixel pallet[] = {
    argb(255,255,255,255),
    argb(255,254,254,254),
    argb(255,252,252,252),
    argb(255,248,248,248),
    argb(255,240,240,240),
    argb(255,224,224,224),
    argb(255,192,192,192),
    argb(255,128,128,128),
    argb(255,64,64,64),
    argb(255,0,0,0),
  };

  linear_paint_tile_render(texture, TEXSZ, TEXSZ,
                           TEXSZ/4, 1,
                           pallet, lenof(pallet));
}

typedef struct {
  coord_offset screen_z;
  coord_offset world_y;
  vo3 colour;
} terrain_interp_data;

typedef struct {
  canvas*restrict dst;
  coord_offset ox, oy;
  zo_scaling_factor scale512;
} terrain_global_data;

static inline unsigned char mod8(unsigned char a, unsigned char b) {
  unsigned short s = a;
  s *= b;
  return s >> 8;
}

static inline canvas_pixel modulate(canvas_pixel mult, const vo3 colour) {
  return argb(
    get_alpha(mult),
    mod8(get_red(mult), colour[0]),
    mod8(get_green(mult), colour[1]),
    mod8(get_blue(mult), colour[2]));
}

static void shade_terrain_pixel(terrain_global_data* d,
                                coord_offset x, coord_offset y,
                                const coord_offset* vinterps) {
  const terrain_interp_data* interp = (const terrain_interp_data*)vinterps;
  unsigned tx = zo_scale((x + d->ox)*512, d->scale512) & TEXMASK;
  unsigned ty = zo_scale((y + interp->world_y + d->oy)*512, d->scale512)
              & TEXMASK;
  canvas_write(d->dst, x, y,
               modulate(texture[tx + ty*TEXSZ], interp->colour),
               interp->screen_z);
}

SHADE_TRIANGLE(shade_terrain, shade_terrain_pixel, 5)

void render_terrain_tile(canvas*restrict dst, sybmap* syb,
                         const basic_world* world,
                         const void*restrict vcontext,
                         coord tx, coord tz,
                         coord_offset logical_tx,
                         coord_offset logical_tz,
                         unsigned char szshift) {
  const rendering_context_invariant*restrict context = vcontext;
  const perspective*restrict proj = context->proj;
  const terrain_context*restrict tcxt = render_terrain_get(vcontext);
  vc3 world_coords[4];
  vo3 screen_coords[4];
  terrain_interp_data interp[4];
  terrain_global_data glob;
  int has_012 = 1, has_123 = 1;
  coord tx1 = (tx+1) & (world->xmax-1);
  coord tz1 = (tz+1) & (world->zmax-1);
  unsigned i;

  world_coords[0][0] = logical_tx * TILE_SZ << szshift;
  world_coords[0][1] = world->tiles[basic_world_offset(world, tx, tz)]
    .elts[0].altitude * TILE_YMUL;
  world_coords[0][2] = logical_tz * TILE_SZ << szshift;
  world_coords[1][0] = (logical_tx+1) * TILE_SZ << szshift;
  world_coords[1][1] = world->tiles[basic_world_offset(world, tx1, tz)]
    .elts[0].altitude * TILE_YMUL;
  world_coords[1][2] = logical_tz * TILE_SZ << szshift;
  world_coords[2][0] = logical_tx * TILE_SZ << szshift;
  world_coords[2][1] = world->tiles[basic_world_offset(world, tx, tz1)]
    .elts[0].altitude * TILE_YMUL;
  world_coords[2][2] = (logical_tz+1) * TILE_SZ << szshift;
  world_coords[3][0] = (logical_tx+1) * TILE_SZ << szshift;
  world_coords[3][1] = world->tiles[basic_world_offset(world, tx1, tz1)]
    .elts[0].altitude * TILE_YMUL;
  world_coords[3][2] = (logical_tz+1) * TILE_SZ << szshift;

  has_012 &= has_123 &= perspective_proj(screen_coords[1], world_coords[1],
                                         proj);
  has_012 &= has_123 &= perspective_proj(screen_coords[2], world_coords[2],
                                         proj);
  has_012 &= perspective_proj(screen_coords[0], world_coords[0], proj);
  has_123 &= perspective_proj(screen_coords[3], world_coords[3], proj);

  for (i = 0; i < 4; ++i) {
    interp[i].screen_z = screen_coords[i][2];
    if (interp[i].screen_z / (METRE/128) + METRE/32)
      interp[i].world_y = world_coords[i][1] /
                          (interp[i].screen_z / (METRE/128) + METRE/32);
  }
  memcpy(interp[0].colour, tcxt->colours[
           world->tiles[basic_world_offset(world, tx , tz )].elts[0].type],
         sizeof(vo3));
  memcpy(interp[1].colour, tcxt->colours[
           world->tiles[basic_world_offset(world, tx1, tz )].elts[0].type],
         sizeof(vo3));
  memcpy(interp[2].colour, tcxt->colours[
           world->tiles[basic_world_offset(world, tx , tz1)].elts[0].type],
         sizeof(vo3));
  memcpy(interp[3].colour, tcxt->colours[
           world->tiles[basic_world_offset(world, tx1, tz1)].elts[0].type],
         sizeof(vo3));

  glob.dst = dst;
  glob.ox = (-(signed)dst->w) * proj->yrot / proj->fov;
  glob.oy = 2 * ((signed)dst->h) * proj->rxrot / proj->fov;
  glob.scale512 = ZO_SCALING_FACTOR_MAX / dst->h;
  if (has_012) {
    shade_terrain(dst,
                  screen_coords[0], (coord_offset*)&interp[0],
                  screen_coords[1], (coord_offset*)&interp[1],
                  screen_coords[2], (coord_offset*)&interp[2],
                  &glob);
    sybmap_put(syb, screen_coords[0], screen_coords[1], screen_coords[2]);
  }

  if (has_123) {
    shade_terrain(dst,
                  screen_coords[1], (coord_offset*)&interp[1],
                  screen_coords[2], (coord_offset*)&interp[2],
                  screen_coords[3], (coord_offset*)&interp[3],
                  &glob);
    sybmap_put(syb, screen_coords[1], screen_coords[2], screen_coords[3]);
  }
}

static const vo3 colours[4] = {
  { 255, 255, 255 },
  { 120, 120, 120 },
  { 10, 120, 16 },
  { 140, 120, 8 },
};

void render_terrain_set_context(void* vcontext) {
  terrain_context* tcxt = render_terrain_getm(vcontext);
  memcpy(tcxt->colours, colours, sizeof(colours));
}
