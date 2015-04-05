/*-
 * Copyright (c) 2013, 2014, 2015 Jason Lingle
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
#include <glew.h>
#include <GL/glu.h>

#include <stdlib.h>
#include <string.h>

#include "../alloc.h"
#include "../micromp.h"
#include "../math/frac.h"
#include "../math/coords.h"
#include "../math/matrix.h"
#include "../gl/marshal.h"
#include "canvas.h"

SDL_PixelFormat* screen_pixel_format;

static void canvas_do_gl_clip_sub(const canvas**);

canvas* canvas_new(unsigned width, unsigned height) {
  canvas* this = xmalloc(
    sizeof(canvas) + UMP_CACHE_LINE_SZ +
    sizeof(canvas_pixel)*width*height + UMP_CACHE_LINE_SZ +
    sizeof(canvas_depth)*width*height);

  this->w = width;
  this->h = height;
  this->pitch = width;
  this->logical_width = width;
  this->ox = this->oy = 0;
  this->px = align_to_cache_line(this + 1);
  this->depth = align_to_cache_line(this->px + width*height);
  return this;
}

void canvas_delete(canvas* this) {
  free(this);
}

void canvas_init_thin(canvas* this, unsigned w, unsigned h) {
  this->w = w;
  this->h = h;
  this->pitch = w;
  this->logical_width = w;
  this->ox = this->oy = 0;
  this->px = NULL;
  this->depth = NULL;
}

void canvas_clear(canvas* this) {
  /* Set each depth element to 0x7F7F7F7F. The sign bit can't be set, as
   * SIMD-based drawing functions use signed depth tests. Nothing will ever be
   * drawing 32km away anyway, so the loss of this bit is insignificant.
   */
  memset(this->depth, 0x7F, sizeof(canvas_depth) * this->w * this->h);
}

void canvas_blit(SDL_Texture* dst, const canvas* this) {
  SDL_UpdateTexture(dst, NULL, this->px, this->w * sizeof(canvas_pixel));
}

void canvas_slice(canvas* slice, const canvas* backing,
                  unsigned x, unsigned y,
                  unsigned w, unsigned h) {
  slice->w = w;
  slice->h = h;
  slice->pitch = backing->pitch;
  slice->logical_width = backing->logical_width;
  slice->ox = x + backing->ox;
  slice->oy = y + backing->oy;
  slice->px = backing->px + canvas_offset(backing, x, y);
  slice->depth = backing->depth + canvas_offset(backing, x, y);
}

void canvas_scale_onto(canvas*restrict dst, const canvas*restrict src) {
  glPixelStorei(GL_UNPACK_ROW_LENGTH, src->pitch);
  glPixelStorei(GL_PACK_ROW_LENGTH, dst->pitch);
  int error = gluScaleImage(GL_RGBA,
                            src->w, src->h,
                            GL_UNSIGNED_BYTE, src->px,
                            dst->w, dst->h,
                            GL_UNSIGNED_BYTE, dst->px);
  if (error)
    errx(EX_SOFTWARE, "Failed to rescale image; code=%d", error);
}

static unsigned halve_dim(unsigned in) {
  if (1 == in) return 1;
  else         return in/2;
}

GLuint canvas_to_texture(const canvas* this, int mipmap) {
  canvas* temporary = NULL;
  const canvas* src = this;
  GLuint texture;
  GLint level = 0;
  int image_is_supported, is_last_level;

  glGenTextures(1, &texture);

  /* Halve the size of the input texture until we succed at writing it to the
   * texture proxy.
   */
  while (1 /* controlled in loop */) {
    glTexImage2D(GL_PROXY_TEXTURE_2D, 0, GL_RGBA,
                 src->w, src->h, 0,
                 GL_BGRA, GL_UNSIGNED_BYTE, src->px);
    glGetTexLevelParameteriv(GL_PROXY_TEXTURE_2D, 0, GL_TEXTURE_WIDTH,
                             &image_is_supported);
    if (image_is_supported) break;

    /* Try again with a canvas half the size */
    if (src->w == 1 && src->h == 1)
      errx(EX_SOFTWARE, "Unable to convert canvas to texture of any size");

    temporary = canvas_new(halve_dim(src->w), halve_dim(src->h));
    canvas_scale_onto(temporary, src);
    if (src != this) canvas_delete((canvas*)src);
    src = temporary;
  }

  glBindTexture(GL_TEXTURE_2D, texture);
  do {
    is_last_level = (src->h == 1 && src->w == 1);
    glPixelStorei(GL_UNPACK_ROW_LENGTH, src->pitch);
    glTexImage2D(GL_TEXTURE_2D, level++, GL_RGBA,
                 src->w, src->h, 0,
                 GL_BGRA, GL_UNSIGNED_BYTE, src->px);
    temporary = canvas_new(halve_dim(src->w), halve_dim(src->h));
    canvas_scale_onto(temporary, src);
    if (src != this) canvas_delete((canvas*)src);
    src = temporary;
  } while (mipmap && !is_last_level);

  glPixelStorei(GL_UNPACK_ROW_LENGTH, 0);
  if (temporary) free(temporary);

  return texture;
}

static void canvas_do_gl_clip_sub(const canvas** parms) {
  const canvas* sub = parms[0], * whole = parms[1];

  free(parms);
  canvas_gl_clip_sub_immediate(sub, whole);
}

void canvas_gl_clip_sub_immediate(const canvas* sub,
                                  const canvas* whole) {
  mat44fgl projection_matrix;

  glViewport(sub->ox, whole->h - sub->oy - sub->h,
             sub->w, sub->h);
  projection_matrix = mat44fgl_identity;
  projection_matrix = mat44fgl_multiply(
    projection_matrix, mat44fgl_ortho(sub->ox, sub->w, sub->oy, sub->h,
                                      0, 4096*METRE));
  /* Invert the Y axis so that GL coordinates match screen coordinates, and the
   * Z axis so that positive moves into the screen. (Ie, match the canvas
   * coordinate system.)
   */
  projection_matrix = mat44fgl_multiply(
    projection_matrix, mat44fgl_scale(1.0f, -1.0f, -1.0f));
  projection_matrix = mat44fgl_multiply(
    projection_matrix, mat44fgl_translate(0.0f, -(float)sub->h, 0.0f));

  glMatrixMode(GL_PROJECTION);
  glLoadMatrixf((const float*)projection_matrix.m);
  glMatrixMode(GL_MODELVIEW);
  glLoadMatrixf((const float*)mat44fgl_identity.m);
}

void canvas_gl_clip_sub(const canvas* sub, const canvas* whole) {
  const canvas** parms;

  parms = xmalloc(2 * sizeof(const canvas*));
  parms[0] = sub;
  parms[1] = whole;
  glm_do((void(*)(void*))canvas_do_gl_clip_sub, parms);
}
