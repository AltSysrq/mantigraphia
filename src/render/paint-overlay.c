/*-
 * Copyright (c) 2014, 2015 Jason Lingle
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

#include <string.h>
#include <glew.h>

#include "../bsd.h"
#include "../alloc.h"
#include "../math/rand.h"
#include "../math/frac.h"
#include "../math/coords.h"
#include "../math/poisson-disc.h"
#include "../gl/shaders.h"
#include "../gl/glinfo.h"
#include "../gl/marshal.h"
#include "../gl/auxbuff.h"
#include "../graphics/canvas.h"
#include "paint-overlay.h"

#define DESIRED_POINTS_PER_SCREENW 256
#define BRUSHTEX_SZ 256
#define BRUSHTEX_LOW_SZ 64
#define POINT_SIZE_MULT 3

struct paint_overlay_s {
  GLuint vbo, fbtex, brushtex_high, brushtex_low;
  unsigned num_points;
  unsigned point_size;
  unsigned fbtex_dim[2];
  unsigned screenw, screenh, src_screenw, src_screenh;
  int using_high_brushtex;
  float xoff, yoff;
};

static void paint_overlay_create_texture(paint_overlay*);

static inline unsigned max_effective_point_size(void) {
  return max_point_size / POINT_SIZE_MULT?
    max_point_size / POINT_SIZE_MULT : 1;
}

static void swap_discs(poisson_disc_point* a,
                       poisson_disc_point* b) {
  poisson_disc_point tmp;

  if (a != b) {
    tmp = *a;
    *a = *b;
    *b = tmp;
  }
}

static void shuffle_discs(poisson_disc_result* pdr) {
  unsigned rnd = 715;
  unsigned i, j;

  for (i = 0; i < pdr->num_points - 1; ++i) {
    j = i + lcgrand(&rnd) % (pdr->num_points - i);
    swap_discs(pdr->points + i, pdr->points + j);
  }
}

paint_overlay* paint_overlay_new(const canvas* canv) {
  paint_overlay* this = zxmalloc(sizeof(paint_overlay));
  shader_paint_overlay_vertex* vertices;
  poisson_disc_result pdr;
  unsigned i;

  poisson_disc_distribution(
    &pdr, canv->w, canv->h,
    DESIRED_POINTS_PER_SCREENW,
    max_effective_point_size() * POISSON_DISC_FP,
    9312);

  /* Shuffle the points to eliminate artefacts arising from the
   * centre-outward generation of the distribution.
   */
  shuffle_discs(&pdr);

  vertices = xmalloc(pdr.num_points * sizeof(shader_paint_overlay_vertex));
  for (i = 0; i < pdr.num_points; ++i) {
    vertices[i].v[0] = pdr.points[i].x_fp / POISSON_DISC_FP;
    vertices[i].v[1] = pdr.points[i].y_fp / POISSON_DISC_FP;
    vertices[i].v[2] = 0;
  }

  glGenTextures(1, &this->fbtex);
  glGenBuffers(1, &this->vbo);
  glBindBuffer(GL_ARRAY_BUFFER, this->vbo);
  glBufferData(GL_ARRAY_BUFFER,
               pdr.num_points * sizeof(shader_paint_overlay_vertex),
               vertices, GL_STATIC_DRAW);
  free(vertices);
  poisson_disc_result_destroy(&pdr);

  paint_overlay_create_texture(this);

  this->using_high_brushtex = 1;
  this->num_points = pdr.num_points;
  this->point_size = umax(pdr.point_size_fp / POISSON_DISC_FP, 1);
  this->screenw = canv->w;
  this->screenh = canv->h;
  return this;
}

static void paint_overlay_create_texture(paint_overlay* this) {
  unsigned i, x, y, min, max, freq, amp;
  unsigned* brushtex_data;
  unsigned char* brushtex_data_bytes;

  brushtex_data = xmalloc(sizeof(unsigned)*BRUSHTEX_SZ*BRUSHTEX_SZ +
                          BRUSHTEX_SZ*BRUSHTEX_SZ);
  brushtex_data_bytes = (void*)brushtex_data + BRUSHTEX_SZ+BRUSHTEX_SZ;

  memset(brushtex_data, 0, sizeof(unsigned)*BRUSHTEX_SZ*BRUSHTEX_SZ);
  for (freq = BRUSHTEX_SZ / 32, amp = 128; freq < BRUSHTEX_SZ;
       freq *= 2, amp /= 2)
    perlin_noise(brushtex_data, BRUSHTEX_SZ, BRUSHTEX_SZ,
                 freq, amp, amp);
  min = ~0u;
  max = 0;
  for (i = 0; i < BRUSHTEX_SZ*BRUSHTEX_SZ; ++i) {
    if (brushtex_data[i] < min) min = brushtex_data[i];
    if (brushtex_data[i] > max) max = brushtex_data[i];
  }

  for (i = 0; i < BRUSHTEX_SZ*BRUSHTEX_SZ; ++i)
    brushtex_data_bytes[i] = (brushtex_data[i] - min) * 255 / (max-min);

  glGenTextures(1, &this->brushtex_high);
  glBindTexture(GL_TEXTURE_2D, this->brushtex_high);
  glTexImage2D(GL_TEXTURE_2D, 0, GL_RED,
               BRUSHTEX_SZ, BRUSHTEX_SZ, 0,
               GL_RED, GL_UNSIGNED_BYTE, brushtex_data_bytes);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT);

  for (y = 0; y < BRUSHTEX_LOW_SZ; ++y) {
    for (x = 0; x < BRUSHTEX_LOW_SZ; ++x) {
      brushtex_data_bytes[y*BRUSHTEX_LOW_SZ + x] =
        brushtex_data_bytes[(y*BRUSHTEX_SZ/BRUSHTEX_LOW_SZ)*BRUSHTEX_SZ +
                            (x*BRUSHTEX_SZ/BRUSHTEX_LOW_SZ)];
    }
  }

  glGenTextures(1, &this->brushtex_low);
  glBindTexture(GL_TEXTURE_2D, this->brushtex_low);
  glTexImage2D(GL_TEXTURE_2D, 0, GL_RED,
               BRUSHTEX_LOW_SZ, BRUSHTEX_LOW_SZ, 0,
               GL_RED, GL_UNSIGNED_BYTE, brushtex_data_bytes);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT);

  free(brushtex_data);
}

