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

#include <stdlib.h>
#include <string.h>

#include "../alloc.h"
#include "../math/coords.h"
#include "../graphics/canvas.h"
#include "../graphics/perspective.h"
#include "../gl/marshal.h"
#include "../gl/shaders.h"
#include "../world/terrain-tilemap.h"
#include "../world/env-vmap.h"
#include "context.h"
#include "env-vmap-renderer.h"

static glm_slab_group* glmsg = NULL;
static GLuint temp_tex, temp_palette;
static shader_voxel_uniform temp_uni;

static void env_vmap_activate(void* _) {
  glBindTexture(GL_TEXTURE_2D, temp_tex);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT);
  glActiveTexture(GL_TEXTURE1);
  glBindTexture(GL_TEXTURE_2D, temp_palette);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP);
  glActiveTexture(GL_TEXTURE0);

  temp_uni.tex = 0;
  temp_uni.palette = 1;
  temp_uni.texture_scale[0] = 1.0f;
  temp_uni.texture_scale[1] = 1.0f;
  shader_voxel_activate(&temp_uni);
}

static void env_vmap_deactivate(void* _) {
}

static void env_vmap_init_textures() {
  unsigned char texd[64][64][3], paletted[12][8][4];
  unsigned x, y, c;
  signed dx, dy;

  for (y = 0; y < 64; ++y) {
    for (x = 0; x < 64; ++x) {
      dx = 32 - x;
      dy = 32 - y;
      texd[y][x][0] = isqrt(dx*dx + dy*dy) * 255 / 32;
      texd[y][x][1] = 255;
      texd[y][x][2] = rand() & 0xFF;
    }
  }

  for (y = 0; y < 12; ++y)
    for (x = 0; x < 8; ++x)
      for (c = 0; c < 4; ++c)
        paletted[y][x][c] = rand() & 0xFF;

  glGenTextures(1, &temp_tex);
  glBindTexture(GL_TEXTURE_2D, temp_tex);
  glPixelStorei(GL_UNPACK_ROW_LENGTH, 0);
  glTexImage2D(GL_TEXTURE_2D, 0, GL_RGB, 64, 64, 0,
               GL_RGB, GL_UNSIGNED_BYTE, texd);

  glGenTextures(1, &temp_palette);
  glBindTexture(GL_TEXTURE_2D, temp_palette);
  glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, 8, 12, 0,
               GL_RGBA, GL_UNSIGNED_BYTE, paletted);
}

env_vmap_renderer* env_vmap_renderer_new(
  const env_vmap* vmap,
  const env_voxel_graphic*const graphics[NUM_ENV_VOXEL_CONTEXTUAL_TYPES],
  const vc3 base_coordinate,
  const void* base_object,
  coord (*get_y_offset)(const void*, coord, coord),
  int subdivide
) {
  env_vmap_renderer* this = xmalloc(sizeof(env_vmap_renderer));

  this->vmap = vmap;
  this->graphics = graphics;
  memcpy(this->base_coordinate, base_coordinate, sizeof(vc3));
  this->base_object = base_object;
  this->get_y_offset = get_y_offset;

  if (!glmsg) {
    glmsg = glm_slab_group_new(env_vmap_activate,
                               env_vmap_deactivate,
                               NULL,
                               shader_voxel_configure_vbo,
                               sizeof(shader_voxel_vertex));
    env_vmap_init_textures();
  }

  return this;
}

void env_vmap_renderer_delete(env_vmap_renderer* this) {
  free(this);
}

static inline float zo_float(zo_scaling_factor f) {
  return f / (float)ZO_SCALING_FACTOR_MAX;
}

