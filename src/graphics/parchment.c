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
#include <SDL_image.h>

#include <string.h>

#include "../bsd.h"

#include "../alloc.h"
#include "canvas.h"
#include "parchment.h"

#define PARCHMENT_DIM 2048
#define PARCHMENT_MASK (PARCHMENT_DIM-1)
#define PARCHMENT_FILE "share/extern/canvas.jpg"

static canvas_pixel texture[PARCHMENT_DIM*PARCHMENT_DIM];

struct parchment_s {
  unsigned tx, ty;
};

void parchment_init(void) {
  SDL_Surface* orig_surf, * new_surf;

  orig_surf = IMG_Load(PARCHMENT_FILE);
  if (!orig_surf)
    errx(EX_DATAERR, "File " PARCHMENT_FILE " is could not be loaded: %s",
         IMG_GetError());

  if (PARCHMENT_DIM != orig_surf->w || PARCHMENT_DIM != orig_surf->h)
    errx(EX_DATAERR, "File " PARCHMENT_FILE " is of the wrong dimensions.");

  new_surf = SDL_ConvertSurface(orig_surf, screen_pixel_format, 0);
  if (!new_surf)
    errx(EX_SOFTWARE, "Could not convert parchment surface: %s",
         SDL_GetError());

  /* new_surf is now the same format as texture[], so we can directly copy the
   * data over.
   */
  memcpy(texture, new_surf->pixels, sizeof(texture));

  SDL_FreeSurface(new_surf);
  SDL_FreeSurface(orig_surf);
}

parchment* parchment_new(void) {
  parchment* this = xmalloc(sizeof(parchment));
  this->tx = this->ty = 0;
  return this;
}

void parchment_delete(parchment* this) {
  free(this);
}

void parchment_draw(canvas* dst, const parchment* this) {
  unsigned y, yo, xo, cnt, off;
#pragma omp parallel for private(y,yo,xo,cnt,off)
  for (yo = 0; yo < dst->h; ++yo) {
    y = (yo + this->ty/1024) & PARCHMENT_MASK;

    for (xo = 0; xo < dst->w; xo += cnt) {
      cnt = PARCHMENT_DIM - ((xo + this->tx/1024) & PARCHMENT_MASK);
      if (xo + cnt > dst->w)
        cnt = dst->w - xo;

      off = ((xo + this->tx/1024) & PARCHMENT_MASK) + y * PARCHMENT_DIM;

      memcpy(dst->px + canvas_offset(dst, xo, yo),
             texture + off,
             sizeof(canvas_pixel) * cnt);
    }
  }
}

void parchment_xform(parchment* this,
                     angle old_yaw,
                     angle old_pitch,
                     angle new_yaw,
                     angle new_pitch,
                     angle fov_x,
                     angle fov_y,
                     coord_offset screen_w,
                     coord_offset screen_h) {
  signed short delta_pitch = new_pitch - old_pitch;
  signed short delta_yaw = new_yaw - old_yaw;
  /* Shift vertically as per change in pitch */
  this->ty += screen_w * delta_pitch * 314159LL / fov_y * 1024 / 200000;
  /* Shift horizontally as per change in yaw, but scale this shift by
   * cos(pitch), since looking vertically has less an effect on perceived
   * rotation.
   */
  this->tx -= screen_w * delta_yaw * 314159LL / fov_x * 1024 / 200000;
}
