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

#include "../math/frac.h"
#include "../defs.h"
#include "../alloc.h"
#include "../math/rand.h"
#include "../gl/marshal.h"
#include "../gl/shaders.h"
#include "../gl/glinfo.h"
#include "canvas.h"
#include "linear-paint-tile.h"
#include "glbrush.h"

static GLuint noise_texture;
static GLuint perlin_texture;
#define TEXSZ 256

struct glbrush_handle_s {
  glm_slab_group* glmsg_line, * glmsg_point, * glmsg_poly_point;
  GLuint palette_texture;
  float decay, noise;
};

static void glbrush_activate_line(glbrush_handle*);
static void glbrush_activate_point(glbrush_handle*);
static void glbrush_deactivate_point(glbrush_handle*);
static void glbrush_activate_poly_point(glbrush_handle*);

void glbrush_load(void) {
  canvas* canv, * brush;
  unsigned char monochrome[TEXSZ*TEXSZ];
  canvas_pixel palette[2];
  unsigned x, y, p;
  angle theta;

  /* Render a bluescale version of the texture */
  palette[0] = argb(0,0,0,0);
  palette[1] = argb(0,0,0,255);

  canv = canvas_new(TEXSZ, TEXSZ);
  brush = canvas_new(TEXSZ, TEXSZ);
  linear_paint_tile_render(canv->px, canv->w, canv->h, 4, 32,
                           palette, lenof(palette));

  /* Reduce to 8-bit monochrome texture to send to GL */
  for (p = 0; p < lenof(monochrome); ++p)
    monochrome[p] = get_blue(canv->px[p]);

  glGenTextures(1, &noise_texture);
  glBindTexture(GL_TEXTURE_2D, noise_texture);
  glPixelStorei(GL_UNPACK_ROW_LENGTH, 0);
  glTexImage2D(GL_TEXTURE_2D, 0, GL_RED,
               TEXSZ, TEXSZ, 0,
               GL_RED, GL_UNSIGNED_BYTE, monochrome);

  memset(canv->px, 0, sizeof(canvas_pixel) * canv->w * canv->h);
  perlin_noise(canv->px, canv->w, canv->h, 16, 64, 0);
  perlin_noise(canv->px, canv->w, canv->h, 32, 32, 1);
  perlin_noise(canv->px, canv->w, canv->h, 64, 16, 2);
  perlin_noise(canv->px, canv->w, canv->h, 128, 16, 2);

  palette[1] = argb(0,0,0,128);
  linear_paint_tile_render(brush->px, brush->w, brush->h, 16, 1,
                           palette, lenof(palette));
  for (y = 0; y < brush->h; ++y) {
    for (x = 0; x < brush->w; ++x) {
      p = x + TEXSZ * y;
      theta = x * 65536 * 2 / TEXSZ;
      theta += canv->px[p] * 128;
      canv->px[p] += brush->px[
        x + TEXSZ *
        ((TEXSZ - 1) & (y + zo_cosms(theta, TEXSZ)/10))];
    }
  }

  for (p = 0; p < lenof(monochrome); ++p)
    monochrome[p] = get_blue(canv->px[p]);

  glGenTextures(1, &perlin_texture);
  glBindTexture(GL_TEXTURE_2D, perlin_texture);
  glTexImage2D(GL_TEXTURE_2D, 0, GL_RED,
               TEXSZ, TEXSZ, 0,
               GL_RED, GL_UNSIGNED_BYTE, monochrome);

  canvas_delete(canv);
  canvas_delete(brush);
}

glbrush_handle* glbrush_hnew(const glbrush_handle_info* info) {
  glbrush_handle* handle = xmalloc(sizeof(glbrush_handle));
  glGenTextures(1, &handle->palette_texture);
  handle->glmsg_line = glm_slab_group_new((void(*)(void*))glbrush_activate_line,
                                          NULL, handle,
                                          shader_brush_configure_vbo,
                                          sizeof(shader_brush_vertex));
  handle->glmsg_point = glm_slab_group_new(
    (void(*)(void*))glbrush_activate_point,
    (void(*)(void*))glbrush_deactivate_point, handle,
    shader_splotch_configure_vbo,
    sizeof(shader_splotch_vertex));
  glm_slab_group_set_primitive(handle->glmsg_point, GL_POINTS);
  glm_slab_group_set_indices_enabled(handle->glmsg_point, 0);

  handle->glmsg_poly_point = glm_slab_group_new(
    (void(*)(void*))glbrush_activate_poly_point,
    NULL, handle,
    shader_poly_splotch_configure_vbo,
    sizeof(shader_poly_splotch_vertex));

  glbrush_hconfig(handle, info);
  return handle;
}

