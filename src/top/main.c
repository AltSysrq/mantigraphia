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

/* Need to define _GNU_SOURCE on GNU systems to get RTLD_NEXT. BSD gives it to
 * us for free.
 */
#ifndef RTLD_NEXT
#define _GNU_SOURCE
#endif

#include <SDL.h>
#include <SDL_image.h>
#include <glew.h>

#include <assert.h>
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
#include "gl/glinfo.h"
#include "gl/marshal.h"
#include "gl/auxbuff.h"
#include "control/mouselook.h"
#include "render/terrabuff.h"
#include "game-state.h"
#include "cosine-world.h"
#include "micromp.h"

static game_state* update(game_state*);
static void draw(canvas*, game_state*, SDL_Window*);
static int handle_input(game_state*);

static void start_render_thread(void);
static int render_thread_main(void*);
static void invoke_draw_on_render_thread(canvas*, game_state*);

/* Whether the multi-display configuration might be a Zaphod configuration. If
 * this is true, we need to try to force SDL to respect the environment and
 * choose the correct display. Otherwise, allow SDL to decide on its own.
 *
 * (We can't just assume that the format :%d.%d indicates Zaphod, as Xinerama
 * displays often use :0.0 for the assembly of all the screens.)
 */
static int might_be_zaphod = 1;

static int parse_x11_screen(signed* screen, const char* display) {
  if (!display) return 0;

  if (':' == display[0])
    return 1 == sscanf(display, ":%*d.%d", screen);
  else
    return 1 == sscanf(display, "%*64s:%*d.%d", screen);
}

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

    if (parse_x11_screen(&screen, display_name) && screen >= 0)
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

void select_window_bounds(SDL_Rect* window_bounds) {
  SDL_Rect display_bounds;
  int i, n, zaphod_screen;
  unsigned largest_index, largest_width;

  n = SDL_GetNumVideoDisplays();
  if (n <= 0) {
    warnx("Unable to determine number of video displays: %s",
          SDL_GetError());
    goto use_conservative_boundaries;
  }

  /* Generally prefer the display with the greatest width */
  largest_width = 0;
  largest_index = n;
  for (i = 0; i < n; ++i) {
    if (SDL_GetDisplayBounds(i, &display_bounds)) {
      warnx("Unable to determine bounds of display %d: %s",
            i, SDL_GetError());
    } else {
      /* A Zaphod configuration has all displays at (0,0) */
      might_be_zaphod &= (0 == display_bounds.x && 0 == display_bounds.y);

      if (display_bounds.w > (signed)largest_width) {
        largest_width = display_bounds.w;
        largest_index = i;
      }
    }
  }

  if (largest_index >= (unsigned)n) {
    warnx("Failed to query the bounds of any display...");
    goto use_conservative_boundaries;
  }

  /* If still a candidate for zaphod mode, and $DISPLAY indicates a particular
   * screen, force that screen. Otherwise, use the one we selected above.
   */
  if (might_be_zaphod && parse_x11_screen(&zaphod_screen, getenv("DISPLAY")) &&
      zaphod_screen >= 0 && zaphod_screen < n) {
    SDL_GetDisplayBounds(zaphod_screen, window_bounds);
  } else {
    SDL_GetDisplayBounds(largest_index, window_bounds);
  }
  return;

  use_conservative_boundaries:
  window_bounds->x = SDL_WINDOWPOS_UNDEFINED;
  window_bounds->y = SDL_WINDOWPOS_UNDEFINED;
  window_bounds->w = 640;
  window_bounds->h = 480;
}

