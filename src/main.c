/*
  Copyright (c) 2013 Jason Lingle
  All rights reserved.

  Redistribution and use in source and binary forms, with or without
  modification, are permitted provided that the following conditions
  are met:
  1. Redistributions of source code must retain the above copyright
     notice, this list of conditions and the following disclaimer.
  2. Redistributions in binary form must reproduce the above copyright
     notice, this list of conditions and the following disclaimer in the
     documentation and/or other materials provided with the distribution.
  3. Neither the name of the author nor the names of its contributors
     may be used to endorse or promote products derived from this software
     without specific prior written permission.

     THIS SOFTWARE IS PROVIDED BY THE AUTHORS AND CONTRIBUTORS ``AS IS'' AND
     ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
     IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
     ARE DISCLAIMED.  IN NO EVENT SHALL THE AUTHORS OR CONTRIBUTORS BE LIABLE
     FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
     DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
     OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
     HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
     LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
     OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
     SUCH DAMAGE.
*/
#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include <SDL.h>
#include <SDL_image.h>

#include <stdlib.h>
#include <time.h>

#include "bsd.h"

#include "graphics/canvas.h"
#include "graphics/parchment.h"
#include "graphics/pencil.h"
#include "graphics/brush.h"
#include "graphics/tscan.h"
#include "graphics/linear-paint-tile.h"
#include "graphics/tiled-texture.h"

static canvas_pixel water_tex[64*64];
static const canvas_pixel water_pallet[4] = {
  argb(255,0,0,0),
  argb(255,0,0,32),
  argb(255,0,0,48),
  argb(255,8,16,56),
};

static void draw_stuff(canvas*);

int main(void) {
  unsigned ww, wh, i;
  SDL_Window* screen;
  SDL_Renderer* renderer;
  SDL_Texture* texture;
  const int image_types = IMG_INIT_JPG | IMG_INIT_PNG;
  parchment* parch;
  canvas* canv;
  clock_t start, end;

  linear_paint_tile_render(water_tex, 64, 64, 8, 1, water_pallet, 4);

  if (SDL_Init(SDL_INIT_VIDEO | SDL_INIT_AUDIO))
    errx(EX_SOFTWARE, "Unable to initialise SDL: %s", SDL_GetError());

  atexit(SDL_Quit);

  screen = SDL_CreateWindow("Mantigraphia",
                            SDL_WINDOWPOS_UNDEFINED,
                            SDL_WINDOWPOS_UNDEFINED,
                            1280, 1024,
                            0);
  if (!screen)
    errx(EX_OSERR, "Unable to create window: %s", SDL_GetError());

  renderer = SDL_CreateRenderer(screen, -1, 0);

  if (!renderer)
    errx(EX_SOFTWARE, "Unable to create SDL renderer: %s", SDL_GetError());

  if (image_types != (image_types & IMG_Init(image_types)))
    errx(EX_SOFTWARE, "Unable to init SDLIMG: %s", IMG_GetError());

  screen_pixel_format = SDL_AllocFormat(SDL_PIXELFORMAT_ARGB8888);
  if (!screen_pixel_format)
    errx(EX_UNAVAILABLE, "Unable to get ARGB8888 pixel format: %s",
         SDL_GetError());

  SDL_GetWindowSize(screen, (int*)&ww, (int*)&wh);

  parchment_init();
  brush_load();

  canv = canvas_new(ww, wh);
  parch = parchment_new();

  texture = SDL_CreateTexture(renderer,
                              /* TODO: Use native format */
                              SDL_PIXELFORMAT_ARGB8888,
                              SDL_TEXTUREACCESS_STREAMING,
                              ww, wh);
  if (!texture)
    errx(EX_UNAVAILABLE, "Unable to create screen texture: %s", SDL_GetError());

  canvas_clear(canv);
  parchment_draw(canv, parch);
  start = clock();
  for (i = 0; i < 1000; ++i)
    draw_stuff(canv);
  end = clock();
  canvas_blit(texture, canv);
  SDL_RenderCopy(renderer, texture, NULL, NULL);
  SDL_RenderPresent(renderer);
  printf("Rendering took %f sec\n", (end-start)/(double)CLOCKS_PER_SEC/1000.0);
  SDL_Delay(15000);

  return 0;
}

