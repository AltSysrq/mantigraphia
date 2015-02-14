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
#include <stddef.h>

#include "bsd.h"
#include "../alloc.h"
#include "../math/coords.h"
#include "../graphics/canvas.h"
#include "../graphics/perspective.h"
#include "../gl/marshal.h"
#include "../gl/shaders.h"
#include "../world/terrain-tilemap.h"
#include "../world/env-vmap.h"
#include "context.h"
#include "env-vmap-manifold-renderer.h"
#include "env-vmap-render-common.h"

#define MHIVE_SZ ENV_VMAP_MANIFOLD_RENDERER_MHIVE_SZ
#define DRAW_DISTANCE 16 /* mhives */

/**
 * A render operation is a set of polygons inside a render mhive which share the
 * same graphic plane.
 */
typedef struct {
  const env_voxel_graphic_plane* graphic;
  /**
   * The offset of the first index to render, and the number of indices in this
   * set.
   */
  unsigned offset, length;
} env_vmap_manifold_render_operation;

/**
 * A render mhive holds on-GPU precomputed data used to render some part
 * (possibly all) of an env_vmap. It is composed of zero or more operations;
 * all operations share the same vertex and index buffers.
 *
 * Render mhives are created on-demand and destroyed when no longer needed.
 */
struct env_vmap_manifold_render_mhive_s {
  /**
   * The level of detail of this mhive (0 = maximum).
   */
  unsigned char lod;
  /**
   * The vertex and index buffers for this mhive.
   */
  GLuint buffers[2];
  /**
   * The length of the operations array.
   */
  unsigned num_operations;
  env_vmap_manifold_render_operation operations[FLEXIBLE_ARRAY_MEMBER];
};

typedef struct {
  env_vmap_manifold_renderer*restrict this;
  const rendering_context*restrict ctxt;
} env_vmap_manifold_render_op;

static env_vmap_manifold_render_mhive* env_vmap_manifold_render_mhive_new(
  const env_vmap_manifold_renderer* parent, coord x0, coord z0,
  unsigned char lod);
static void env_vmap_manifold_render_mhive_delete(env_vmap_manifold_render_mhive*);
static void render_env_vmap_manifolds_impl(env_vmap_manifold_render_op*);
static void env_vmap_manifold_render_mhive_render(
  const env_vmap_manifold_render_mhive*restrict,
  const rendering_context*restrict);

env_vmap_manifold_renderer* env_vmap_manifold_renderer_new(
  const env_vmap* vmap,
  const env_voxel_graphic*const graphics[NUM_ENV_VOXEL_CONTEXTUAL_TYPES],
  const vc3 base_coordinate,
  const void* base_object,
  coord (*get_y_offset)(const void*, coord, coord)
) {
  size_t mhives_sz = sizeof(env_vmap_manifold_render_mhive*) *
    (vmap->xmax / MHIVE_SZ) * (vmap->zmax / MHIVE_SZ);
  env_vmap_manifold_renderer* this = xmalloc(
    offsetof(env_vmap_manifold_renderer, mhives) +
    mhives_sz);

  this->vmap = vmap;
  this->graphics = graphics;
  memcpy(this->base_coordinate, base_coordinate, sizeof(vc3));
  this->base_object = base_object;
  this->get_y_offset = get_y_offset;
  memset(this->mhives, 0, mhives_sz);

  return this;
}

void env_vmap_manifold_renderer_delete(env_vmap_manifold_renderer* this) {
  unsigned num_mhives =
    this->vmap->xmax / MHIVE_SZ * this->vmap->zmax / MHIVE_SZ;
  unsigned i;

  for (i = 0; i < num_mhives; ++i)
    if (this->mhives[i])
      env_vmap_manifold_render_mhive_delete(this->mhives[i]);

  free(this);
}