int main(int argc, char** argv) {
  unsigned ww, wh;
  SDL_Window* screen;
  SDL_GLContext glcontext;
  const int image_types = IMG_INIT_JPG | IMG_INIT_PNG;
  canvas canv;
  game_state* state;
  GLenum glew_status;
  SDL_Rect window_bounds;
  unsigned last_fps_report, frames_since_fps_report;

  if (SDL_Init(SDL_INIT_VIDEO | SDL_INIT_AUDIO))
    errx(EX_SOFTWARE, "Unable to initialise SDL: %s", SDL_GetError());

  atexit(SDL_Quit);

  SDL_GL_SetAttribute(SDL_GL_CONTEXT_PROFILE_MASK,
                      SDL_GL_CONTEXT_PROFILE_CORE);
  SDL_GL_SetAttribute(SDL_GL_CONTEXT_MAJOR_VERSION, 3);
  SDL_GL_SetAttribute(SDL_GL_CONTEXT_MINOR_VERSION, 0);

  select_window_bounds(&window_bounds);
  screen = SDL_CreateWindow("Mantigraphia",
                            window_bounds.x,
                            window_bounds.y,
                            window_bounds.w,
                            window_bounds.h,
                            SDL_WINDOW_OPENGL);
  if (!screen)
    errx(EX_OSERR, "Unable to create window: %s", SDL_GetError());

  if (image_types != (image_types & IMG_Init(image_types)))
    errx(EX_SOFTWARE, "Unable to init SDLIMG: %s", IMG_GetError());

  screen_pixel_format = SDL_AllocFormat(SDL_PIXELFORMAT_ARGB8888);
  if (!screen_pixel_format)
    errx(EX_UNAVAILABLE, "Unable to get ARGB8888 pixel format: %s",
         SDL_GetError());

  SDL_GetWindowSize(screen, (int*)&ww, (int*)&wh);
  glcontext = SDL_GL_CreateContext(screen);
  if (!glcontext)
    errx(EX_OSERR, "Unable to initialise OpenGL context: %s",
         SDL_GetError());

  glew_status = glewInit();
  if (GLEW_OK != glew_status)
    errx(EX_SOFTWARE, "Unable to initialise GLEW: %s",
         glewGetErrorString(glew_status));

  glEnable(GL_DEPTH_TEST);
  glEnable(GL_TEXTURE_2D);
  glEnableClientState(GL_VERTEX_ARRAY);
  canvas_init_thin(&canv, ww, wh);
  canvas_gl_clip_sub_immediate(&canv, &canv);

  ump_init(SDL_GetCPUCount()-1);
  glinfo_detect(wh);
  glm_init();
  auxbuff_init(ww, wh);
  parchment_init();
  mouselook_init(screen);
  terrabuff_init();
  start_render_thread();

  state = cosine_world_new(2 == argc? atoi(argv[1]) : 3);

  last_fps_report = SDL_GetTicks();
  frames_since_fps_report = 0;
  do {
    draw(&canv, state, screen);
    if (handle_input(state)) break; /* quit */
    state = update(state);

    ++frames_since_fps_report;
    if (SDL_GetTicks() - last_fps_report >= 3000) {
      printf("FPS: %d\n", frames_since_fps_report/3);
      frames_since_fps_report = 0;
      last_fps_report = SDL_GetTicks();
    }
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
                 SDL_Window* screen) {
  (*state->predraw)(state, canv);
  invoke_draw_on_render_thread(canv, state);
  /* Process OpenGL commands until the rendering thread calls glm_done() and
   * all the GL work itself is complete.
   */
  glm_main();
  SDL_GL_SwapWindow(screen);
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

static SDL_Thread* render_thread;
static SDL_cond* render_thread_cond;
static SDL_mutex* render_thread_lock;
/* Nulled when a rendering pass completes */
static canvas* render_thread_target_canvas;
static game_state* render_thread_game_state;

static void start_render_thread(void) {
  render_thread_cond = SDL_CreateCond();
  if (!render_thread_cond)
    errx(EX_SOFTWARE, "Unable to create rendering condition: %s",
         SDL_GetError());
  render_thread_lock = SDL_CreateMutex();
  if (!render_thread_lock)
    errx(EX_SOFTWARE, "Unable to create rendering lock: %s",
         SDL_GetError());

  render_thread = SDL_CreateThread(render_thread_main, "rendering", NULL);
  if (!render_thread)
    errx(EX_SOFTWARE, "Unable to create rendering thread: %s",
         SDL_GetError());
  /* SDL_DetachThread is not present in SDL 2.0.2, which is still the current
   * version in Debian Sid (as of 2015-03-30).
   *
   * Not detaching merely means that the exit status will be leaked when the
   * thread exits, which it never does, and that it cannot be awaited, which we
   * never do, so just don't call it for now.
  SDL_DetachThread(render_thread);
   */
}

static int render_thread_main(void* ignored) {
  canvas* canv;
  game_state* state;

  for (;;) {
    if (SDL_LockMutex(render_thread_lock))
      errx(EX_SOFTWARE, "Failed to acquire rendering lock: %s",
           SDL_GetError());

    while (!render_thread_target_canvas) {
      if (SDL_CondWait(render_thread_cond, render_thread_lock))
        errx(EX_SOFTWARE, "Failed to wait on rendering condition: %s",
             SDL_GetError());
    }

    canv = render_thread_target_canvas;
    state = render_thread_game_state;
    render_thread_target_canvas = NULL;
    render_thread_game_state = NULL;
    if (SDL_UnlockMutex(render_thread_lock))
      errx(EX_SOFTWARE, "Failed to release rendering lock: %s",
           SDL_GetError());

    (*state->draw)(state, canv);
    /* Assume that any threads involved in drawing have already called
     * glm_finish_thread(). While we *could* try to do that here, the fact that
     * a thread might not have run (ie, due to impersonation) complicates
     * matters a bit, and it also means that we'd have to wait for *all*
     * threads to run the task, which as described in uMP, would cause
     * occasional but substantial delays.
     */
    glm_done();
  }

  /* unreachable */
  return 0;
}

static void invoke_draw_on_render_thread(canvas* canv, game_state* state) {
  if (SDL_LockMutex(render_thread_lock))
    errx(EX_SOFTWARE, "Failed to acquire rendering lock: %s",
         SDL_GetError());

  assert(!render_thread_target_canvas);
  assert(!render_thread_game_state);
  render_thread_target_canvas = canv;
  render_thread_game_state = state;

  if (SDL_CondSignal(render_thread_cond))
    errx(EX_SOFTWARE, "Failed to signal rendering condition: %s",
         SDL_GetError());

  if (SDL_UnlockMutex(render_thread_lock))
    errx(EX_SOFTWARE, "Failed to release rendering lock: %s",
         SDL_GetError());
}