void render_env_vmap(canvas* dst,
                     env_vmap_renderer*restrict this,
                     const rendering_context*restrict ctxt) {
  /* TODO: This is insanely inefficient, has a very limited draw distance, and
   * makes presumptious assumptions about the coordinate system.
   */
  const rendering_context_invariant*restrict context =
    CTXTINV(ctxt);
  glm_slab* slab = glm_slab_get(glmsg);
  unsigned short* indices, base;
  shader_voxel_vertex* vertices;
  coord x, y, z, base_y, i;
  coord_offset xo, zo;
  vc3 world_coords;
  shader_voxel_vertex prepared_vertices[12];

  temp_uni.palette_t = context->month_integral / 9.0f +
    context->month_fraction / (float)fraction_of(1) / 9.0f;
  temp_uni.torus_sz[0] = context->proj->torus_w;
  temp_uni.torus_sz[1] = context->proj->torus_h;
  temp_uni.yrot[0] = zo_float(context->proj->yrot_cos);
  temp_uni.yrot[1] = zo_float(context->proj->yrot_sin);
  temp_uni.rxrot[0] = zo_float(context->proj->rxrot_cos);
  temp_uni.rxrot[1] = zo_float(context->proj->rxrot_sin);
  temp_uni.zscale = zo_float(context->proj->zscale);
  temp_uni.soff[0] = context->proj->sxo;
  temp_uni.soff[1] = context->proj->syo;
  for (i = 0; i < 3; ++i) {
    temp_uni.camera_integer[i] = context->proj->camera[i] & 0xFFFF0000;
    temp_uni.camera_fractional[i] = context->proj->camera[i] & 0x0000FFFF;
  }

  for (zo = -32; zo < 32; ++zo) {
    z = zo + context->proj->camera[2] / TILE_SZ;
    z &= this->vmap->zmax - 1;
    for (xo = -32; xo < 32; ++xo) {
      x = xo + context->proj->camera[0] / TILE_SZ;
      x &= this->vmap->xmax - 1;
      base_y = (*this->get_y_offset)(
        this->base_object,
        x * TILE_SZ + TILE_SZ/2,
        z * TILE_SZ + TILE_SZ/2);

      for (y = 0; y < ENV_VMAP_H; ++y) {
        if (this->vmap->voxels[env_vmap_offset(this->vmap, x, y, z)]) {
          vertices = prepared_vertices;
#define PUTV(xo,yo,zo,ts,tt) do {                                       \
            world_coords[0] = x * TILE_SZ + xo*TILE_SZ/2;               \
            world_coords[1] = base_y + y * TILE_SZ + yo*TILE_SZ/2;      \
            world_coords[2] = z * TILE_SZ + zo*TILE_SZ/2;               \
            vertices[0].v[0] = world_coords[0];                         \
            vertices[0].v[1] = world_coords[1];                         \
            vertices[0].v[2] = world_coords[2];                         \
            vertices[0].tc[0] = ts;                                     \
            vertices[0].tc[1] = tt;                                     \
            ++vertices;                                                 \
          } while (0)
#define PUTI(ix) (*indices++) = base + (ix)
#define PUTIR(ix) do {                                  \
            PUTI((ix)+0); PUTI((ix)+1); PUTI((ix)+2);   \
            PUTI((ix)+2); PUTI((ix)+3); PUTI((ix)+0);   \
          } while (0)

          PUTV(1,0,0,0,0);
          PUTV(1,0,2,0,1);
          PUTV(1,2,2,1,1);
          PUTV(1,2,0,1,0);
          PUTV(0,1,0,0,0);
          PUTV(0,1,2,0,1);
          PUTV(2,1,2,1,1);
          PUTV(2,1,0,1,0);
          PUTV(0,0,1,0,0);
          PUTV(0,2,1,0,1);
          PUTV(2,2,1,1,1);
          PUTV(2,0,1,1,0);
          base = GLM_ALLOC(&vertices, &indices, slab, 12, 18);
          memcpy(vertices, prepared_vertices, sizeof(prepared_vertices));
          PUTIR(0);
          PUTIR(4);
          PUTIR(8);
#undef PUTIR
#undef PUTI
#undef PUTV
        }
      }
    }
  }

  glm_finish_thread();
}
