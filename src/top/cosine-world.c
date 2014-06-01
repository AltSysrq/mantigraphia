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
#include "micromp.h"

#include "graphics/canvas.h"
#include "graphics/parchment.h"
#include "world/basic-world.h"
#include "world/propped-world.h"
#include "world/terrain.h"
#include "world/props.h"
#include "world/generate.h"
#include "render/propped-world.h"
#include "render/context.h"
#include "control/mouselook.h"

#include "cosine-world.h"

#define FOV (110 * 65536 / 360)
#define SIZE 4096
#define NUM_GRASS (1024*1024*4)
#define NUM_TREES 32768*2

typedef struct {
  game_state vtab;
  unsigned seed;
  int is_running;
  coord x, z;
  chronon now;
  unsigned frame_no;
  mouselook_state look;
  parchment* bg;
  propped_world* world;
  rendering_context*restrict context;

  int moving_forward, moving_backward, moving_left, moving_right;
  unsigned month_integral;
  fraction month_fraction;
  int advancing_time, sprinting;
  coord_offset camera_y_off;

  perspective proj;
} cosine_world_state;

static void deserialise(cosine_world_state*, FILE* in);
static game_state* cosine_world_update(cosine_world_state*, chronon);
static void cosine_world_predraw(cosine_world_state*, canvas*);
static void cosine_world_draw(cosine_world_state*, canvas*);
static void cosine_world_key(cosine_world_state*, SDL_KeyboardEvent*);
static void cosine_world_mmotion(cosine_world_state*, SDL_MouseMotionEvent*);

static void cosine_world_init_world(cosine_world_state*);
static void cosine_world_delete(cosine_world_state*);

game_state* cosine_world_new(unsigned seed) {
  cosine_world_state template = {
    { (game_state_update_t) cosine_world_update,
      (game_state_predraw_t)cosine_world_predraw,
      (game_state_draw_t)   cosine_world_draw,
      (game_state_key_t)    cosine_world_key,
      NULL,
      (game_state_mmotion_t)cosine_world_mmotion,
      NULL, NULL, NULL,
    },
    seed,
    1,
    0, 0, 0, 0,
    { 0, 0 },
    parchment_new(),
    propped_world_new(
      basic_world_new(SIZE, SIZE, SIZE/256, SIZE/256),
      NUM_GRASS, NUM_TREES),
    rendering_context_new(),
    0,0,0,0,
    0,0,0,0,
    2 * METRE
  };
  FILE* in = fopen("/tmp/mantigraphia.dump", "rb");

  cosine_world_state* this = xmalloc(sizeof(cosine_world_state));
  memcpy(this, &template, sizeof(template));

  if (in) {
    deserialise(this, in);
    fclose(in);
  } else {
    cosine_world_init_world(this);
  }

  mouselook_set(1);

  return (game_state*)this;
}

static void cosine_world_delete(cosine_world_state* this) {
  parchment_delete(this->bg);
  propped_world_delete(this->world);
  rendering_context_delete(this->context);
  free(this);
}

static void cosine_world_init_world(cosine_world_state* this) {
  FILE* out;
  unsigned seed = this->seed;
  world_generate(this->world->terrain, seed);
  grass_generate(this->world->grass.props,
                 this->world->grass.size,
                 this->world->terrain, seed+1);
  props_sort_z(this->world->grass.props,
               this->world->grass.size);
  trees_generate(this->world->trees[0].props,
                 this->world->trees[0].size,
                 this->world->terrain, seed+2, seed+3);
  props_sort_z(this->world->trees[0].props,
               this->world->trees[0].size);
  trees_generate(this->world->trees[1].props,
                 this->world->trees[1].size,
                 this->world->terrain, seed+2, seed+4);
  props_sort_z(this->world->trees[1].props,
               this->world->trees[1].size);

  out = fopen("world.bmp", "wb");
  if (out) {
    basic_world_bmp_dump(out, this->world->terrain);
    fclose(out);
  }
}

#define SPEED (4*METRES_PER_SECOND)
static game_state* cosine_world_update(cosine_world_state* this, chronon et) {
  velocity speed = SPEED * (this->sprinting? 8 : 1);
  this->now += et;

  if (this->moving_forward) {
    this->x -= zo_sinms(this->look.yrot, et * speed);
    this->z -= zo_cosms(this->look.yrot, et * speed);
  }

  if (this->moving_backward) {
    this->x += zo_sinms(this->look.yrot, et * speed);
    this->z += zo_cosms(this->look.yrot, et * speed);
  }

  if (this->moving_left) {
    this->x -= zo_cosms(this->look.yrot, et * speed);
    this->z += zo_sinms(this->look.yrot, et * speed);
  }

  if (this->moving_right) {
    this->x += zo_cosms(this->look.yrot, et * speed);
    this->z -= zo_sinms(this->look.yrot, et * speed);
  }
  this->x &= this->world->terrain->xmax*TILE_SZ - 1;
  this->z &= this->world->terrain->zmax*TILE_SZ - 1;

  if (this->month_integral < 8 || this->month_fraction < fraction_of(1)) {
    this->month_fraction +=
      fraction_of(this->advancing_time? 8*SECOND : 64*SECOND) * et;
    if (this->month_fraction > fraction_of(1)) {
      if (this->month_integral < 8) {
        this->month_fraction -= fraction_of(1);
        ++this->month_integral;
      } else {
        this->month_fraction = fraction_of(1);
      }
    }
  }

  if (this->is_running) {
    return (game_state*)this;
  } else {
    cosine_world_delete(this);
    return NULL;
  }
}

