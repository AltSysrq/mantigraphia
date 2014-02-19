/*-
 * Copyright (c) 2014 Jason Lingle
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

#include "../frac.h"
#include "../defs.h"
#include "../gl/marshal.h"
#include "../gl/shaders.h"
#include "canvas.h"
#include "linear-paint-tile.h"
#include "glbrush.h"

static glm_slab_group* glmsg_line;
static GLuint texture;
#define TEXSZ 256

static void glbrush_activate_line(void*);

void glbrush_load(void) {
  canvas* canv;
  unsigned char monochrome[TEXSZ*TEXSZ];
  canvas_pixel pallet[256], p;

  glmsg_line = glm_slab_group_new(glbrush_activate_line,
                                  NULL, NULL,
                                  shader_brush_configure_vbo,
                                  sizeof(shader_brush_vertex));

  /* Render a bluescale version of the texture */
  for (p = 0; p < lenof(pallet); ++p)
    pallet[p] = argb(0,0,0,p);

  canv = canvas_new(TEXSZ, TEXSZ);
  linear_paint_tile_render(canv->px, canv->w, canv->h, 4, 32,
                           pallet, lenof(pallet));

  /* Reduce to 8-bit monochrome texture to send to GL */
  for (p = 0; p < lenof(monochrome); ++p)
    monochrome[p] = get_blue(canv->px[p]);
  canvas_delete(canv);

  glGenTextures(1, &texture);
  glBindTexture(GL_TEXTURE_2D, texture);
  glPixelStorei(GL_UNPACK_ROW_LENGTH, 0);
  glTexImage2D(GL_TEXTURE_2D, 0, GL_RED,
               TEXSZ, TEXSZ, 0,
               GL_RED, GL_UNSIGNED_BYTE, monochrome);
}

void glbrush_init(glbrush_spec* this) {
  this->meth.draw_point = (drawing_method_draw_point_t)glbrush_draw_point;
  this->meth.draw_line  = (drawing_method_draw_line_t) glbrush_draw_line;
  this->meth.flush      = (drawing_method_flush_t)     glbrush_flush;
  this->slab = glm_slab_get(glmsg_line);
}

void glbrush_draw_point(glbrush_accum* accum, const glbrush_spec* spec,
                        const vo3 where, zo_scaling_factor weight) {
  /* TODO */
}

static void glbrush_activate_line(void* ignored) {
  shader_brush_uniform uniform;

  glBindTexture(GL_TEXTURE_2D, texture);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT);

  uniform.tex = 0;
  shader_brush_activate(&uniform);
}

void glbrush_draw_line(glbrush_accum* accum, const glbrush_spec* spec,
                       const vo3 from, zo_scaling_factor from_weight,
                       const vo3 to, zo_scaling_factor to_weight) {
  shader_brush_vertex* vertices;
  unsigned short* indices, base;
  vo3 delta;
  unsigned pixel_length, from_pixel_w, to_pixel_w, i;
  float distance, from_w, to_w;
  coord_offset xoff, yoff;

  delta[0] = from[0] - to[0];
  delta[1] = from[1] - to[1];
  delta[2] = 0;
  pixel_length = omagnitude(delta);
  if (!pixel_length || pixel_length > 1024) return;

  xoff = - ((signed)(spec->screen_width * delta[1])) / 2 / (signed)pixel_length;
  yoff = + ((signed)(spec->screen_width * delta[0])) / 2 / (signed)pixel_length;
  from_pixel_w = zo_scale(spec->screen_width, from_weight);
  to_pixel_w = zo_scale(spec->screen_width, to_weight);
  from_w = from_pixel_w /
           (float)fraction_umul(spec->screen_width, spec->xscale);
  to_w = to_pixel_w /
         (float)fraction_umul(spec->screen_width, spec->xscale);
  distance = pixel_length /
             (float)fraction_umul(spec->screen_width, spec->yscale);

  base = GLM_ALLOC(&vertices, &indices, spec->slab, 4, 6);
  for (i = 0; i < 4; ++i) {
    vertices[i].parms[0] = spec->decay;
    vertices[i].parms[2] = spec->texoff;
    vertices[i].parms[3] = spec->noise;
    canvas_pixel_to_gl4fv(vertices[i].colour, spec->colour[0]);
    canvas_pixel_to_gl4fv(vertices[i].sec_colour, spec->colour[1]);
  }

  vertices[0].v[0] = from[0] - zo_scale(xoff, from_weight);
  vertices[0].v[1] = from[1] - zo_scale(yoff, from_weight);
  vertices[0].v[2] = from[2];
  vertices[1].v[0] = from[0] + zo_scale(xoff, from_weight);
  vertices[1].v[1] = from[1] + zo_scale(yoff, from_weight);
  vertices[1].v[2] = from[2];
  vertices[2].v[0] = to  [0] - zo_scale(xoff, to_weight);
  vertices[2].v[1] = to  [1] - zo_scale(yoff, to_weight);
  vertices[2].v[2] = to  [2];
  vertices[3].v[0] = to  [0] + zo_scale(xoff, to_weight);
  vertices[3].v[1] = to  [1] + zo_scale(yoff, to_weight);
  vertices[3].v[2] = to  [2];

  vertices[0].tc[0] = 0.5f - from_w / 2.0f;
  vertices[0].tc[1] = accum->distance;
  vertices[1].tc[0] = 0.5f + from_w / 2.0f;
  vertices[1].tc[1] = accum->distance;
  vertices[2].tc[0] = 0.5f - to_w / 2.0f;
  vertices[2].tc[1] = accum->distance + distance;
  vertices[3].tc[0] = 0.5f + to_w / 2.0f;
  vertices[3].tc[1] = accum->distance + distance;

  vertices[0].parms[1] = from_w;
  vertices[1].parms[1] = from_w;
  vertices[2].parms[1] = to_w;
  vertices[3].parms[1] = to_w;

  indices[0] = base + 0;
  indices[1] = base + 1;
  indices[2] = base + 2;
  indices[3] = base + 1;
  indices[4] = base + 2;
  indices[5] = base + 3;

  accum->distance += distance;
}

void glbrush_flush(glbrush_accum* accum, const glbrush_spec* this) {
  accum->distance = 0.0f;
}
