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

#include <SDL.h>

#include <stdlib.h>
#include <string.h>

#include "alloc.h"
#include "coords.h"
#include "defs.h"

#include "graphics/canvas.h"
#include "graphics/parchment.h"
#include "world/world.h"
#include "world/terrain.h"
#include "render/world.h"
#include "control/mouselook.h"

#include "cosine-world.h"

#define SIZE 1024

typedef struct {
  game_state vtab;
  int is_running;
  coord x, z;
  mouselook_state look;
  parchment* bg;
  basic_world* world;
} cosine_world_state;

static game_state* cosine_world_update(cosine_world_state*, chronon);
static void cosine_world_draw(cosine_world_state*, canvas*);
static void cosine_world_key(cosine_world_state*, SDL_KeyboardEvent*);
static void cosine_world_mmotion(cosine_world_state*, SDL_MouseMotionEvent*);

static void cosine_world_init_world(cosine_world_state*);
static void cosine_world_delete(cosine_world_state*);

game_state* cosine_world_new(void) {
  cosine_world_state template = {
    { (game_state_update_t) cosine_world_update,
      (game_state_draw_t)   cosine_world_draw,
      (game_state_key_t)    cosine_world_key,
      NULL,
      (game_state_mmotion_t)cosine_world_mmotion,
      NULL, NULL, NULL,
    },
    1,
    0, 0,
    { 0, 0 },
    parchment_new(),
    basic_world_new(SIZE, SIZE, SIZE/8, SIZE/8),
  };

  cosine_world_state* this = xmalloc(sizeof(cosine_world_state));
  memcpy(this, &template, sizeof(template));

  cosine_world_init_world(this);

  mouselook_set(1);

  return (game_state*)this;
}

static void cosine_world_delete(cosine_world_state* this) {
  parchment_delete(this->bg);
  basic_world_delete(this->world);
  free(this);
}

static void cosine_world_init_world(cosine_world_state* this) {
  coord x, z;
  memset(this->world->tiles, 0,
         this->world->xmax * this->world->zmax * sizeof(tile_info));
  for (z = 0; z < SIZE; ++z) {
    for (x = 0; x < SIZE; ++x) {
      this->world->tiles[basic_world_offset(this->world, x, z)]
        .elts[0].altitude =
        (zo_cosms((x+z)*256, 10*METRE) + 20*METRE) / TILE_YMUL;
    }
  }

  basic_world_calc_next(this->world);
}

static game_state* cosine_world_update(cosine_world_state* this, chronon et) {
  if (this->is_running) {
    return (game_state*)this;
  } else {
    cosine_world_delete(this);
    return NULL;
  }
}

static void cosine_world_draw(cosine_world_state* this, canvas* dst) {
  perspective proj;
  sybmap* coverage[2];
  unsigned i;

  for (i = 0; i < 2; ++i) {
    coverage[i] = sybmap_new(dst->w, dst->h);
    sybmap_clear(coverage[i]);
  }

  proj.camera[0] = this->x;
  proj.camera[1] = terrain_base_y(this->world, this->x, this->z) + 2*METRE;
  proj.camera[2] = this->z;
  proj.torus_w = this->world->xmax * TILE_SZ;
  proj.torus_h = this->world->zmax * TILE_SZ;
  proj.yrot_cos = zo_cos(this->look.yrot);
  proj.yrot_sin = zo_sin(this->look.yrot);
  proj.rxrot_cos = zo_cos(this->look.rxrot);
  proj.rxrot_sin = zo_sin(this->look.rxrot);
  proj.near_clipping_plane = 1;
  perspective_init(&proj, dst, DEG_90);

  parchment_draw(dst, this->bg);
  basic_world_render(dst, coverage, this->world, &proj);

  for (i = 0; i < 2; ++i)
    sybmap_delete(coverage[i]);
}

static void cosine_world_key(cosine_world_state* this,
                             SDL_KeyboardEvent* evt) {
  if (SDL_KEYDOWN != evt->type) return;

  switch (evt->keysym.sym) {
  case SDLK_ESCAPE:
    this->is_running = 0;
    break;

  case SDLK_l:
    this->x -= zo_sinms(this->look.yrot, METRE/2);
    this->z -= zo_cosms(this->look.yrot, METRE/2);
    break;

  case SDLK_a:
    this->x += zo_sinms(this->look.yrot, METRE/2);
    this->z += zo_cosms(this->look.yrot, METRE/2);
    break;

  case SDLK_i:
    this->x -= zo_cosms(this->look.yrot, METRE/2);
    this->z += zo_sinms(this->look.yrot, METRE/2);
    break;

  case SDLK_e:
    this->x += zo_cosms(this->look.yrot, METRE/2);
    this->z -= zo_sinms(this->look.yrot, METRE/2);
    break;
  }

  this->x &= this->world->xmax*TILE_SZ - 1;
  this->z &= this->world->zmax*TILE_SZ - 1;
}

static void cosine_world_mmotion(cosine_world_state* this,
                                 SDL_MouseMotionEvent* evt) {
  mouselook_update(&this->look, this->bg, evt, DEG_90, DEG_90);
}
