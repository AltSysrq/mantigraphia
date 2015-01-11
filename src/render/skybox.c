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
#include "../graphics/canvas.h"
#include "../gl/shaders.h"
#include "../gl/marshal.h"
#include "context.h"
#include "skybox.h"

struct skybox_s {
  /* TODO, there'll be stuff here later */
  int dummy;
};

skybox* skybox_new(void) {
  skybox* this = xmalloc(sizeof(skybox));

  return this;
}

void skybox_delete(skybox* this) {
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
  shader_skybox_uniform uniform;

  free(op);

  glPushAttrib(GL_ENABLE_BIT);
  glDisable(GL_DEPTH_TEST);

  uniform.screen_size[0] = dst->w;
  uniform.screen_size[1] = dst->h;

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
