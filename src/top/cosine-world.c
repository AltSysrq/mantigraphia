/*-
 * Copyright (c) 2013, 2014, 2015 Jason Lingle
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
#include "math/coords.h"
#include "math/rand.h"
#include "defs.h"
#include "micromp.h"

#include "graphics/canvas.h"
#include "graphics/parchment.h"
#include "world/terrain-tilemap.h"
#include "world/terrain.h"
#include "world/env-vmap.h"
#include "world/generate.h"
#include "world/vmap-painter.h"
#include "world/nfa-turtle-vmap-painter.h"
#include "world/world-object-distributor.h"
#include "world/flower-map.h"
#include "gl/marshal.h"
#include "gl/auxbuff.h"
#include "render/context.h"
#include "render/terrain-tilemap.h"
#include "render/paint-overlay.h"
#include "render/env-vmap-manifold-renderer.h"
#include "render/skybox.h"
#include "render/flower-map-renderer.h"
#include "control/mouselook.h"
#include "resource/resource-loader.h"
#include "llua-bindings/lluas.h"

#include "cosine-world.h"

#define FOV (110 * 65536 / 360)
#define SIZE 4096
#define NUM_GRASS (1024*1024*4)
#define NUM_TREES 32768*2

/* This should be a configuration at some point, not a compile-time constant. */
#ifdef HIGH_RES_PAINT
#define RENDER_SIZE_REDUCTION 1
#define PAINT_SIZE_REDUCTION 1
#else
#define RENDER_SIZE_REDUCTION 2
#define PAINT_SIZE_REDUCTION 2
#endif

typedef struct {
  game_state self;
  unsigned seed;
  int is_running;
  coord x, z;
  chronon now;
  unsigned frame_no;
  mouselook_state look;
  parchment* bg;
  paint_overlay* overlay;
  terrain_tilemap* world;
  env_vmap* vmap;
  flower_map* flowers;
  skybox* sky;
  rendering_context*restrict context;
  env_vmap_manifold_renderer* vmap_manifold_renderer;
  flower_map_renderer* flower_renderer;

  int moving_forward, moving_backward, moving_left, moving_right;
  unsigned month_integral;
  fraction month_fraction;
  int advancing_time, sprinting;
  coord_offset camera_y_off;
  int use_paint_overlay, use_parchment;

  perspective proj;
} cosine_world_state;

static game_state* cosine_world_update(cosine_world_state*, chronon);
static void cosine_world_predraw(cosine_world_state*, canvas*);
static void cosine_world_draw(cosine_world_state*, canvas*);
static void cosine_world_key(cosine_world_state*, SDL_KeyboardEvent*);
static void cosine_world_mmotion(cosine_world_state*, SDL_MouseMotionEvent*);

static void cosine_world_init_world(cosine_world_state*);
static void cosine_world_delete(cosine_world_state*);

game_state* cosine_world_new(unsigned seed) {
  const vc3 origin = { 0, 0, 0 };
  cosine_world_state* this = zxmalloc(sizeof(cosine_world_state));

  this->self.update = (game_state_update_t)cosine_world_update;
  this->self.predraw = (game_state_predraw_t)cosine_world_predraw;
  this->self.draw = (game_state_draw_t)cosine_world_draw;
  this->self.key = (game_state_key_t)cosine_world_key;
  this->self.mmotion = (game_state_mmotion_t)cosine_world_mmotion;
  this->seed = seed;
  this->is_running = 1;
  this->bg = parchment_new();
  this->world = terrain_tilemap_new(SIZE, SIZE, SIZE/256, SIZE/256);
  this->vmap = env_vmap_new(SIZE, SIZE, 1);
  this->flowers = flower_map_new(SIZE, SIZE);
  this->sky = skybox_new(seed + 7512);
  this->context = rendering_context_new();
  this->camera_y_off = 7 * METRE / 4;
  this->use_paint_overlay = 1;
  this->use_parchment = 1;
  parchment_set_interpolate_postprocess(this->bg, 1 != PAINT_SIZE_REDUCTION);

  this->vmap_manifold_renderer = env_vmap_manifold_renderer_new(
    this->vmap, (const env_voxel_graphic*const*)&res_voxel_graphics,
    origin, this->world,
    (coord(*)(const void*,coord,coord))terrain_base_y);
  this->flower_renderer = flower_map_renderer_new(
    this->flowers, res_flower_graphics, this->world);

  rl_clear();
  rl_set_frozen(0);
  ntvp_clear_all();
  wod_init(this->world, this->flowers, seed + 6420);
  lluas_init();
  lluas_load_file("share/llua/core.lua", 65536);
  lluas_load_file("share/llua/oak-tree.lua", 65536);
  lluas_load_file("share/llua/cherry-tree.lua", 65536);
  lluas_load_file("share/llua/common-flowers.lua", 65536);
  lluas_load_file("share/llua/test-resources.lua", 65536);
  lluas_invoke_global("load_resources", 1<<24);
  if (lluas_get_error_status())
    errx(EX_SOFTWARE, "Lluas not OK, aborting");
  rl_set_frozen(1);

  cosine_world_init_world(this);
  mouselook_set(1);

  return (game_state*)this;
}

