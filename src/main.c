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

#include "bsd.h"

#include "graphics/canvas.h"
#include "graphics/parchment.h"
#include "graphics/pencil.h"

static void draw_stuff(canvas*);

int main(void) {
  unsigned ww, wh;
  SDL_Window* screen;
  SDL_Renderer* renderer;
  SDL_Texture* texture;
  const int image_types = IMG_INIT_JPG | IMG_INIT_PNG;
  parchment* parch;
  canvas* canv;

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
  draw_stuff(canv);
  canvas_blit(texture, canv);
  SDL_RenderCopy(renderer, texture, NULL, NULL);
  SDL_RenderPresent(renderer);
  SDL_Delay(5000);

  return 0;
}

static void draw_stuff(canvas* dst) {
  pencil_spec pencil;
  vo3 a, b;

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
}