void glbrush_hconfig(glbrush_handle* handle, const glbrush_handle_info* info) {
  /* Even though this is a one-dimensional texture, we need to make it 2D,
   * since the activation of GL_TEXTURE_2D supercedes the use of 1D textures.
   */
  glBindTexture(GL_TEXTURE_2D, handle->palette_texture);
  glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA,
               info->palette_size, 1, 0,
               GL_BGRA, GL_UNSIGNED_BYTE, info->palette);

  handle->decay = info->decay;
  handle->noise = info->noise;
}

void glbrush_hset(glbrush_handle** handle,
                  const glbrush_handle_info* info,
                  int permit_refresh) {
  if (!*handle)
    *handle = glbrush_hnew(info);
  else if (permit_refresh)
    glbrush_hconfig(*handle, info);
}

void glbrush_hdelete(glbrush_handle* handle) {
  glm_slab_group_delete(handle->glmsg_line);
  glm_slab_group_delete(handle->glmsg_point);
  glDeleteTextures(1, &handle->palette_texture);
  free(handle);
}

void glbrush_init(glbrush_spec* this, glbrush_handle* handle) {
  this->meth.draw_point = (drawing_method_draw_point_t)glbrush_draw_point;
  this->meth.draw_line  = (drawing_method_draw_line_t) glbrush_draw_line;
  this->meth.flush      = (drawing_method_flush_t)     glbrush_flush;
  this->line_slab = glm_slab_get(handle->glmsg_line);
  this->point_slab = glm_slab_get(handle->glmsg_point);
  this->point_poly_slab = glm_slab_get(handle->glmsg_poly_point);
}

static void glbrush_activate_point_common(glbrush_handle* handle) {
  glBindTexture(GL_TEXTURE_2D, perlin_texture);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT);
  glActiveTexture(GL_TEXTURE1);
  glBindTexture(GL_TEXTURE_2D, handle->palette_texture);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP);
  glActiveTexture(GL_TEXTURE0);
}

static void glbrush_activate_point(glbrush_handle* handle) {
  shader_splotch_uniform uniform;

  glbrush_activate_point_common(handle);

  glPushAttrib(GL_ENABLE_BIT | GL_POINT_BIT);
  glEnable(GL_POINT_SPRITE);
  glEnable(GL_PROGRAM_POINT_SIZE);
  /* Point vertices are clipped with respect to the internal point-size setting
   * and their location alone, regardless of how big the vertex shader says
   * they are. Since we can't vary this point-size per vertex, set one bigger
   * than any point we'll ever draw so that only the quad produced by the
   * point can be clipped away. (It's still faster than drawing quads
   * manually.)
   */
  glPointSize(65536);

  uniform.tex = 0;
  uniform.palette = 1;
  uniform.noise = handle->noise;
  shader_splotch_activate(&uniform);
}

static void glbrush_activate_poly_point(glbrush_handle* handle) {
  shader_poly_splotch_uniform uniform;

  glbrush_activate_point_common(handle);

  uniform.tex = 0;
  uniform.palette = 1;
  uniform.noise = handle->noise;
  shader_poly_splotch_activate(&uniform);
}

static void glbrush_deactivate_point(glbrush_handle* handle) {
  glPopAttrib();
}

