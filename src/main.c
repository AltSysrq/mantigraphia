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

/* Need to define _GNU_SOURCE on GNU systems to get RTLD_NEXT. BSD gives it to
 * us for free.
 */
#ifndef RTLD_NEXT
#define _GNU_SOURCE
#endif

#include <SDL.h>
#include <SDL_image.h>

#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <time.h>

#ifdef HAVE_DLFCN_H
#include <dlfcn.h>
#endif

#if defined(RTLD_NEXT) && defined(HAVE_DLFCN_H) && defined(HAVE_DLFUNC)
#define ENABLE_SDL_ZAPHOD_MODE_FIX
#endif

#include "bsd.h"

#include "graphics/canvas.h"
#include "graphics/parchment.h"
#include "graphics/brush.h"
#include "control/mouselook.h"
#include "render/terrabuff.h"
#include "game-state.h"
#include "cosine-world.h"
#include "micromp.h"

static game_state* update(game_state*);
static void draw(canvas*, game_state*, SDL_Texture*, SDL_Renderer*);
static int handle_input(game_state*);

#ifdef ENABLE_SDL_ZAPHOD_MODE_FIX
/* On X11 with "Zaphod mode" multiple displays, all displays have a position of
 * (0,0), so SDL always uses the same screen regardless of $DISPLAY. Work
 * around this overriding the internal (!!) function it uses to select display
 * screens, and providing the correct response for Zaphod mode ourselves. If
 * not in Zaphod mode, delegate to the original function.
 */
int SDL_GetWindowDisplayIndex(SDL_Window* window) {
  const char* display_name;
  /* We can consider this thread-safe. SDL only allows window manipulation by
   * the thread that creates the window, and we only create one window. (And if
   * we were to create more, they'd also be owned by the main thread anyway.)
   *
   * It is worth caching this, since SDL calls this function at least several
   * times per second. On most systems, getenv() walks a linear list that can
   * be quite large, and dlfunc() needs to walk a DAG of libraries.
   */
  static int screen = -1;
  static int do_delegate = 0;
  static int (*delegate)(SDL_Window*);
  static int delegate_initialised;

  if (screen < 0 && !do_delegate) {
    display_name = getenv("DISPLAY");

    if (display_name &&
        1 == sscanf(display_name,
                    (display_name[0] == ':'? ":%*d.%d" : "%*64s:%*d.%d"),
                    &screen) &&
        screen > 0)
      do_delegate = 0;
    else
      do_delegate = 1;
  }

  if (!do_delegate)
    return screen;

  /* Not on Zaphod mode, delegate to original */
  if (!delegate_initialised) {
    delegate =
      (int(*)(SDL_Window*))dlfunc(RTLD_NEXT, "SDL_GetWindowDisplayIndex");
    delegate_initialised = 1;

    if (!delegate) {
      warnx("Unable to delegate to SDL to choose display, assuming zero. %s",
#ifdef HAVE_DLERROR
            dlerror()
#else
            "(reason unknown because dlerror() unavailable)"
#endif
        );
    }
  }

  if (delegate)
    return (*delegate)(window);
  else
    return 0;
}
#endif

int main(void) {
  unsigned ww, wh;
  SDL_Window* screen;
  SDL_Renderer* renderer;
  SDL_Texture* texture;
  const int image_types = IMG_INIT_JPG | IMG_INIT_PNG;
  canvas* canv;
  game_state* state;

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

  ump_init(SDL_GetCPUCount()-1);
  parchment_init();
  brush_load();
  mouselook_init(screen);
  terrabuff_init();

  canv = canvas_new(ww, wh);

  texture = SDL_CreateTexture(renderer,
                              /* TODO: Use native format */
                              SDL_PIXELFORMAT_ARGB8888,
                              SDL_TEXTUREACCESS_STREAMING,
                              ww, wh);
  if (!texture)
    errx(EX_UNAVAILABLE, "Unable to create screen texture: %s", SDL_GetError());

  state = cosine_world_new();

  do {
    draw(canv, state, texture, renderer);
    if (handle_input(state)) break; /* quit */
    state = update(state);
  } while (state);

  return 0;
}

static game_state* update(game_state* state) {
  static chronon prev;
  chronon now, elapsed;
  unsigned long long now_tmp;

  /* Busy-wait for the time to roll to the next chronon. Sleep for 1ms between
   * attempts so we don't use the *whole* CPU doing nothing.
   */
  do {
    /* Get current time in chronons */
    now_tmp = SDL_GetTicks();
    now_tmp *= SECOND;
    now = now_tmp / 1000;
    if (now == prev) SDL_Delay(1);
  } while (now == prev);

  elapsed = now - prev;
  prev = now;

  return (*state->update)(state, elapsed);
}

static void draw(canvas* canv, game_state* state,
                 SDL_Texture* texture, SDL_Renderer* renderer) {
  unsigned draw_start, draw_end;

  canvas_clear(canv);
  draw_start = SDL_GetTicks();
  (*state->draw)(state, canv);
  draw_end = SDL_GetTicks();

  printf("Drawing took %3d ms (%3d FPS)\n", draw_end-draw_start,
         1000 / (draw_end-draw_start));

  canvas_blit(texture, canv);
  SDL_RenderCopy(renderer, texture, NULL, NULL);
  SDL_RenderPresent(renderer);
}

static int handle_input(game_state* state) {
  SDL_Event evt;

  while (SDL_PollEvent(&evt)) {
    switch (evt.type) {
    case SDL_QUIT: return 1;
    case SDL_KEYDOWN:
    case SDL_KEYUP:
      if (state->key)
        (*state->key)(state, &evt.key);
      break;

    case SDL_MOUSEMOTION:
      if (state->mmotion)
        (*state->mmotion)(state, &evt.motion);
      break;

    case SDL_MOUSEBUTTONDOWN:
    case SDL_MOUSEBUTTONUP:
      if (state->mbutton)
        (*state->mbutton)(state, &evt.button);
      break;

    case SDL_MOUSEWHEEL:
      if (state->scroll)
        (*state->scroll)(state, &evt.wheel);
      break;

    case SDL_TEXTEDITING:
      if (state->txted)
        (*state->txted)(state, &evt.edit);
      break;

    case SDL_TEXTINPUT:
      if (state->txtin)
        (*state->txtin)(state, &evt.text);
      break;

    default: /* ignore */ break;
    }
  }

  return 0; /* continue running */
}
