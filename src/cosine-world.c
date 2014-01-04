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
#include "world/generate.h"
#include "render/world.h"
#include "render/context.h"
#include "render/fog.h"
#include "control/mouselook.h"

#include "cosine-world.h"

#define SIZE 4096

typedef struct {
  game_state vtab;
  int is_running;
  coord x, z;
  mouselook_state look;
  parchment* bg;
  basic_world* world;
  fog_effect* fog;
  rendering_context*restrict context;

  int moving_forward, moving_backward, moving_left, moving_right;
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
    basic_world_new(SIZE, SIZE, SIZE/256, SIZE/256),
    fog_effect_new(4096, fraction_of(32), fraction_of(64), 0),
    rendering_context_new(),
    0,0,0,0
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
  fog_effect_delete(this->fog);
  rendering_context_delete(this->context);
  free(this);
}

static void cosine_world_init_world(cosine_world_state* this) {
  world_generate(this->world, 3);
}

#define SPEED (4*METRES_PER_SECOND)
static game_state* cosine_world_update(cosine_world_state* this, chronon et) {
  fog_effect_update(this->fog, et);

  if (this->moving_forward) {
    this->x -= zo_sinms(this->look.yrot, et * SPEED);
    this->z -= zo_cosms(this->look.yrot, et * SPEED);
  }

  if (this->moving_backward) {
    this->x += zo_sinms(this->look.yrot, et * SPEED);
    this->z += zo_cosms(this->look.yrot, et * SPEED);
  }

  if (this->moving_left) {
    this->x -= zo_cosms(this->look.yrot, et * SPEED);
    this->z += zo_sinms(this->look.yrot, et * SPEED);
  }

  if (this->moving_right) {
    this->x += zo_cosms(this->look.yrot, et * SPEED);
    this->z -= zo_sinms(this->look.yrot, et * SPEED);
  }
  this->x &= this->world->xmax*TILE_SZ - 1;
  this->z &= this->world->zmax*TILE_SZ - 1;

  if (this->is_running) {
    return (game_state*)this;
  } else {
    cosine_world_delete(this);
    return NULL;
  }
}

static void cosine_world_draw(cosine_world_state* this, canvas* dst) {
  rendering_context_invariant context_inv;
  perspective proj;

  context_inv.proj = &proj;
  context_inv.long_yrot = this->look.yrot;

  proj.camera[0] = this->x;
  proj.camera[1] = terrain_base_y(this->world, this->x, this->z) + 2*METRE;
  proj.camera[2] = this->z;
  proj.torus_w = this->world->xmax * TILE_SZ;
  proj.torus_h = this->world->zmax * TILE_SZ;
  proj.yrot = this->look.yrot;
  proj.yrot_cos = zo_cos(this->look.yrot);
  proj.yrot_sin = zo_sin(this->look.yrot);
  proj.rxrot = this->look.rxrot;
  proj.rxrot_cos = zo_cos(this->look.rxrot);
  proj.rxrot_sin = zo_sin(this->look.rxrot);
  proj.near_clipping_plane = 1;
  perspective_init(&proj, dst, DEG_90);

  rendering_context_set(this->context, &context_inv);

  parchment_draw(dst, this->bg);
  render_basic_world(dst, this->world, this->context);
  fog_effect_apply(dst, this->fog, this->bg, 512*METRE, 1024*METRE);
}

static void cosine_world_key(cosine_world_state* this,
                             SDL_KeyboardEvent* evt) {
  int down = SDL_KEYDOWN == evt->type;

  switch (evt->keysym.sym) {
  case SDLK_ESCAPE:
    this->is_running = 0;
    break;

  case SDLK_l:
    this->moving_forward = down;
    break;

  case SDLK_a:
    this->moving_backward = down;
    break;

  case SDLK_i:
    this->moving_left = down;
    break;

  case SDLK_e:
    this->moving_right = down;
    break;
  }
}

static void cosine_world_mmotion(cosine_world_state* this,
                                 SDL_MouseMotionEvent* evt) {
  mouselook_update(&this->look, this->bg, evt, DEG_90, DEG_90);
}