void paint_overlay_delete(paint_overlay* this) {
  glDeleteBuffers(1, &this->vbo);
  glDeleteTextures(1, &this->fbtex);
  glDeleteTextures(1, &this->brushtex_high);
  glDeleteTextures(1, &this->brushtex_low);
  free(this);
}

static void paint_overlay_preprocess_impl(paint_overlay* this) {
  if (this->src_screenw != this->fbtex_dim[0] ||
      this->src_screenh != this->fbtex_dim[1]) {
    glBindTexture(GL_TEXTURE_2D, this->fbtex);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA,
                 this->src_screenw, this->src_screenh, 0,
                 GL_RGBA, GL_UNSIGNED_BYTE, NULL);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    this->fbtex_dim[0] = this->src_screenw;
    this->fbtex_dim[1] = this->src_screenh;
  }

  auxbuff_target_immediate(this->fbtex, this->src_screenw, this->src_screenh);
}

static void paint_overlay_postprocess_impl(paint_overlay* this) {
  shader_paint_overlay_uniform uniform;

  glPushAttrib(GL_ENABLE_BIT);
  glEnable(GL_POINT_SPRITE);
  glEnable(GL_TEXTURE_2D);

  /* It might seem like it would be better to disable everything related to the
   * depth buffer, since this effect is supposed to simply replace the whole
   * contents of the screen.
   *
   * However, the effect has a lot of overlapping splotches. By keeping the
   * depth test enabled (with GL_LESS, the default), each fragment will be
   * written at most once. This will (hopefully) allow implementations to
   * optimise away redundant fragments.
   *
   * This (plus not enabling GL_BLEND, though the latter never had any
   * perceptable effect) improved performance on test systems as follows:
   *
   *   Desktop nVidia: 60 FPS => 60 FPS (already vsync-locked)
   *   Mobile nVidia:  30 FPS => 30 FPS (probably faster, but vsync-locked)
   *   Mobile Intel:   7.5 FPS => 15 FPS
   *
   * (All tests at 1920x1080 with no resolution reduction.)
   */
  glClear(GL_DEPTH_BUFFER_BIT);

  glBindTexture(GL_TEXTURE_2D, this->fbtex);
  glActiveTexture(GL_TEXTURE1);
  glBindTexture(GL_TEXTURE_2D, this->using_high_brushtex?
                this->brushtex_high : this->brushtex_low);
  glActiveTexture(GL_TEXTURE0);

  uniform.framebuffer = 0;
  uniform.brush = 1;
  uniform.screen_size[0] = this->screenw;
  uniform.screen_size[1] = this->screenh;
  uniform.screen_off[0] = this->xoff;
  uniform.screen_off[1] = this->yoff;
  uniform.texture_freq = this->using_high_brushtex?
    1.0f : BRUSHTEX_SZ / BRUSHTEX_LOW_SZ;
  shader_paint_overlay_activate(&uniform);
  glBindBuffer(GL_ARRAY_BUFFER, this->vbo);
  glPointSize(this->point_size * POINT_SIZE_MULT);
  shader_paint_overlay_configure_vbo();
  glDrawArrays(GL_POINTS, 0, this->num_points);

  glPopAttrib();
}

void paint_overlay_preprocess(paint_overlay* this,
                              const rendering_context*restrict ctxt,
                              const canvas* src, const canvas* whole) {
  this->src_screenw = src->w;
  this->src_screenh = src->h;
  glm_do((void(*)(void*))paint_overlay_preprocess_impl, this);
}

void paint_overlay_postprocess(paint_overlay* this,
                               const rendering_context*restrict ctxt) {
  const rendering_context_invariant*restrict context =
    CTXTINV(ctxt);

  this->xoff = (-(signed)this->screenw) * 314159 / 200000 *
    context->long_yrot / context->proj->fov;
  this->yoff = (-(signed)this->screenh) * 314159 / 200000 *
    ((signed)context->proj->rxrot) / context->proj->fov;
  glm_do((void(*)(void*))paint_overlay_postprocess_impl, this);
}

int paint_overlay_is_using_high_res_texture(const paint_overlay* this) {
  return this->using_high_brushtex;
}

void paint_overlay_set_using_high_res_texture(paint_overlay* this, int high) {
  this->using_high_brushtex = high;
}
