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

static void env_vmap_activate(void* _) {
  glPushAttrib(GL_ENABLE_BIT);
  glDisable(GL_TEXTURE_2D);
  shader_solid_activate(NULL);
}

static void env_vmap_deactivate(void* _) {
  glPopAttrib();
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
                               shader_solid_configure_vbo,
                               sizeof(shader_solid_vertex));
  }

  return this;
}

void env_vmap_renderer_delete(env_vmap_renderer* this) {
  free(this);
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
  shader_solid_vertex* vertices;
  coord x, y, z, base_y;
  coord_offset xo, zo;
  vc3 world_coords;
  vo3 screen_coords;
  shader_solid_vertex prepared_vertices[12];

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
#define PUTV(xo,yo,zo) do {                                             \
            world_coords[0] = x * TILE_SZ + xo*TILE_SZ/2;               \
            world_coords[1] = base_y + y * TILE_SZ + yo*TILE_SZ/2;      \
            world_coords[2] = z * TILE_SZ + zo*TILE_SZ/2;               \
            if (!perspective_proj(screen_coords, world_coords,          \
                                  context->proj))                       \
              goto next_voxel;                                          \
            vertices[0].v[0] = screen_coords[0];                        \
            vertices[0].v[1] = screen_coords[1];                        \
            vertices[0].v[2] = screen_coords[2];                        \
            vertices[0].colour[0] = xo * 0.5f;                          \
            vertices[0].colour[1] = yo * 0.5f;                          \
            vertices[0].colour[2] = zo * 0.5f;                          \
            vertices[0].colour[3] = 1.0f;                               \
            ++vertices;                                                 \
          } while (0)
#define PUTI(ix) (*indices++) = base + (ix)
#define PUTIR(ix) do {                                  \
            PUTI((ix)+0); PUTI((ix)+1); PUTI((ix)+2);   \
            PUTI((ix)+2); PUTI((ix)+3); PUTI((ix)+0);   \
          } while (0)

          PUTV(1,0,0);
          PUTV(1,0,2);
          PUTV(1,2,2);
          PUTV(1,2,0);
          PUTV(0,1,0);
          PUTV(0,1,2);
          PUTV(2,1,2);
          PUTV(2,1,0);
          PUTV(0,0,1);
          PUTV(0,2,1);
          PUTV(2,2,1);
          PUTV(2,0,1);
          base = GLM_ALLOC(&vertices, &indices, slab, 12, 18);
          memcpy(vertices, prepared_vertices, sizeof(prepared_vertices));
          PUTIR(0);
          PUTIR(4);
          PUTIR(8);
#undef PUTIR
#undef PUTI
#undef PUTV
        }

        next_voxel:;
      }
    }
  }

  glm_finish_thread();
}