void glbrush_draw_point(glbrush_accum* accum, const glbrush_spec* spec,
                        const vo3 where, zo_scaling_factor weight) {
  shader_splotch_vertex* vertices;
  shader_poly_splotch_vertex* pvertices;
  unsigned short base, * indices;
  signed size;
  float txxoff, txyoff;

  txxoff = lcgrand(&accum->rand) / 65536.0f;
  txyoff = lcgrand(&accum->rand) / 65536.0f;

  /* Size here will actually become negative for large objects when the camera
   * becomes very close to them. An interesting effect of this is that as the
   * camera gets closer, the object gets *smaller*. This gives a nice "pushing
   * out of the way effect" and also means that the object doesn't suddenly
   * vanish, so handle the case appropriately and adapt to OpenGL by taking the
   * absolute value before we pass it in.
   */
  size = zo_scale(spec->screen_width, weight);
  if (!size || size > 65536) return;

  /* Draw point sprites if possible, as that's much faster, but we must
   * sometimes fall back on polygons.
   */
  if ((unsigned)abs(size) < max_point_size &&
      (can_draw_offscreen_points ||
       (where[0] >= 0 && where[0] < (signed)spec->screen_width &&
        where[1] >= 0 && where[1] < (signed)spec->screen_height))) {
    (void)GLM_ALLOC(&vertices, NULL, spec->point_slab, 1, 0);
    vertices[0].v[0] = where[0];
    vertices[0].v[1] = where[1];
    vertices[0].v[2] = where[2];
    vertices[0].parms[0] = txxoff;
    vertices[0].parms[1] = txyoff;
    vertices[0].parms[2] = abs(size);
  } else {
    /* We need to fall back on polygons */
    base = GLM_ALLOC(&pvertices, &indices, spec->point_poly_slab, 4, 6);
    pvertices[0].v[0] = where[0] - size/2;
    pvertices[0].v[1] = where[1] - size/2;
    pvertices[0].v[2] = where[2];
    pvertices[0].tc[0] = 0.0f;
    pvertices[0].tc[1] = 0.0f;
    pvertices[0].parms[0] = txxoff;
    pvertices[0].parms[1] = txyoff;
    pvertices[0].parms[2] = abs(size);
    pvertices[1].v[0] = where[0] + size/2;
    pvertices[1].v[1] = where[1] - size/2;
    pvertices[1].v[2] = where[2];
    pvertices[1].tc[0] = 1.0f;
    pvertices[1].tc[1] = 0.0f;
    pvertices[1].parms[0] = txxoff;
    pvertices[1].parms[1] = txyoff;
    pvertices[1].parms[2] = abs(size);
    pvertices[2].v[0] = where[0] - size/2;
    pvertices[2].v[1] = where[1] + size/2;
    pvertices[2].v[2] = where[2];
    pvertices[2].tc[0] = 0.0f;
    pvertices[2].tc[1] = 1.0f;
    pvertices[2].parms[0] = txxoff;
    pvertices[2].parms[1] = txyoff;
    pvertices[2].parms[2] = abs(size);
    pvertices[3].v[0] = where[0] + size/2;
    pvertices[3].v[1] = where[1] + size/2;
    pvertices[3].v[2] = where[2];
    pvertices[3].tc[0] = 1.0f;
    pvertices[3].tc[1] = 1.0f;
    pvertices[3].parms[0] = txxoff;
    pvertices[3].parms[1] = txyoff;
    pvertices[3].parms[2] = abs(size);
    indices[0] = base + 0;
    indices[1] = base + 1;
    indices[2] = base + 2;
    indices[3] = base + 1;
    indices[4] = base + 2;
    indices[5] = base + 3;
  }
}

static void glbrush_activate_line(glbrush_handle* handle) {
  shader_brush_uniform uniform;

  glBindTexture(GL_TEXTURE_2D, noise_texture);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT);
  glActiveTexture(GL_TEXTURE1);
  glBindTexture(GL_TEXTURE_2D, handle->palette_texture);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP);
  glActiveTexture(GL_TEXTURE0);

  uniform.tex = 0;
  uniform.palette = 1;
  uniform.decay = handle->decay;
  uniform.noise = handle->noise;
  shader_brush_activate(&uniform);
}

void glbrush_draw_line(glbrush_accum* accum, const glbrush_spec* spec,
                       const vo3 from, zo_scaling_factor from_weight,
                       const vo3 to, zo_scaling_factor to_weight) {
  shader_brush_vertex* vertices;
  unsigned short* indices, base;
  vo3 delta;
  unsigned pixel_length, from_pixel_w, to_pixel_w;
  float distance, from_w, to_w;
  coord_offset xoff, yoff;

  delta[0] = from[0] - to[0];
  delta[1] = from[1] - to[1];
  delta[2] = 0;
  pixel_length = omagnitude(delta);
  if (!pixel_length || pixel_length > 65535) return;

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

  base = GLM_ALLOC(&vertices, &indices, spec->line_slab, 4, 6);

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

  vertices[0].info[0] = vertices[1].info[0] = from_w;
  vertices[2].info[0] = vertices[3].info[0] = to_w;
  vertices[0].info[1] = vertices[1].info[1] = -1.0f;
  vertices[2].info[1] = vertices[3].info[1] = +1.0f;

  indices[0] = base + 0;
  indices[1] = base + 1;
  indices[2] = base + 2;
  indices[3] = base + 1;
  indices[4] = base + 2;
  indices[5] = base + 3;

  accum->distance += distance;
}

void glbrush_flush(glbrush_accum* accum, const glbrush_spec* this) {
  accum->distance = this->base_distance;
  accum->rand = this->random_seed;
}
