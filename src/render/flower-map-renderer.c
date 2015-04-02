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

#include <stdlib.h>
#include <string.h>

#include "bsd.h"
#include "../alloc.h"
#include "../defs.h"
#include "../math/coords.h"
#include "../math/rand.h"
#include "../graphics/canvas.h"
#include "../graphics/perspective.h"
#include "../gl/marshal.h"
#include "../gl/shaders.h"
#include "../world/terrain-tilemap.h"
#include "../world/flower-map.h"
#include "context.h"
#include "flower-map-renderer.h"

#define DRAW_DISTANCE 16 /* fhives */
#define DRAW_DIAMETER (2 * DRAW_DISTANCE)

typedef struct {
  /**
   * The index of the fhive in the flower map.
   *
   * If equal to ~0u, the render_fhive has not been initialised.
   */
  unsigned fhive_index;
  /**
   * The vertex and index buffers for this fhive.
   */
  GLuint buffers[2];
  /**
   * The number of indices in the index buffer.
   */
  unsigned length;
} flower_map_render_fhive;

struct flower_map_renderer_s {
  const flower_map* flowers;
  const flower_graphic* graphics;
  const terrain_tilemap* terrain;

  /**
   * Hives currently within the draw distance. Each fhive at (x,z) is mapped to
   * hives[z%DRAW_DIAMETER][x%DRAW_DIAMETER]; render_fhives are tagged with the
   * index of their fhive to see if they need to be regenerated for the fhive
   * that is actually to be drawn.
   */
  flower_map_render_fhive hives[DRAW_DIAMETER][DRAW_DIAMETER];
};

typedef struct {
  flower_map_renderer* this;
  const rendering_context*restrict ctxt;
} flower_map_render_op;

static void render_flower_map_impl(flower_map_render_op*);
static void flower_map_render_fhive_prepare(
  flower_map_render_fhive* this,
  const flower_map_renderer* renderer,
  unsigned x, unsigned z);
static void flower_map_render_fhive_render(
  const flower_map_render_fhive* this,
  const flower_map_renderer* renderer,
  const rendering_context*restrict ctxt,
  unsigned x, unsigned z);

flower_map_renderer* flower_map_renderer_new(
  const flower_map* flowers,
  const flower_graphic graphics[NUM_FLOWER_TYPES],
  const terrain_tilemap* terrain
) {
  flower_map_renderer* this = xmalloc(sizeof(flower_map_renderer));
  this->flowers = flowers;
  this->graphics = graphics;
  this->terrain = terrain;

  memset(this->hives, ~0, sizeof(this->hives));
  return this;
}

void flower_map_renderer_delete(flower_map_renderer* this) {
  unsigned i, j;

  for (i = 0; i < lenof(this->hives); ++i)
    for (j = 0; j < lenof(this->hives[i]); ++j)
      if (~this->hives[i][j].fhive_index)
        glDeleteBuffers(2, this->hives[i][j].buffers);

  free(this);
}

void render_flower_map(canvas* dst,
                       flower_map_renderer*restrict this,
                       const rendering_context*restrict ctxt) {
  flower_map_render_op* op = xmalloc(sizeof(flower_map_render_op));

  op->this = this;
  op->ctxt = ctxt;
  glm_do((void(*)(void*))render_flower_map_impl, op);
}

static void render_flower_map_impl(flower_map_render_op* op) {
  flower_map_renderer* this = op->this;
  const rendering_context*restrict ctxt = op->ctxt;
  const rendering_context_invariant*restrict context = CTXTINV(ctxt);

  signed xo, zo;
  unsigned cx, cz, fx, fz, rfx, rfz;

  free(op);

  cx = context->proj->camera[0] / TILE_SZ / FLOWER_FHIVE_SIZE;
  cz = context->proj->camera[2] / TILE_SZ / FLOWER_FHIVE_SIZE;

  for (zo = -DRAW_DISTANCE+1; zo < DRAW_DISTANCE; ++zo) {
    fz = (zo + cz) & (this->flowers->fhives_h - 1);
    rfz = fz % lenof(this->hives);

    for (xo = -DRAW_DISTANCE+1; xo < DRAW_DISTANCE; ++xo) {
      fx = (xo + cx) & (this->flowers->fhives_w - 1);
      rfx = fx % lenof(this->hives[fz]);

      flower_map_render_fhive_prepare(this->hives[rfz] + rfx,
                                      this, fx, fz);
      flower_map_render_fhive_render(this->hives[rfz] + rfx,
                                     this, ctxt, fx, fz);
    }
  }
}

