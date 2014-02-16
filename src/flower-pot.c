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

#include <time.h>

#include "alloc.h"
#include "coords.h"
#include "defs.h"
#include "micromp.h"

#include "graphics/canvas.h"
#include "graphics/parchment.h"
#include "graphics/perspective.h"
#include "graphics/pencil.h"
#include "graphics/brush.h"
#include "graphics/tiled-texture.h"
#include "graphics/linear-paint-tile.h"
#include "graphics/hull.h"
#include "graphics/dm-proj.h"
#include "graphics/tscan.h"
#include "graphics/fast-brush.h"
#include "render/turtle.h"
#include "render/draw-queue.h"
#include "render/lsystem.h"

#include "flower-pot.h"

typedef struct {
  game_state vtab;
  int is_running;
  angle rotation;
  parchment* bg;
  drawing_queue* queue;
  drawing_method* stem_brush;
} flower_pot_state;

static game_state* flower_pot_update(flower_pot_state*, chronon);
static void init_data(void);
static void flower_pot_predraw(flower_pot_state* this, canvas* dst) { }
static void flower_pot_draw(flower_pot_state*, canvas*);
static void flower_pot_key(flower_pot_state*, SDL_KeyboardEvent*);
static void flower_pot_mbutton(flower_pot_state*, SDL_MouseButtonEvent*);
static void flower_pot_mmotion(flower_pot_state*, SDL_MouseMotionEvent*);
static void flower_pot_scroll(flower_pot_state*, SDL_MouseWheelEvent*);

static lsystem flower_system;

#define FLOWER_UNIT (TURTLE_UNIT*4)

game_state* flower_pot_new(void) {
  brush_spec stem_brush;
  flower_pot_state template = {
    { (game_state_update_t) flower_pot_update,
      (game_state_predraw_t)flower_pot_predraw,
      (game_state_draw_t)   flower_pot_draw,
      (game_state_key_t)    flower_pot_key,
      (game_state_mbutton_t)flower_pot_mbutton,
      (game_state_mmotion_t)flower_pot_mmotion,
      (game_state_scroll_t) flower_pot_scroll,
      0, 0,
    },
    1, 0,
    parchment_new(),
    drawing_queue_new(),
    NULL
  };
  flower_pot_state* this = xmalloc(sizeof(flower_pot_state));
  unsigned i;

  memcpy(this, &template, sizeof(flower_pot_state));
  init_data();

  brush_init(&stem_brush);
  for (i = 0; i < MAX_BRUSH_BRISTLES; ++i)
    stem_brush.init_bristles[i] = 1;
  this->stem_brush = fast_brush_new(&stem_brush, 64, 1024, 0);

  lsystem_compile(&flower_system,
                  "9 9 8.8",
                  "8 8 7.7",
                  "7 7 6.6",
                  "6 6 5.5",
                  "5 5 4.4",
                  "4 4 3.3",
                  ". . BrBrBrBrBrBr",
                  "B B [b8P]",
                  "P P [b9p]r[b9p]r[b9p]r[b9p]r[b9p]r[b9p]",
                  NULL);

  return (game_state*)this;
}

static game_state* flower_pot_update(flower_pot_state* this, chronon et) {
  if (this->is_running) {
    return (game_state*)this;
  } else {
    parchment_delete(this->bg);
    fast_brush_delete(this->stem_brush);
    drawing_queue_delete(this->queue);
    free(this);
    return NULL;
  }
}

static void flower_pot_key(flower_pot_state* this, SDL_KeyboardEvent* evt) {
  printf("key: %s scancode %d (%s), keycode %d (%s)\n",
         (SDL_KEYDOWN == evt->type? "down" : "up"),
         (int)evt->keysym.scancode, SDL_GetScancodeName(evt->keysym.scancode),
         (int)evt->keysym.sym, SDL_GetKeyName(evt->keysym.sym));

  if (SDL_KEYDOWN == evt->type) {
    if (SDLK_ESCAPE == evt->keysym.sym)
      this->is_running = 0;
    else if (SDLK_LEFT == evt->keysym.sym)
      this->rotation += 256;
    else if (SDLK_RIGHT == evt->keysym.sym)
      this->rotation -= 256;
  }
}