void render_env_vmap_manifolds(canvas* dst,
                               env_vmap_manifold_renderer*restrict this,
                               const rendering_context*restrict ctxt) {
  env_vmap_manifold_render_op* op = xmalloc(sizeof(env_vmap_manifold_render_op));

  op->this = this;
  op->ctxt = ctxt;
  glm_do((void(*)(void*))render_env_vmap_manifolds_impl, op);
}

void render_env_vmap_manifolds_impl(env_vmap_manifold_render_op* op) {
  env_vmap_manifold_renderer* this = op->this;
  const rendering_context*restrict ctxt = op->ctxt;
  const rendering_context_invariant*restrict context = CTXTINV(ctxt);
  unsigned x, z, xmax, zmax, cx, cz;
  signed dx, dz;
  unsigned d;
  signed dot;
  unsigned char desired_lod;

  free(op);

  xmax = this->vmap->xmax / MHIVE_SZ;
  zmax = this->vmap->zmax / MHIVE_SZ;
  cx = context->proj->camera[0] / TILE_SZ / MHIVE_SZ;
  cz = context->proj->camera[2] / TILE_SZ / MHIVE_SZ;

  for (z = 0; z < zmax; ++z) {
    for (x = 0; x < xmax; ++x) {
      dx = x - cx;
      dz = z - cz;
      if (this->vmap->is_toroidal) {
        dx = torus_dist(dx, xmax);
        dz = torus_dist(dz, zmax);
      }

      d = umax(abs(dx), abs(dz));

      if (d < DRAW_DISTANCE) {
        if (d <= 2)
          desired_lod = 0;
        else if (d <= 4)
          desired_lod = 1;
        else if (d < DRAW_DISTANCE/2)
          desired_lod = 2;
        else
          desired_lod = 3;

        /* We want to keep mhives prepared even if they won't be rendered this
         * frame, since the angle the camera is facing can change rapidly.
         */

        if (this->mhives[z*xmax + x] &&
            this->mhives[z*xmax + x]->lod != desired_lod) {
          env_vmap_manifold_render_mhive_delete(this->mhives[z*xmax + x]);
          this->mhives[z*xmax + x] = NULL;
        }

        if (!this->mhives[z*xmax + x])
          this->mhives[z*xmax + x] = env_vmap_manifold_render_mhive_new(
            this, x*MHIVE_SZ, z*MHIVE_SZ, desired_lod);

        dot = dx * context->proj->yrot_sin +
              dz * context->proj->yrot_cos;
        /* Only render mhives that are actually visible.
         *
         * The (d<2) condition serves two purposes. First, it is a "fudge
         * factor" to account for the fact that the calculations are based on
         * the corners of the mhives instead of the centre. Second, it causes
         * mhives "behind" the camera to be drawn when very close, which means
         * that even at somewhat high altitutes, one cannot see the mhives not
         * being drawn.
         *
         * (One can still go high enough to see those mhives, but this shouldn't
         * be too much of an issue, since the plan is to obscure that part of
         * the view anyway.)
         */
        if (dot <= 0 || d < 2)
          env_vmap_manifold_render_mhive_render(this->mhives[z*xmax + x], ctxt);
      } else {
        if (this->mhives[z*xmax + x]) {
          env_vmap_manifold_render_mhive_delete(this->mhives[z*xmax + x]);
          this->mhives[z*xmax + x] = NULL;
        }
      }
    }
  }
}

static env_vmap_manifold_render_mhive* env_vmap_manifold_render_mhive_new(
  const env_vmap_manifold_renderer* r, coord x0, coord z0,
  unsigned char lod
) {
  /* TODO */
  return NULL;
}

static void env_vmap_manifold_render_mhive_delete(
  env_vmap_manifold_render_mhive* this
) {
  glDeleteBuffers(2, this->buffers);
  free(this);
}

static void env_vmap_manifold_render_mhive_render(
  const env_vmap_manifold_render_mhive*restrict mhive,
  const rendering_context*restrict ctxt
) {
  /* TODO */
}
