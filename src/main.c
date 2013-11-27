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

#include <stdlib.h>

#include "bsd.h"

static void draw_stuff(SDL_Renderer*, SDL_Texture*,
                       Uint32*, unsigned, unsigned);

int main(void) {
  unsigned ww, wh, i;
  SDL_Window* screen;
  SDL_Renderer* renderer;
  SDL_Texture* texture;
  Uint32* buff;

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

  SDL_SetRenderDrawColor(renderer, 0, 0, 0, 255);
  SDL_RenderClear(renderer);
  SDL_RenderPresent(renderer);
  SDL_Delay(258);

  SDL_GetWindowSize(screen, (int*)&ww, (int*)&wh);

  texture = SDL_CreateTexture(renderer,
                              /* TODO: Use native format */
                              SDL_PIXELFORMAT_ARGB8888,
                              SDL_TEXTUREACCESS_STREAMING,
                              ww, wh);
  if (!texture)
    errx(EX_UNAVAILABLE, "Unable to create screen texture: %s", SDL_GetError());

  buff = malloc(sizeof(Uint32) * ww * wh);

  for (i = 0; i < 50; ++i) {
    draw_stuff(renderer, texture, buff, ww, wh);
    SDL_Delay(100);
  }

  return 0;
}

static void draw_stuff(SDL_Renderer* renderer,
                       SDL_Texture* texture,
                       Uint32* buff,
                       unsigned ww, unsigned wh) {
  unsigned i, v;

  for (i = 0; i < ww*wh; ++i) {
    v = rand() & 0xFF;
    buff[i] = 0xFF000000 | (v << 16) | (v << 8) | v;
  }

  SDL_UpdateTexture(texture, NULL, buff, ww*sizeof(Uint32));
  SDL_RenderClear(renderer);
  SDL_RenderCopy(renderer, texture, NULL, NULL);
  SDL_RenderPresent(renderer);
}