static void flower_map_render_fhive_prepare(
  flower_map_render_fhive* this,
  const flower_map_renderer* renderer,
  unsigned x, unsigned z
) {
  static const float corner_offsets[4][2] = {
    { -0.5f, -0.5f }, { +0.5f, -0.5f },
    { +0.5f, +0.5f }, { -0.5f, +0.5f },
  };
  /* Maximum size is capped so that indices fit in shorts anyway, and only the
   * OpenGL thread can run this code, so just statically allocate the data
   * arrays.
   */
  static shader_flower_vertex vertices[65536 / 4][4];
  static unsigned short indices[65536 / 4][6];

  const flower_fhive* hive;
  unsigned i, j, count, shadow, date_stagger, max_date_stagger;
  vc3 flower_position;
  float date0, date1;

  if (this->fhive_index == flower_map_fhive_offset(renderer->flowers, x, z))
    return;

  if (!~this->fhive_index)
    glGenBuffers(2, this->buffers);

  this->fhive_index = flower_map_fhive_offset(renderer->flowers, x, z);
  hive = renderer->flowers->hives + this->fhive_index;

  count = hive->size;
  if (count > lenof(vertices))
    count = 65536 / 4;

  for (i = 0; i < count; ++i) {
    flower_position[0] = x * FLOWER_FHIVE_SIZE * TILE_SZ +
      hive->flowers[i].x * FLOWER_COORD_UNIT;
    flower_position[2] = z * FLOWER_FHIVE_SIZE * TILE_SZ +
      hive->flowers[i].z * FLOWER_COORD_UNIT;
    flower_position[1] = hive->flowers[i].y * FLOWER_HEIGHT_UNIT +
      terrain_base_y(renderer->terrain,
                     flower_position[0], flower_position[2]);

    vertices[i][0].v[0] = flower_position[0] - x * FLOWER_FHIVE_SIZE * TILE_SZ;
    vertices[i][0].v[1] = flower_position[1];
    vertices[i][0].v[2] = flower_position[2] - z * FLOWER_FHIVE_SIZE * TILE_SZ;

    shadow = renderer->terrain->type[
      terrain_tilemap_offset(renderer->terrain,
                             flower_position[0] / TILE_SZ,
                             flower_position[2] / TILE_SZ)] &
      ((1 << TERRAIN_SHADOW_BITS) - 1);
    canvas_pixel_to_gl4fv(
      vertices[i][0].colour,
      renderer->graphics[hive->flowers[i].type].colour[shadow]);

    date0 = renderer->graphics[hive->flowers[i].type].date_appear / 65536.0f;
    date1 = renderer->graphics[hive->flowers[i].type].date_disappear / 65536.0f;
    max_date_stagger = renderer->graphics[hive->flowers[i].type].date_stagger;
    date_stagger = chaos_of(chaos_accum(chaos_accum(0, this->fhive_index), i));
    date_stagger %= 1u + max_date_stagger;
    date0 -= ((float)date_stagger - max_date_stagger / 2.0f) / 65536.0f;
    date1 -= ((float)date_stagger - max_date_stagger / 2.0f) / 65536.0f;

    vertices[i][0].lifetime_centre[0] = (date0 + date1) / 2.0f;
    vertices[i][0].lifetime_scale[0] = 2.0f / (date1 - date0);
    vertices[i][0].max_size[0] =
      renderer->graphics[hive->flowers[i].type].size;

    for (j = 0; j < 4; ++j) {
      if (j) memcpy(vertices[i] + j, vertices[i], sizeof(vertices[i]));
      memcpy(vertices[i][j].corner_offset, corner_offsets + j,
             sizeof(corner_offsets[j]));
    }

    indices[i][0] = i * 4 + 0;
    indices[i][1] = i * 4 + 1;
    indices[i][2] = i * 4 + 2;
    indices[i][3] = i * 4 + 0;
    indices[i][4] = i * 4 + 2;
    indices[i][5] = i * 4 + 3;
  }

  this->length = count * 6;
  glBindBuffer(GL_ARRAY_BUFFER, this->buffers[0]);
  glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, this->buffers[1]);
  glBufferData(GL_ARRAY_BUFFER, count * sizeof(shader_flower_vertex) * 4,
               vertices, GL_STATIC_DRAW);
  glBufferData(GL_ELEMENT_ARRAY_BUFFER,
               count * sizeof(unsigned short) * 6,
               indices, GL_STATIC_DRAW);
}

static void flower_map_render_fhive_render(
  const flower_map_render_fhive* this,
  const flower_map_renderer* renderer,
  const rendering_context*restrict ctxt,
  unsigned x, unsigned z
) {
  const rendering_context_invariant*restrict context = CTXTINV(ctxt);
  shader_flower_uniform uniform;
  unsigned i;
  coord effective_camera;

  uniform.torus_sz[0] = context->proj->torus_w;
  uniform.torus_sz[1] = context->proj->torus_h;
  uniform.yrot[0] = zo_float(context->proj->yrot_cos);
  uniform.yrot[1] = zo_float(context->proj->yrot_sin);
  uniform.rxrot[0] = zo_float(context->proj->rxrot_cos);
  uniform.rxrot[1] = zo_float(context->proj->rxrot_sin);
  uniform.zscale = zo_float(context->proj->zscale);
  uniform.soff[0] = context->proj->sxo;
  uniform.soff[1] = context->proj->syo;
  for (i = 0; i < 3; ++i) {
    effective_camera = context->proj->camera[i];
    switch (i) {
    case 0:
      effective_camera -= x * FLOWER_FHIVE_SIZE * TILE_SZ;
      effective_camera &= context->proj->torus_w - 1;
      break;
    case 1:
      break;
    case 2:
      effective_camera -= z * FLOWER_FHIVE_SIZE * TILE_SZ;
      effective_camera &= context->proj->torus_h - 1;
      break;
    }

    uniform.camera_integer[i]    = effective_camera & 0xFFFF0000;
    uniform.camera_fractional[i] = effective_camera & 0x0000FFFF;
  }
  uniform.date = context->month_integral +
    context->month_fraction / (float)fraction_of(1);
  uniform.inv_max_distance = 1.0f /
    ((DRAW_DISTANCE-1) * FLOWER_FHIVE_SIZE * TILE_SZ);

  glBindBuffer(GL_ARRAY_BUFFER, this->buffers[0]);
  glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, this->buffers[1]);
  shader_flower_activate(&uniform);
  shader_flower_configure_vbo();
  glDrawElements(GL_TRIANGLES, this->length, GL_UNSIGNED_SHORT, (GLvoid*)0);
}
