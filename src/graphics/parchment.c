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
#include "../gl/auxbuff.h"
#include "canvas.h"
#include "parchment.h"
#include "../defs.h"

static GLuint postprocess_tex;
static GLuint vbo;
static unsigned postprocess_tex_dim[2];

struct parchment_s {
  unsigned tx, ty;
};

void parchment_init(void) {
  glGenTextures(1, &postprocess_tex);
  glGenBuffers(1, &vbo);
}

parchment* parchment_new(void) {
  parchment* this = xmalloc(sizeof(parchment));
  this->tx = this->ty = 0;
  return this;
}

void parchment_delete(parchment* this) {
  free(this);
}

static void parchment_do_preprocess(const canvas* selection) {
  if (selection->w != postprocess_tex_dim[0] ||
      selection->h != postprocess_tex_dim[1]) {
    glBindTexture(GL_TEXTURE_2D, postprocess_tex);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGB,
                 selection->w, selection->h, 0,
                 GL_RGB, GL_UNSIGNED_BYTE, NULL);
    postprocess_tex_dim[0] = selection->w;
    postprocess_tex_dim[1] = selection->h;
  }

  auxbuff_target_immediate(postprocess_tex, selection->w, selection->h);
}

void parchment_preprocess(const parchment* this,
                          const canvas* selection) {
  glm_do((void(*)(void*))parchment_do_preprocess, (void*)selection);
}

struct parchment_postprocess {
  const parchment* this;
  const canvas* canv, * selection;
};

static void parchment_do_postprocess(struct parchment_postprocess* d) {
  shader_postprocess_uniform uniform;

  shader_postprocess_vertex vertices[] = {
    { { d->canv->w, 0.0f       }, { 1.0f, 1.0f } },
    { { 0.0f,       0.0f       }, { 0.0f, 1.0f } },
    { { d->canv->w, d->canv->h }, { 1.0f, 0.0f } },
    { { 0.0f,       d->canv->h }, { 0.0f, 0.0f } },
  };

  glPushAttrib(GL_ENABLE_BIT | GL_DEPTH_BUFFER_BIT);
  glDisable(GL_DEPTH_TEST);
  glDepthMask(GL_FALSE);

  uniform.framebuffer = 0;
  /* We want these to be integer divisons so that the result is an integer, and
   * the effect remains sharp (ie, to the pixel).
   */
  uniform.pocket_size_px = d->canv->w / 426;
  uniform.px_offset[0] = ((signed)d->this->tx) / 1024;
  uniform.px_offset[1] = - ((signed)d->this->ty) / 1024;
  uniform.pocket_size_scr[0] = uniform.pocket_size_px / (float)d->canv->w;
  uniform.pocket_size_scr[1] = uniform.pocket_size_px / (float)d->canv->h;

  glBindTexture(GL_TEXTURE_2D, postprocess_tex);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);

  shader_postprocess_activate(&uniform);
  glBindBuffer(GL_ARRAY_BUFFER, vbo);
  glBufferData(GL_ARRAY_BUFFER, sizeof(vertices), vertices,
               GL_STREAM_DRAW);
  shader_postprocess_configure_vbo();

  glDrawArrays(GL_TRIANGLE_STRIP, 0, lenof(vertices));

  glPopAttrib();

  free(d);
}

void parchment_postprocess(const parchment* this, const canvas* canv,
                           const canvas* selection) {
  struct parchment_postprocess* d =
    xmalloc(sizeof(struct parchment_postprocess));
  d->this = this;
  d->canv = canv;
  d->selection = selection;

  glm_do((void(*)(void*))parchment_do_postprocess, d);
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
