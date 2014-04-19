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

#include "../alloc.h"
#include "../gl/marshal.h"
#include "../gl/shaders.h"
#include "glpencil.h"

struct glpencil_handle_s {
  glpencil_handle_info info;
  glm_slab_group* line_group, * point_group;
};

static void glpencil_line_activate(glpencil_handle*);
static void glpencil_line_deactivate(glpencil_handle*);
static void glpencil_point_activate(glpencil_handle*);
static void glpencil_point_deactivate(glpencil_handle*);

glpencil_handle* glpencil_hnew(const glpencil_handle_info* info) {
  glpencil_handle* this = xmalloc(sizeof(glpencil_handle));
  this->info = *info;
  this->line_group = glm_slab_group_new(
    (void(*)(void*))glpencil_line_activate,
    (void(*)(void*))glpencil_line_deactivate,
    this,
    shader_solid_configure_vbo, sizeof(shader_solid_vertex));
  glm_slab_group_set_primitive(this->line_group, GL_LINES);
  this->point_group = glm_slab_group_new(
    (void(*)(void*))glpencil_point_activate,
    (void(*)(void*))glpencil_point_deactivate,
    this,
    shader_pointcircle_configure_vbo, sizeof(shader_pointcircle_vertex));
  glm_slab_group_set_primitive(this->point_group, GL_POINTS);

  return this;
}

void glpencil_hconfig(glpencil_handle* this, const glpencil_handle_info* info) {
  this->info = *info;
}

void glpencil_hdelete(glpencil_handle* this) {
  glm_slab_group_delete(this->line_group);
  glm_slab_group_delete(this->point_group);
  free(this);
}

void glpencil_init(glpencil_spec* spec, glpencil_handle* handle) {
  spec->meth.draw_point = (drawing_method_draw_point_t)glpencil_draw_point;
  spec->meth.draw_line  = (drawing_method_draw_line_t) glpencil_draw_line;
  spec->meth.flush      = (drawing_method_flush_t)     glpencil_flush;
  spec->line_slab = glm_slab_get(handle->line_group);
  spec->point_slab = glm_slab_get(handle->point_group);
}

static void glpencil_line_activate(glpencil_handle* this) {
  glPushAttrib(GL_LINE_BIT | GL_TEXTURE_BIT);
  glDisable(GL_TEXTURE_2D);
  glLineWidth(this->info.thickness);
  shader_solid_activate(NULL);
}

static void glpencil_line_deactivate(glpencil_handle* this) {
  glPopAttrib();
}

static void glpencil_point_activate(glpencil_handle* this) {
  glPushAttrib(GL_ENABLE_BIT | GL_POINT_BIT | GL_TEXTURE_BIT);
  glPointSize(this->info.thickness);
  glTexEnvi(GL_POINT_SPRITE, GL_COORD_REPLACE, GL_TRUE);
  glEnable(GL_POINT_SPRITE);
  glDisable(GL_TEXTURE_2D);
  shader_pointcircle_activate(NULL);
}

static void glpencil_point_deactivate(glpencil_handle* this) {
  glPopAttrib();
}

void glpencil_draw_point(void* accum_ignore, const glpencil_spec* spec,
                         const vo3 where, zo_scaling_factor weight_ignore) {
  shader_pointcircle_vertex* vertex;
  unsigned short* index, base;

  base = GLM_ALLOC(&vertex, &index, spec->point_slab, 1, 1);
  vertex->v[0] = where[0];
  vertex->v[1] = where[1];
  vertex->v[2] = where[2];
  memcpy(vertex->colour, spec->colour, sizeof(float)*3);
  vertex->colour[3] = 1.0f;
  *index = base;
}

void glpencil_draw_line(void* accum_ignore, const glpencil_spec* spec,
                        const vo3 from, zo_scaling_factor from_weight_ignore,
                        const vo3 to, zo_scaling_factor to_weight_ignore) {
  shader_solid_vertex* vertices;
  unsigned short* indices, base;

  base = GLM_ALLOC(&vertices, &indices, spec->line_slab, 2, 2);
  vertices[0].v[0] = from[0];
  vertices[0].v[1] = from[1];
  vertices[0].v[2] = from[2];
  vertices[1].v[0] = to[0];
  vertices[1].v[1] = to[1];
  vertices[1].v[2] = to[2];
  memcpy(vertices[0].colour, spec->colour, sizeof(float)*3);
  memcpy(vertices[1].colour, spec->colour, sizeof(float)*3);
  vertices[0].colour[3] = 1.0f;
  vertices[1].colour[3] = 1.0f;

  indices[0] = base + 0;
  indices[1] = base + 1;
}