static void cosine_world_predraw(cosine_world_state* this, canvas* dst) {
  rendering_context_invariant context_inv;
  perspective* proj = &this->proj;

  context_inv.proj = proj;
  context_inv.long_yrot = this->look.yrot;
  context_inv.screen_width = dst->w;
  context_inv.screen_height = dst->h;
  context_inv.now = this->now;
  context_inv.frame_no = this->frame_no++;
  context_inv.month_integral = this->month_integral;
  context_inv.month_fraction = this->month_fraction;

  proj->camera[0] = this->x;
  proj->camera[1] = terrain_base_y(this->world->terrain, this->x, this->z) +
                    this->camera_y_off;
  proj->camera[2] = this->z;
  proj->torus_w = this->world->terrain->xmax * TILE_SZ;
  proj->torus_h = this->world->terrain->zmax * TILE_SZ;
  proj->yrot = this->look.yrot;
  proj->yrot_cos = zo_cos(this->look.yrot);
  proj->yrot_sin = zo_sin(this->look.yrot);
  proj->rxrot = this->look.rxrot;
  proj->rxrot_cos = zo_cos(this->look.rxrot);
  proj->rxrot_sin = zo_sin(this->look.rxrot);
  proj->near_clipping_plane = 1;
  perspective_init(proj, dst, FOV);

  rendering_context_set(this->context, &context_inv);
}

static void cosine_world_draw(cosine_world_state* this, canvas* dst) {
  parchment_draw(dst, this->bg);
  render_propped_world(dst, this->world, this->context);
  ump_join();
  parchment_postprocess(this->bg, dst);
}

static void deserialise(cosine_world_state* this, FILE* in) {
  cosine_world_state stored;

  printf("Deserialising state from /tmp/mantigraphia.dump\n");

  if (1 != fread(&stored, sizeof(stored), 1, in))
    err(EX_OSERR, "Error deserialising");

  /* It's more concise to replace non-stored stuff in stored then copy over,
   * than it is to cherry-pick the stored items over.
   */
  stored.world = propped_world_deserialise(in);
  stored.bg = this->bg;
  stored.context = this->context;
  stored.vtab = this->vtab;
  stored.moving_forward = stored.moving_backward = 0;
  stored.moving_left = stored.moving_right = 0;
  stored.advancing_time = 0;
  propped_world_delete(this->world);
  memcpy(this, &stored, sizeof(stored));

  printf("Deserialisation complete\n");
}

static void serialise(cosine_world_state* this) {
  FILE* out = fopen("/tmp/mantigraphia.dump", "wb");
  if (!out) {
    warn("Unable to open dump file");
    return;
  }

  if (1 != fwrite(this, sizeof(*this), 1, out)) {
    warn("Error writing to dump file");
    goto done;
  }

  propped_world_serialise(out, this->world);
  printf("Dump written to /tmp/mantigraphia.dump\n");

  done:
  fclose(out);
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

  case SDLK_F11:
    evt->keysym.sym = SDLK_F6;
  case SDLK_F1:
  case SDLK_F2:
  case SDLK_F3:
  case SDLK_F4:
  case SDLK_F5:
  case SDLK_F6:
  case SDLK_F7:
  case SDLK_F8:
  case SDLK_F9:
    if (down) {
      this->month_integral = evt->keysym.sym - SDLK_F1;
      this->month_fraction = 0;
    }
    break;

  case SDLK_F10:
    if (down) {
      this->month_integral = 8;
      this->month_fraction = fraction_of(1);
    }
    break;

  case SDLK_F12:
    this->advancing_time = down;
    break;

  case SDLK_LSHIFT:
  case SDLK_RSHIFT:
    this->sprinting = down;
    break;

  case SDLK_RETURN:
    if (down)
      serialise(this);
    break;

  case SDLK_PAGEUP:
    if (down)
      this->camera_y_off += METRE/2;
    break;

  case SDLK_PAGEDOWN:
    if (down)
      this->camera_y_off -= METRE/2;
    break;
  }
}

static void cosine_world_mmotion(cosine_world_state* this,
                                 SDL_MouseMotionEvent* evt) {
  mouselook_update(&this->look, this->bg, evt, FOV, FOV);
}
