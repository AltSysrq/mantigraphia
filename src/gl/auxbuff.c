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

#include <glew.h>

#include "../bsd.h"
#include "../alloc.h"
#include "../graphics/canvas.h"
#include "marshal.h"
#include "auxbuff.h"

static GLuint fbo, rbo;

void auxbuff_init(unsigned w, unsigned h) {
  glGenFramebuffers(1, &fbo);
  glGenRenderbuffers(1, &rbo);
  glBindRenderbuffer(GL_RENDERBUFFER, rbo);
  glRenderbufferStorage(GL_RENDERBUFFER, GL_DEPTH_COMPONENT32, w, h);
}

typedef struct {
  unsigned tex, ww, wh;
} auxbuff_target_op;

static void auxbuff_do_target_immediate(auxbuff_target_op* op) {
  auxbuff_target_immediate(op->tex, op->ww, op->wh);
  free(op);
}

void auxbuff_target(unsigned tex, unsigned ww, unsigned wh) {
  auxbuff_target_op* op = xmalloc(sizeof(auxbuff_target_op));

  op->tex = tex;
  op->ww = ww;
  op->wh = wh;
  glm_do((void(*)(void*))auxbuff_do_target_immediate, op);
}

void auxbuff_target_immediate(unsigned tex, unsigned ww, unsigned wh) {
  canvas dims;

  if (tex) {
    glBindFramebuffer(GL_DRAW_FRAMEBUFFER, fbo);
    glFramebufferRenderbuffer(GL_DRAW_FRAMEBUFFER, GL_DEPTH_ATTACHMENT,
                              GL_RENDERBUFFER, rbo);
    glFramebufferTexture2D(GL_DRAW_FRAMEBUFFER, GL_COLOR_ATTACHMENT0,
                           GL_TEXTURE_2D, tex, 0);

    switch (glCheckFramebufferStatus(GL_DRAW_FRAMEBUFFER)) {
#define ERR(name) case name: \
      errx(EX_SOFTWARE, "Unable to set framebuffer: %s", #name)
    ERR(GL_FRAMEBUFFER_UNDEFINED);
    ERR(GL_FRAMEBUFFER_INCOMPLETE_ATTACHMENT);
    ERR(GL_FRAMEBUFFER_INCOMPLETE_MISSING_ATTACHMENT);
    ERR(GL_FRAMEBUFFER_INCOMPLETE_DRAW_BUFFER);
    ERR(GL_FRAMEBUFFER_INCOMPLETE_READ_BUFFER);
    ERR(GL_FRAMEBUFFER_UNSUPPORTED);
    ERR(GL_FRAMEBUFFER_INCOMPLETE_MULTISAMPLE);
    case GL_FRAMEBUFFER_COMPLETE: break; //OK
    default:
      errx(EX_SOFTWARE, "Unknown error setting framebuffer");
    }
  } else {
    glBindFramebuffer(GL_DRAW_FRAMEBUFFER, 0);
  }

  canvas_init_thin(&dims, ww, wh);
  canvas_gl_clip_sub_immediate(&dims, &dims);
}
