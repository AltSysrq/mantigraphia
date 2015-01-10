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
#include "../gl/shaders.h"
#include "../gl/glinfo.h"
#include "../gl/marshal.h"
#include "../graphics/canvas.h"
#include "paint-overlay.h"

#define DESIRED_POINTS_PER_SCREENW 256
#define BRUSHTEX_SZ 256
#define POINT_SIZE_MULT 3

struct paint_overlay_s {
  GLuint vbo, fbtex, brushtex;
  unsigned num_points;
  unsigned point_size;
  unsigned screenw, screenh, src_screenw, src_screenh;
  float xoff, yoff;
};

static void paint_overlay_create_texture(paint_overlay*);

static inline unsigned max_effective_point_size(void) {
  return max_point_size / POINT_SIZE_MULT?
    max_point_size / POINT_SIZE_MULT : 1;
}

paint_overlay* paint_overlay_new(const canvas* canv) {
  /* Poisson-disc sampling.
   *
   * See http://bost.ocks.org/mike/algorithms/
   */
  paint_overlay* this = xmalloc(sizeof(paint_overlay));

  unsigned point_size = canv->w / DESIRED_POINTS_PER_SCREENW <
    max_effective_point_size()?
    canv->w / DESIRED_POINTS_PER_SCREENW :
    max_effective_point_size();
  unsigned radius = point_size/2 + 1;
  unsigned natural_grid_sz =
    fraction_umul(radius, 0x5A827999 /*1/sqrt(2)*/) - 1;
  unsigned grid_sz = natural_grid_sz? natural_grid_sz : 1;
  unsigned gridw = canv->w / grid_sz, gridh = canv->h / grid_sz;

  unsigned* active_points;
  struct { unsigned x, y; }* points;
  unsigned (*indices)[gridw];
  unsigned num_points = 0, num_active_points = 0;

  unsigned lcg = 0x5A827999 /* arbitrary seed */;
  angle ang;
  unsigned r, x, y, gx, gy;
  signed gox, goy, dx, dy;
  unsigned i, p;

  shader_paint_overlay_vertex* vertices;

  active_points = xmalloc(gridw*gridh * (2*sizeof(unsigned) +
                                         sizeof(points[0])));
  points = (void*)(active_points + gridw*gridh);
  indices = (void*)(points + gridw*gridh);

  memset(indices, 0, sizeof(unsigned)*gridh*gridw);
  memset(points, 0, sizeof(points[0])*gridh*gridw);

  points[0].x = canv->w / 2;
  points[0].y = canv->h / 2;
  active_points[0] = 0;
  num_active_points = num_points = 1;

  while (num_active_points) {
    place_next_point:

    /* Ensure the most recent point is in the grid */
    indices[points[num_points-1].y / grid_sz]
      [points[num_points-1].x / grid_sz] = num_points - 1;

    p = active_points[lcgrand(&lcg) % num_active_points];
    for (i = 0; i < 8; ++i) {
      ang = lcgrand(&lcg);
      r = radius + lcgrand(&lcg) % radius;
      x = points[p].x + zo_cosms(ang, r);
      y = points[p].y + zo_sinms(ang, r);
      if (x >= canv->w || y >= canv->h) goto reject_point;

      gx = x / grid_sz;
      gy = y / grid_sz;
      if (gx == points[indices[gy][gx]].x / grid_sz ||
          gy == points[indices[gy][gx]].y / grid_sz)
        goto reject_point;

      for (goy = -5; goy <= +5; ++goy) {
        if ((unsigned)(gy+goy) < gridh) {
          for (gox = -5; gox <= +5; ++gox) {
            if ((unsigned)(gx+gox) < gridw) {
              dx = x - points[indices[gy+goy][gx+gox]].x;
              dy = y - points[indices[gy+goy][gx+gox]].y;
              if (dx*dx + dy*dy < (signed)(radius*radius))
                goto reject_point;
            }
          }
        }
      }

      /* Acceptable distance from all neighbours */
      points[num_points].x = x;
      points[num_points].y = y;
      active_points[num_active_points] = num_points;
      ++num_points;
      ++num_active_points;
      goto place_next_point;

      reject_point:;
    }

    /* Failed to place any new points near this one, mark inactive */
    for (i = 0; active_points[i] != p; ++i);
    memmove(active_points + i, active_points + i + 1,
            (num_active_points - i - 1) * sizeof(unsigned));
    --num_active_points;
  }

  vertices = xmalloc(num_points * sizeof(shader_paint_overlay_vertex));
  for (i = 0; i < num_points; ++i) {
    vertices[i].v[0] = points[i].x;
    vertices[i].v[1] = points[i].y;
    vertices[i].v[2] = 0;
  }

  glGenTextures(1, &this->fbtex);
  glGenBuffers(1, &this->vbo);
  glBindBuffer(GL_ARRAY_BUFFER, this->vbo);
  glBufferData(GL_ARRAY_BUFFER,
               num_points * sizeof(shader_paint_overlay_vertex),
               vertices, GL_STATIC_DRAW);
  free(vertices);
  free(active_points);

  paint_overlay_create_texture(this);

  this->num_points = num_points;
  this->point_size = point_size;
  this->screenw = canv->w;
  this->screenh = canv->h;
  return this;
}

