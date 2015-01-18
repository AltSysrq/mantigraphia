/*-
 * Copyright (c) 2015 Jason Lingle
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

#include "../alloc.h"
#include "../math/coords.h"
#include "../math/rand.h"
#include "../graphics/canvas.h"
#include "../gl/shaders.h"
#include "../gl/marshal.h"
#include "context.h"
#include "skybox.h"

#define TEXSZ 1024

struct skybox_s {
  GLuint clouds;
};

skybox* skybox_new(unsigned seed) {
  skybox* this = xmalloc(sizeof(skybox));
  unsigned* texdata = zxmalloc(sizeof(unsigned) * TEXSZ * TEXSZ);
  unsigned freq, amp;

  for (freq = 16, amp = 0x80000000; freq < TEXSZ/4 && amp;
       freq *= 2, amp /= 2)
    perlin_noise(texdata, TEXSZ, TEXSZ, freq, amp, seed + amp);

  glGenTextures(1, &this->clouds);
  glBindTexture(GL_TEXTURE_2D, this->clouds);
  glTexImage2D(GL_TEXTURE_2D, 0, GL_RED, TEXSZ, TEXSZ, 0,
               GL_RED, GL_UNSIGNED_INT, texdata);

  free(texdata);

  return this;
}

void skybox_delete(skybox* this) {
  glDeleteTextures(1, &this->clouds);
  free(this);
}

typedef struct {
  canvas* dst;
  skybox* this;
  const rendering_context*restrict ctxt;
} skybox_render_op;

static void skybox_do_render(skybox_render_op*);
void skybox_render(canvas* dst, skybox* this,
                   const rendering_context*restrict ctxt) {
  skybox_render_op* op;

  op = xmalloc(sizeof(skybox_render_op));
  op->dst = dst;
  op->this = this;
  op->ctxt = ctxt;
  glm_do((void(*)(void*))skybox_do_render, op);
}

static void skybox_do_render(skybox_render_op* op) {
  canvas* dst = op->dst;
  skybox* this = op->this;
  const rendering_context*restrict ctxt = op->ctxt;
  const rendering_context_invariant*restrict context = CTXTINV(ctxt);
  shader_skybox_uniform uniform;

  free(op);

  glPushAttrib(GL_ENABLE_BIT | GL_DEPTH_BUFFER_BIT);
  glDisable(GL_DEPTH_TEST);
  glDepthMask(GL_FALSE);

  glBindTexture(GL_TEXTURE_2D, this->clouds);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT);

  float basic_cloud_offset = - context->month_fraction / (float)fraction_of(1);
  basic_cloud_offset -= context->month_integral;
  basic_cloud_offset /= 200.0f;

  uniform.screen_size[0] = dst->w;
  uniform.screen_size[1] = dst->h;
  uniform.fov = context->proj->fov * 2.0f * 3.14159f / 65536.0f;
  uniform.yrot[0] = zo_float(context->proj->yrot_cos);
  uniform.yrot[1] = zo_float(context->proj->yrot_sin);
  uniform.rxrot[0] = zo_float(context->proj->rxrot_cos);
  uniform.rxrot[1] = zo_float(context->proj->rxrot_sin);
  uniform.cloud_offset_1[0] = basic_cloud_offset;
  uniform.cloud_offset_1[1] = basic_cloud_offset;
  uniform.cloud_offset_2[0] = basic_cloud_offset * 3.14f;
  uniform.cloud_offset_2[1] = basic_cloud_offset * 4.14f;
  uniform.cloudiness = 0.5f + 0.1f * zo_float(
    zo_cos(65536 * (context->month_integral + 1) / 10 +
           context->month_fraction / (fraction_of(1) / 65536) / 10));
  uniform.clouds = 0;

  shader_skybox_activate(&uniform);
  shader_skybox_configure_vbo();
  glBegin(GL_QUADS);
  glVertex3f(0.0f, 0.0f, 4095.0f * METRE);
  glVertex3f(dst->w, 0.0f, 4095.0f * METRE);
  glVertex3f(dst->w, dst->h, 4095.0f * METRE);
  glVertex3f(0.0f, dst->h, 4095.0f * METRE);
  glEnd();

  glPopAttrib();
}