static void flower_pot_mbutton(flower_pot_state* this,
                               SDL_MouseButtonEvent* evt) {
  printf("button: %s %d\n",
         (SDL_PRESSED == evt->state? "pressed" : "released"),
         (int)evt->button);
}

static void flower_pot_mmotion(flower_pot_state* this,
                               SDL_MouseMotionEvent* evt) {
  printf("motion: (%d,%d) delta (%d,%d)\n",
         evt->x, evt->y, evt->xrel, evt->yrel);
}

static void flower_pot_scroll(flower_pot_state* this,
                              SDL_MouseWheelEvent* evt) {
  printf("scroll: (%d,%d)\n", evt->x, evt->y);
}

/*****************************************************************************/

#define TEXDIM 64
#define ROTRES 32
#define NPET_R 6
#define NPET_H 5
#define PET_W 32
#define PET_YOFF 24
#define STEM_H 200
#define STEM_BASE 80

static canvas_pixel clay[TEXDIM*TEXDIM], soil[TEXDIM*TEXDIM];
static const canvas_pixel clay_pallet[] = {
  argb(0, 0xFF, 0x66, 0x33),
  argb(0, 0xFF, 0x66, 0x00),
  argb(0, 0xCC, 0x66, 0x33),
  argb(0, 0xCC, 0x66, 0x00),
  argb(0, 0xFF, 0x66, 0x33),
  argb(0, 0xFF, 0x66, 0x00),
  argb(0, 0xCC, 0x66, 0x33),
  argb(0, 0xCC, 0x66, 0x00),
  argb(0, 0x7F, 0x7F, 0x7F),
};
static const canvas_pixel soil_pallet[6] = {
  argb(0, 0x66, 0x33, 0x00),
  argb(0, 0x33, 0x33, 0x00),
  argb(0, 0x66, 0x33, 0x00),
  argb(0, 0x33, 0x33, 0x00),
  argb(0, 0x00, 0x00, 0x00),
  argb(0, 0x99, 0x66, 0x00),
};

static hull_triangle pot_mesh[ROTRES*4], soil_mesh[ROTRES];
static coord_offset pot_verts[ROTRES*2*3], soil_verts[3*(ROTRES+1)];

static const canvas_pixel plant_colours[6] = {
  argb(0, 0, 48, 0),
  argb(0, 0, 64, 0),
  argb(0, 0, 64, 0),
  argb(0, 8, 72, 8),
  argb(0, 16, 85, 16),
  argb(0, 32, 108, 32),
};

static const canvas_pixel petal_colours[4] = {
  argb(0, 220, 220, 64),
  argb(0, 220, 64, 220),
  argb(0, 240, 96, 240),
  argb(0, 255, 128, 255),
};