static void paint_overlay_create_texture(paint_overlay* this) {
  unsigned i, min, max;
  unsigned* brushtex_data;
  unsigned char* brushtex_data_bytes;

  brushtex_data = xmalloc(sizeof(unsigned)*BRUSHTEX_SZ*BRUSHTEX_SZ +
                          BRUSHTEX_SZ*BRUSHTEX_SZ);
  brushtex_data_bytes = (void*)brushtex_data + BRUSHTEX_SZ+BRUSHTEX_SZ;

  memset(brushtex_data, 0, sizeof(unsigned)*BRUSHTEX_SZ*BRUSHTEX_SZ);
  perlin_noise(brushtex_data, BRUSHTEX_SZ, BRUSHTEX_SZ, 8, 128, 5);
  perlin_noise(brushtex_data, BRUSHTEX_SZ, BRUSHTEX_SZ, 16, 64, 6);
  perlin_noise(brushtex_data, BRUSHTEX_SZ, BRUSHTEX_SZ, 32, 32, 7);
  perlin_noise(brushtex_data, BRUSHTEX_SZ, BRUSHTEX_SZ, 64, 16, 8);
  perlin_noise(brushtex_data, BRUSHTEX_SZ, BRUSHTEX_SZ, 128, 8, 9);
  min = ~0u;
  max = 0;
  for (i = 0; i < BRUSHTEX_SZ*BRUSHTEX_SZ; ++i) {
    if (brushtex_data[i] < min) min = brushtex_data[i];
    if (brushtex_data[i] > max) max = brushtex_data[i];
  }

  for (i = 0; i < BRUSHTEX_SZ*BRUSHTEX_SZ; ++i)
    brushtex_data_bytes[i] = (brushtex_data[i] - min) * 255 / (max-min);

  glGenTextures(1, &this->brushtex);
  glBindTexture(GL_TEXTURE_2D, this->brushtex);
  glTexImage2D(GL_TEXTURE_2D, 0, GL_RED,
               BRUSHTEX_SZ, BRUSHTEX_SZ, 0,
               GL_RED, GL_UNSIGNED_BYTE, brushtex_data_bytes);

  free(brushtex_data);
}

void paint_overlay_delete(paint_overlay* this) {
  glDeleteBuffers(1, &this->vbo);
  glDeleteTextures(1, &this->fbtex);
  glDeleteTextures(1, &this->brushtex);
  free(this);
}

static void paint_overlay_preprocess_impl(paint_overlay* this) {
  glBindTexture(GL_TEXTURE_2D, this->fbtex);
  glCopyTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA,
                   /* Need to offset Y because OpenGL's concept of the Y axis
                    * is upside-down.
                    */
                   0, this->screenh - this->src_screenh,
                   this->src_screenw, this->src_screenh, 0);
}

static void paint_overlay_postprocess_impl(paint_overlay* this) {
  shader_paint_overlay_uniform uniform;

  glPushAttrib(GL_ENABLE_BIT);
  glEnable(GL_POINT_SPRITE);
  glEnable(GL_TEXTURE_2D);
  glDisable(GL_DEPTH_TEST);
  glEnable(GL_BLEND);
  glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
  glBindTexture(GL_TEXTURE_2D, this->fbtex);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
  glActiveTexture(GL_TEXTURE1);
  glBindTexture(GL_TEXTURE_2D, this->brushtex);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT);
  glActiveTexture(GL_TEXTURE0);

  uniform.framebuffer = 0;
  uniform.brush = 1;
  uniform.screen_size[0] = this->screenw;
  uniform.screen_size[1] = this->screenh;
  uniform.screen_off[0] = this->xoff;
  uniform.screen_off[1] = this->yoff;
  shader_paint_overlay_activate(&uniform);
  glBindBuffer(GL_ARRAY_BUFFER, this->vbo);
  glPointSize(this->point_size * POINT_SIZE_MULT);
  shader_paint_overlay_configure_vbo();
  glDrawArrays(GL_POINTS, 0, this->num_points);

  glPopAttrib();
}

void paint_overlay_preprocess(paint_overlay* this,
                              const rendering_context*restrict ctxt,
                              const canvas* src) {
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