static void cosine_world_delete(cosine_world_state* this) {
  if (this->overlay) paint_overlay_delete(this->overlay);
  parchment_delete(this->bg);
  env_vmap_manifold_renderer_delete(this->vmap_manifold_renderer);
  flower_map_renderer_delete(this->flower_renderer);
  env_vmap_delete(this->vmap);
  flower_map_delete(this->flowers);
  terrain_tilemap_delete(this->world);
  skybox_delete(this->sky);
  rendering_context_delete(this->context);
  free(this);
}

static void cosine_world_init_world(cosine_world_state* this) {
  unsigned seed = this->seed;
  world_generate(this->world, seed);

  vmap_painter_init(this->vmap);
  lluas_invoke_global("populate_vmap", 1 << 24);
  vmap_painter_flush();

  world_add_shadow(this->world, this->vmap);
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
  this->x &= this->world->xmax*TILE_SZ - 1;
  this->z &= this->world->zmax*TILE_SZ - 1;

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
  canvas render_dst;
  canvas after_paint_overlay;

  canvas_init_thin(&render_dst, dst->w / RENDER_SIZE_REDUCTION,
                   dst->h / RENDER_SIZE_REDUCTION);
  canvas_init_thin(&after_paint_overlay,
                   dst->w/PAINT_SIZE_REDUCTION, dst->h/PAINT_SIZE_REDUCTION);

  context_inv.proj = proj;
  context_inv.long_yrot = this->look.yrot;
  context_inv.screen_width = dst->w / RENDER_SIZE_REDUCTION;
  context_inv.screen_height = dst->h / RENDER_SIZE_REDUCTION;
  context_inv.now = this->now;
  context_inv.frame_no = this->frame_no++;
  context_inv.month_integral = this->month_integral;
  context_inv.month_fraction = this->month_fraction;

  proj->camera[0] = this->x;
  proj->camera[1] = terrain_base_y(this->world, this->x, this->z) +
                    this->camera_y_off;
  proj->camera[2] = this->z;
  proj->torus_w = this->world->xmax * TILE_SZ;
  proj->torus_h = this->world->zmax * TILE_SZ;
  proj->yrot = this->look.yrot;
  proj->yrot_cos = zo_cos(this->look.yrot);
  proj->yrot_sin = zo_sin(this->look.yrot);
  proj->rxrot = this->look.rxrot;
  proj->rxrot_cos = zo_cos(this->look.rxrot);
  proj->rxrot_sin = zo_sin(this->look.rxrot);
  proj->near_clipping_plane = 1;
  perspective_init(proj, &render_dst, FOV);

  rendering_context_set(this->context, &context_inv);

  if (!this->overlay)
    this->overlay = paint_overlay_new(&after_paint_overlay);
}

static void cosine_world_draw(cosine_world_state* this, canvas* dst) {
  /* GL calls don't actually execute until after this function returns, so
   * ensure that the value remains valid until then by making the value static.
   */
  static canvas before_paint_overlay;
  static canvas after_paint_overlay;

  canvas_init_thin(&before_paint_overlay,
                   dst->w/RENDER_SIZE_REDUCTION, dst->h/RENDER_SIZE_REDUCTION);
  canvas_init_thin(&after_paint_overlay,
                   dst->w/PAINT_SIZE_REDUCTION, dst->h/PAINT_SIZE_REDUCTION);

  if (this->use_paint_overlay)
    paint_overlay_preprocess(this->overlay, this->context,
                             &before_paint_overlay, dst);
  else if (this->use_parchment)
    parchment_preprocess(this->bg, &before_paint_overlay);
  else
    auxbuff_target(0, dst->w, dst->h);

  glm_clear(GL_DEPTH_BUFFER_BIT);
  skybox_render(&before_paint_overlay, this->sky, this->context);
  render_terrain_tilemap(&before_paint_overlay, this->world, this->context);
  render_env_vmap_manifolds(
    &before_paint_overlay, this->vmap_manifold_renderer, this->context);
  render_flower_map(&before_paint_overlay, this->flower_renderer,
                    this->context);
  ump_join();

  if (this->use_paint_overlay) {
    if (this->use_parchment)
      parchment_preprocess(this->bg, &after_paint_overlay);
    else
      auxbuff_target(0, dst->w, dst->h);

    paint_overlay_postprocess(this->overlay, this->context);
  }

  if (this->use_parchment) {
    auxbuff_target(0, dst->w, dst->h);
    parchment_postprocess(this->bg, dst, &after_paint_overlay);
  }
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

  case SDLK_n:
    if (down)
      this->use_paint_overlay = !this->use_paint_overlay;
    break;

  case SDLK_r:
    if (down)
      this->use_parchment = !this->use_parchment;
    break;

  case SDLK_t:
    if (down && this->overlay)
      paint_overlay_set_using_high_res_texture(
        this->overlay, !paint_overlay_is_using_high_res_texture(this->overlay));
    break;

  case SDLK_d:
    if (down && this->bg)
      parchment_set_interpolate_postprocess(
        this->bg, !parchment_get_interpolate_postprocess(this->bg));
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