static void init_data(void) {
  unsigned h, i, j;
  angle theta;

  linear_paint_tile_render(clay, TEXDIM, TEXDIM,
                           2, 16, clay_pallet,
                           sizeof(clay_pallet) / sizeof(clay_pallet[0]));
  linear_paint_tile_render(soil, TEXDIM, TEXDIM,
                           4, 4, soil_pallet,
                           sizeof(soil_pallet) / sizeof(soil_pallet[0]));

  soil_verts[0] = 0;
  soil_verts[1] = 85 * MILLIMETRE*10;
  soil_verts[2] = 0;

  for (i = 0; i < ROTRES; ++i) {
    j = (i+1) % ROTRES;
    theta = i * (65536 / ROTRES);
    h = (i? i-1 : ROTRES-1);

    cossinms(pot_verts + i*6+0, pot_verts + i*6+2, theta,
             55 * MILLIMETRE*10);
    pot_verts[i*6+1] = 30 * MILLIMETRE*10;
    pot_verts[i*6+3] = pot_verts[i*6+0];
    pot_verts[i*6+4] = 100 * MILLIMETRE*10;
    pot_verts[i*6+5] = pot_verts[i*6+2];
    soil_verts[3 + i*3+0] = pot_verts[i*6+0] * 250 / 256;
    soil_verts[3 + i*3+1] = 85 * MILLIMETRE*10;
    soil_verts[3 + i*3+2] = pot_verts[i*6+2] * 250 / 256;

    pot_mesh[i*4 + 0].vert[0] = i*2+0;
    pot_mesh[i*4 + 0].vert[1] = j*2+0;
    pot_mesh[i*4 + 0].vert[2] = i*2+1;
    pot_mesh[i*4 + 1].vert[0] = j*2+0;
    pot_mesh[i*4 + 1].vert[1] = j*2+1;
    pot_mesh[i*4 + 1].vert[2] = i*2+1;
    pot_mesh[i*4 + 2].vert[0] = i*2+1;
    pot_mesh[i*4 + 2].vert[1] = j*2+1;
    pot_mesh[i*4 + 2].vert[2] = j*2+0;
    pot_mesh[i*4 + 3].vert[0] = i*2+1;
    pot_mesh[i*4 + 3].vert[1] = j*2+0;
    pot_mesh[i*4 + 3].vert[2] = i*2+0;

    pot_mesh[i*4 + 0].adj[0] = i*4 + 3;
    pot_mesh[i*4 + 0].adj[1] = i*4 + 1;
    pot_mesh[i*4 + 0].adj[2] = h*4 + 1;
    pot_mesh[i*4 + 1].adj[0] = j*4 + 0;
    pot_mesh[i*4 + 1].adj[1] = i*4 + 2;
    pot_mesh[i*4 + 1].adj[2] = i*4 + 0;
    pot_mesh[i*4 + 2].adj[0] = i*4 + 1;
    pot_mesh[i*4 + 2].adj[1] = j*4 + 3;
    pot_mesh[i*4 + 2].adj[2] = i*4 + 3;
    pot_mesh[i*4 + 3].adj[0] = i*4 + 2;
    pot_mesh[i*4 + 3].adj[1] = i*4 + 0;
    pot_mesh[i*4 + 3].adj[2] = h*4 + 2;

    soil_mesh[i].vert[0] = 0;
    soil_mesh[i].vert[1] = j+1;
    soil_mesh[i].vert[2] = i+1;
    soil_mesh[i].adj[0] = j;
    soil_mesh[i].adj[1] = ~0;
    soil_mesh[i].adj[2] = h;
  }
}

