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

#include <SDL.h>
#include <SDL_image.h>

#include <string.h>
#include <glew.h>

#include "../bsd.h"

#include "../alloc.h"
#include "../micromp.h"
#include "../gl/marshal.h"
#include "../gl/shaders.h"
#include "canvas.h"
#include "parchment.h"

#define PARCHMENT_DIM 2048
#define PARCHMENT_MASK (PARCHMENT_DIM-1)
#define PARCHMENT_FILE "share/extern/parchment.jpg"

static GLuint texture;
static glm_slab_group* glmsg;
static void parchment_activate(void*);

struct parchment_s {
  unsigned tx, ty;
};

void parchment_init(void) {
  SDL_Surface* orig_surf, * new_surf;
  canvas canv;

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

  /* new_surf is now the same format as a canvas, so we can directly point to
   * the data inside the surface.
   */
  canv.w = PARCHMENT_DIM;
  canv.h = PARCHMENT_DIM;
  canv.pitch = PARCHMENT_DIM;
  canv.logical_width = PARCHMENT_DIM;
  canv.ox = canv.oy = 0;
  canv.px = (canvas_pixel*)new_surf->pixels;
  canv.depth = NULL;

  texture = canvas_to_texture(&canv, 0);
  glBindTexture(GL_TEXTURE_2D, texture);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT);

  SDL_FreeSurface(new_surf);
  SDL_FreeSurface(orig_surf);

  glmsg = glm_slab_group_new(parchment_activate, NULL, NULL,
                             shader_fixed_texture_configure_vbo,
                             sizeof(shader_fixed_texture_vertex));
}

parchment* parchment_new(void) {
  parchment* this = xmalloc(sizeof(parchment));
  this->tx = this->ty = 0;
  return this;
}

void parchment_delete(parchment* this) {
  free(this);
}

static void parchment_activate(void* ignore) {
  glEnable(GL_TEXTURE_2D);
  glBindTexture(GL_TEXTURE_2D, texture);
  shader_fixed_texture_activate(NULL);
}

void parchment_draw(canvas* dst, const parchment* this) {
  shader_fixed_texture_vertex* vertices;
  unsigned short* indices, base;
  coord_offset x, y, x0, y0;
  glm_slab* slab = glm_slab_get(glmsg);

  x0 = this->tx / 1024 & PARCHMENT_MASK;
  if (x0 > 0) x0 -= PARCHMENT_DIM;
  y0 = this->ty / 1024 & PARCHMENT_MASK;
  if (y0 > 0) y0 -= PARCHMENT_DIM;

  for (y = y0; y < (signed)dst->h; y += PARCHMENT_DIM) {
    for (x = x0; x < (signed)dst->w; x += PARCHMENT_DIM) {
      base = GLM_ALLOC(&vertices, &indices, slab, 4, 6);
      vertices[0].v[0] = x;
      vertices[0].v[1] = y;
      vertices[0].v[2] = 4095*METRE;
      vertices[1].v[0] = x + PARCHMENT_DIM;
      vertices[1].v[1] = y;
      vertices[1].v[2] = 4095*METRE;
      vertices[2].v[0] = x;
      vertices[2].v[1] = y + PARCHMENT_DIM;
      vertices[2].v[2] = 4095*METRE;
      vertices[3].v[0] = x + PARCHMENT_DIM;
      vertices[3].v[1] = y + PARCHMENT_DIM;
      vertices[3].v[2] = 4095*METRE;
      vertices[0].tc[0] = 0;
      vertices[0].tc[1] = 0;
      vertices[1].tc[0] = 1;
      vertices[1].tc[1] = 0;
      vertices[2].tc[0] = 0;
      vertices[2].tc[1] = 1;
      vertices[3].tc[0] = 1;
      vertices[3].tc[1] = 1;
      indices[0] = base + 0;
      indices[1] = base + 1;
      indices[2] = base + 2;
      indices[3] = base + 1;
      indices[4] = base + 2;
      indices[5] = base + 3;
    }
  }

  glm_finish_thread();
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
  this->ty -= screen_w * delta_pitch * 314159LL / fov_y * 1024 / 200000;
  /* Shift horizontally as per change in yaw, but scale this shift by
   * cos(pitch), since looking vertically has less an effect on perceived
   * rotation.
   */
  this->tx += screen_w * delta_yaw * 314159LL / fov_x * 1024 / 200000;
}
