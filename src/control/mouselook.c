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

#include "../bsd.h"
#include "../coords.h"
#include "mouselook.h"

static SDL_Window* window;
static int is_enabled, emulate_srmm;

void mouselook_init(SDL_Window* w) {
  window = w;
  is_enabled = 0;
}

int mouselook_get(void) {
  return is_enabled;
}

void mouselook_set(int enabled) {
  static int has_shown_srmm_warning = 0;

  /* Normalise */
  enabled = !!enabled;
  /* If already in this state, nothing to do */
  if (is_enabled == enabled) return;

  is_enabled = enabled;
  if (enabled) {
    SDL_SetWindowGrab(window, SDL_TRUE);
    if (SDL_SetRelativeMouseMode(SDL_TRUE)) {
      if (!has_shown_srmm_warning) {
        warnx("Could not set relative mouse mode: %s",
              SDL_GetError());
        has_shown_srmm_warning = 1;
      }

      /* Explicitly hide the cursor, and remember that we need to emulate SRMM
       * by warping the mouse.
       */
      SDL_SetCursor(SDL_DISABLE);
      emulate_srmm = 1;
    } else {
      emulate_srmm = 0;
    }
  } else {
    if (emulate_srmm) {
      /* Restore cursor */
      SDL_SetCursor(SDL_GetDefaultCursor());
    } else {
      SDL_SetRelativeMouseMode(SDL_FALSE);
    }

    SDL_SetWindowGrab(window, SDL_FALSE);
  }
}

void mouselook_update(mouselook_state* state,
                      parchment* bg,
                      const SDL_MouseMotionEvent* evt) {
  /* Ignore the first event that comes in during the process's lifetime, as it
   * will usually have large relative movements (since the initial position is
   * zero or so, regardless of the actual position).
   */
  static int ignore_next_event = 1;
  signed rxrot = (signed short)state->rxrot;
  int w, h;

  /* Do nothing if this event is to be ignored because it was generated by a
   * call to SDL_WarpMouseInWindow().
   */
  if (ignore_next_event) {
    ignore_next_event = 0;
    SDL_FlushEvent(SDL_MOUSEMOTION);
    return;
  }

  /* Do nothing if not currently enabled */
  if (!is_enabled) return;

  /* TODO: Adjustable sensitivity */
  state->yrot -= evt->xrel * (signed)DEG_180 / 1024;
  rxrot += evt->yrel * (signed)DEG_180 / 1024;
  if (rxrot < -(signed)DEG_90) rxrot = -(signed)DEG_90;
  if (rxrot > +(signed)DEG_90) rxrot = +(signed)DEG_90;
  state->rxrot = rxrot;

  /* TODO: Update parchment */

  /* If necessary, warp cursor.
   * This generates another motion event, which we'll need to ignore on the
   * next call.
   */
  if (emulate_srmm) {
    SDL_GetWindowSize(window, &w, &h);
    SDL_WarpMouseInWindow(window, w/2, h/2);
    ignore_next_event = 1;
  }
}