static void flower_pot_draw(flower_pot_state* this, canvas* dst) {
  hull_render_scratch hrscratch[sizeof(pot_mesh)/sizeof(pot_mesh[0])];
  pencil_spec   pencil;
  dm_proj       brush_proj;
  fast_brush_accum fbaccum;
  perspective   proj;
  tiled_texture pot_texture, soil_texture;
  vc3           va;
  unsigned      size;
  turtle_state  turtle[128];
  lsystem_state system;
  const char*   system_in;
  unsigned      turtleix = 0;
  drawing_queue_burst burst;

  /* Configure drawing utinsils */
  perspective_init(&proj, dst, DEG_90);
  proj.camera[0] = METRE + zo_cosms(this->rotation, 1000 * MILLIMETRE*10);
  proj.camera[1] = 500 * MILLIMETRE*10;
  proj.camera[2] = METRE + zo_sinms(this->rotation, 1000 * MILLIMETRE*10);
  proj.torus_w = proj.torus_h = 1024 * METRE;
  proj.yrot_cos = zo_cos(65536 - (this->rotation - DEG_90));
  proj.yrot_sin = zo_sin(65536 - (this->rotation - DEG_90));
  proj.rxrot_cos = zo_cos(15 * 65536 / 360);
  proj.rxrot_sin = zo_sin(15 * 65536 / 360);
  proj.near_clipping_plane = 1;

  pencil_init(&pencil);
  pencil.colour = 0;
  pencil.thickness *= 2;

  dm_init(&brush_proj);
  brush_proj.proj = &proj;
  brush_proj.near_clipping = 1;
  brush_proj.near_max = 1;
  brush_proj.far_max = 1000*MILLIMETRE * 10;
  brush_proj.far_clipping = 100 * METRE;

  pot_texture.texture = clay;
  pot_texture.w_mask = TEXDIM-1;
  pot_texture.h_mask = TEXDIM-1;
  pot_texture.pitch = TEXDIM;
  pot_texture.x_off = 0;
  pot_texture.y_off = 0;
  pot_texture.rot_cos = ZO_SCALING_FACTOR_MAX;
  pot_texture.rot_sin = 0;
  pot_texture.nominal_resolution = 1024;
  soil_texture.texture = soil;
  soil_texture.w_mask = TEXDIM-1;
  soil_texture.h_mask = TEXDIM-1;
  soil_texture.pitch = TEXDIM;
  soil_texture.x_off = 0;
  soil_texture.y_off = 0;
  soil_texture.rot_cos = ZO_SCALING_FACTOR_MAX;
  soil_texture.rot_sin = 0;
  soil_texture.nominal_resolution = 1024;

  lsystem_execute(&system, &flower_system, "8.8P", 8, time(NULL)/4);

  /* Draw background */
  parchment_draw(dst, this->bg);
  ump_join();

  /* Draw pot and soil */
  hull_render(dst, hrscratch,
              pot_mesh, lenof(pot_mesh),
              pot_verts, 3,
              METRE, 0, METRE, 0,
              tiled_texture_fill_a,
              &pot_texture,
              &proj);
  hull_outline(dst, hrscratch,
               pot_mesh, lenof(pot_mesh),
               pot_verts, 3,
               METRE, 0, METRE, 0,
               (drawing_method*)&pencil, dst,
               &proj);
  hull_render(dst, hrscratch,
              soil_mesh, lenof(soil_mesh),
              soil_verts, 3,
              METRE, 0, METRE, 0,
              tiled_texture_fill_a,
              &soil_texture,
              &proj);

  va[0] = METRE;
  va[1] = STEM_BASE*MILLIMETRE * 10;
  va[2] = METRE;
  size = dm_proj_calc_weight(dst->logical_width, &proj,
                             brush_proj.far_max, MILLIMETRE * 10 * 10);
  fbaccum.colours = plant_colours;
  fbaccum.num_colours = lenof(plant_colours);
  fbaccum.distance = 0;
  fbaccum.random = fbaccum.random_seed = 0;
  fbaccum.dst = dst;
  drawing_queue_clear(this->queue);
  drawing_queue_start_burst(&burst, this->queue);
  DQMETHPTR(burst, this->stem_brush);
  DQACC(burst, fbaccum);

  turtle_init(&turtle[0], &proj, va, FLOWER_UNIT);
  for (system_in = system.buffer; *system_in; ++system_in) {
    switch (*system_in) {
    case '0':
    case '1':
    case '2':
    case '3':
    case '4':
    case '5':
    case '6':
    case '7':
    case '8':
    case '9':
      turtle_move(turtle+turtleix, 0,
                  STEM_H*MILLIMETRE*10/FLOWER_UNIT >> ('9' - *system_in), 0);
      turtle_put_points(&burst, turtle+turtleix,
                        size >> turtleix, size >> turtleix);
      /* Lie about the width, since it's always on the screen in this demo
       * anyway.
       */
      drawing_queue_draw_line(&burst, 1000);
      break;

    case 'b':
      turtle_rotate_z(turtle+turtleix, 65536 / 6);
      break;

    case 'r':
      turtle_rotate_y(turtle+turtleix, 65536 / 6);
      break;

    case 'P':
    case 'p':
      drawing_queue_flush(&burst);
      fbaccum.colours = petal_colours;
      fbaccum.num_colours = lenof(petal_colours);
      fbaccum.random_seed = fbaccum.random = system_in - system.buffer;
      DQACC(burst, fbaccum);
      turtle_put_point(&burst, turtle+turtleix,
                       dm_proj_calc_weight(dst->logical_width, &proj,
                                           brush_proj.far_max,
                                           5*10*MILLIMETRE/2 * 10));
      drawing_queue_draw_point(&burst, 1000);
      drawing_queue_flush(&burst);

      fbaccum.colours = plant_colours;
      fbaccum.num_colours = lenof(plant_colours);
      fbaccum.random_seed = fbaccum.random = 0;
      DQACC(burst, fbaccum);
      break;

    case '[':
      memcpy(turtle+turtleix+1, turtle+turtleix, sizeof(turtle_state));
      ++turtleix;
      turtle_scale_down(turtle+turtleix, 1);
      turtle_rotate_z(turtle+turtleix, -65536 / 16);
      break;

    case ']':
      drawing_queue_flush(&burst);
      --turtleix;
      break;

    case '.': break;
    case 'B': break;
    default: abort();
    }
  }

  drawing_queue_end_burst(this->queue, &burst);
  drawing_queue_execute(dst, this->queue, 0, 0);
}
