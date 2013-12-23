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
#ifndef CONTROL_MOUSELOOK_H_
#define CONTROL_MOUSELOOK_H_

#include <SDL.h>

#include "../coords.h"
#include "../graphics/parchment.h"

/**
 * Defines the current state of mouselook in some context. The fields within
 * are to be initialised by the creator.
 */
typedef struct {
  /**
   * The absolute rotation on the Y axis.
   */
  angle yrot;
  /**
   * The relative rotation on the X axis. Ranges from -DEG_90 to +DEG_90.
   */
  angle rxrot;
} mouselook_state;

/**
 * Initialises mouselook support on the given window. Only one window is
 * supported at a time.
 *
 * This assumes that mouselook is currently disabled, and does not attempt to
 * change parameters on the window during this call.
 */
void mouselook_init(SDL_Window*);

/**
 * Returns whether mouselook is currently enabled on the main window.
 */
int mouselook_get(void);
/**
 * Sets whether mouselook is currently enabled on the main window.
 *
 * Depending on the underlying system and configuration, there are a couple
 * ways this may function. Ideally, SDL_SetRelativeMouseMode() is
 * used. However, since it does not work universally, this is accomplished
 * SDL1.2-style by explicitly hiding the cursor and manually warping it to the
 * centre of the window on every movement.
 *
 * If mouselook is enabled, it is the controller's responsibility to pass every
 * SDL_MouseMotionEvent that occurs to mouselook_update().
 */
void mouselook_set(int);

/**
 * Informs the mouselook system that the given mouse motion event has
 * occurred. If mouselook is enabled, the angles in the mouselook_state are
 * updated appropriately, and the background is adjusted if not NULL.
 */
void mouselook_update(mouselook_state*,
                      parchment* bg, const SDL_MouseMotionEvent*,
                      angle fov_x, angle fov_y);

#endif /* CONTROL_MOUSELOOK_H_ */
