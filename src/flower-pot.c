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

#include "alloc.h"
#include "coords.h"
#include "flower-pot.h"

typedef struct {
  game_state vtab;
  int is_running;
} flower_pot_state;

static game_state* flower_pot_update(flower_pot_state*, chronon);
static void flower_pot_draw(flower_pot_state*, canvas*);
static void flower_pot_key(flower_pot_state*, SDL_KeyboardEvent*);
static void flower_pot_mbutton(flower_pot_state*, SDL_MouseButtonEvent*);
static void flower_pot_mmotion(flower_pot_state*, SDL_MouseMotionEvent*);
static void flower_pot_scroll(flower_pot_state*, SDL_MouseWheelEvent*);

game_state* flower_pot_new(void) {
  flower_pot_state template = {
    { (game_state_update_t) flower_pot_update,
      (game_state_draw_t)   flower_pot_draw,
      (game_state_key_t)    flower_pot_key,
      (game_state_mbutton_t)flower_pot_mbutton,
      (game_state_mmotion_t)flower_pot_mmotion,
      (game_state_scroll_t) flower_pot_scroll,
      0, 0,
    },
    1,
  };
  flower_pot_state* this = xmalloc(sizeof(flower_pot_state));
  memcpy(this, &template, sizeof(flower_pot_state));
  return (game_state*)this;
}

static game_state* flower_pot_update(flower_pot_state* this, chronon et) {
  if (this->is_running) {
    return (game_state*)this;
  } else {
    free(this);
    return NULL;
  }
}

static void flower_pot_draw(flower_pot_state* this, canvas* dst) {
  /* TODO */
}

static void flower_pot_key(flower_pot_state* this, SDL_KeyboardEvent* evt) {
  printf("key: %s scancode %d (%s), keycode %d (%s)\n",
         (SDL_KEYDOWN == evt->type? "down" : "up"),
         (int)evt->keysym.scancode, SDL_GetScancodeName(evt->keysym.scancode),
         (int)evt->keysym.sym, SDL_GetKeyName(evt->keysym.sym));

  if (SDL_KEYDOWN == evt->type && SDLK_ESCAPE == evt->keysym.sym)
    this->is_running = 0;
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