static const canvas_pixel brush_colours[8] = {
  argb(0,0,0,0),
  argb(0,0,0,16),
  argb(0,0,8,32),
  argb(0,8,16,32),
  argb(0,10,20,40),
  argb(0,20,40,60),
  argb(0,30,50,70),
  argb(0,40,60,80),
};

static const canvas_pixel brush_red_colours[8] = {
  argb(0, 32, 0, 0),
  argb(0, 48, 0, 0),
  argb(0, 64, 8, 0),
  argb(0, 80, 16, 8),
  argb(0, 96, 32, 16),
  argb(0, 112, 48, 24),
  argb(0, 128, 64, 32),
  argb(0, 144, 80, 40),
};

static const canvas_pixel brush_green_colours[8] = {
  argb(0, 0, 32, 0),
  argb(0, 8, 48, 0),
  argb(0, 12, 64, 0),
  argb(0, 16, 80, 8),
  argb(0, 32, 96, 16),
  argb(0, 48, 112, 24),
  argb(0, 64, 128, 32),
  argb(0, 80, 144, 40),
};

static const coord_offset
  tb[3] = { 42, 64, 0 },
  tc[3] = { 128, 256, 4 },
  ta[3] = { 1024, 1024, 8 };

static void draw_stuff(canvas* dst) {
  pencil_spec pencil;
  brush_spec brush;
  brush_accum baccum;
  vo3 a, b;
  unsigned i;
  tiled_texture water;

  pencil_init(&pencil);
  pencil.colour = argb(0, 0, 0, 32);
  pencil.thickness = ZO_SCALING_FACTOR_MAX / 64;

  a[0] = 32;
  a[1] = 32;
  a[2] = 1;
  pencil_draw_point(dst, &pencil, a, ZO_SCALING_FACTOR_MAX);

  a[0] = 64;
  a[1] = 64;
  b[0] = 128;
  b[1] = 256;
  b[2] = 1;
  pencil_draw_line(dst, &pencil,
                   a, ZO_SCALING_FACTOR_MAX,
                   b, ZO_SCALING_FACTOR_MAX / 2);

  brush_init(&brush);
  brush.colours = brush_colours;
  brush.num_colours = 8;
  brush.size = ZO_SCALING_FACTOR_MAX / 64;
  brush_prep(&baccum, &brush, dst, 5);

  a[0] = 0;
  a[1] = 0;
  b[0] = 1280;
  b[1] = 1024;
  brush_draw_line(&baccum, &brush,
                  a, ZO_SCALING_FACTOR_MAX,
                  b, ZO_SCALING_FACTOR_MAX);
  brush_flush(&baccum, &brush);

  brush.inner_weakening_chance = 3000;
  brush.inner_strengthening_chance = 3000;
  brush.colours = brush_red_colours;
  for (i = 0; i < 64; ++i) {
    cossinms(a+0, a+1, i * (65536/64), 128);
    a[0] += 256;
    a[1] += 768;
    cossinms(b+0, b+1, (i+1) * (65536/64), 128);
    b[0] += 256;
    b[1] += 768;
    brush_draw_line(&baccum, &brush,
                    a, ZO_SCALING_FACTOR_MAX,
                    b, ZO_SCALING_FACTOR_MAX);
  }
  a[0] = 256 - 48;
  a[1] = 768;
  brush_draw_point(&baccum, &brush, a, ZO_SCALING_FACTOR_MAX);
  brush_flush(&baccum, &brush);
  a[0] = 256 + 48;
  brush_draw_point(&baccum, &brush, a, ZO_SCALING_FACTOR_MAX);

  brush.colours = brush_green_colours;
  brush.inner_strengthening_chance = 3860;
  a[2] = b[2] = 3;
  for (i = 0; i < 1280; i += 5) {
    a[0] = i;
    b[0] = i+5;
    a[1] = zo_cosms(a[0]*256, 128) + 150;
    b[1] = zo_cosms(b[0]*256, 128) + 150;
    brush_draw_line(&baccum, &brush,
                    a, ZO_SCALING_FACTOR_MAX,
                    b, ZO_SCALING_FACTOR_MAX);
  }
  brush_flush(&baccum, &brush);

  water.texture = water_tex;
  water.w_mask = water.h_mask = 63;
  water.pitch = 64;
  water.x_off = water.y_off = 0;
  water.rot_cos = zo_cos(0x2300);
  water.rot_sin = zo_cos(0x2300);
  water.nominal_resolution = 1280;
  tiled_texture_fill(dst, &water, ta, tb, tc);
}
