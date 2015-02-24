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

#include <assert.h>
#include <stdlib.h>
#include <string.h>
#include <stddef.h>

#include "bsd.h"
#include "../alloc.h"
#include "../defs.h"
#include "../math/coords.h"
#include "../math/sse.h"
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
  const env_voxel_graphic_blob* graphic;
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
  /**
   * The base coordinate of this mhive.
   */
  vc3 base_coordinate;

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

  glPushAttrib(GL_ENABLE_BIT);
  glEnable(GL_CULL_FACE);

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

  glPopAttrib();
}

#define MAX_VERTICES 65535
#define MAX_FACES 65535
#define NVX (3+MHIVE_SZ)
#define NVY (1+ENV_VMAP_H)
#define NVZ (3+MHIVE_SZ)

typedef struct {
  unsigned short vertices[4];
  unsigned char  graphic;
  unsigned char  is_extraneous;
} manifold_face;

static const env_voxel_graphic_blob* env_vmap_manifold_renderer_get_graphic_blob(
  const env_voxel_graphic*const* graphics,
  const env_vmap* vmap,
  coord x, coord y, coord z,
  unsigned char lod
) {
  const env_voxel_graphic* g;

  g = env_vmap_renderer_get_graphic(graphics, vmap, x, y, z, lod);
  if (!g) return NULL;
  return g->blob;
}

static env_vmap_manifold_render_mhive* env_vmap_manifold_render_mhive_new(
  const env_vmap_manifold_renderer* r, coord x0, coord z0,
  unsigned char lod
) {
  /*
    Producing the manifold data for the mhive is divided into three phases:

    - Scan the space within the mhive and generate vertices and faces as
      necessary, producing a mesh of axis-aligned unit quads.

    - Run Catmull-Clark subdivision on the mesh as vertex and face capacity
      permit, up to a number of passes defined by the lod.

    - Triangulate the resulting faces to be sent to OpenGL, grouped according
      to their graphic.

    Note that no distinction is made between graphic blobs when forming the
    mesh; different blobs will flow smoothely into each other.

    When scanning space, a face is if the voxel under the cursor has a graphic
    blob, and a selected neighbour does not. Vertices for faces are generated
    on-demand in the `vertices` array, which grows linearly. `vertex_indices`
    maps spacial coordinates to indices within this array; values with all 1
    bits indicate vertices that have not yet been generated. Y coordinates of
    the base object are cached in `base_y`, with values of all 1 bits
    indicating tiles whose Y coordinate is not yet known.

    Each face is a quad, wound in counter-clockwise order when viewed from the
    adjacent cell with no graphic blob. In order for subdivision to work
    correctly, faces are generated for voxels one coordinate _outside_ the
    mhive; these faces are considered extraneous, and are not included in
    rendering proper, since their subdivision is not fully defined, and they
    are rendered by other mhives anyway.

    Faces are generated into the `faces` array linearly, and are not spatially
    addressible; they do not store their own adjacencies. They do track the
    ordinal of their graphic blob, which is the graphic blob of the vertex from
    which they were generated.

    Adjacencies for vertices are stored in the `vertex_adjacency` array,
    indexed by vertex index. Elements for each vertex are in no particular
    order; values of ~0 indicate an empty slot.
   */
  static coord base_y[NVZ][NVX];
  static sseps vertices[MAX_VERTICES];
  static unsigned short vertex_indices[NVZ][NVX][NVY];
  static unsigned short vertex_adjacency[MAX_VERTICES][6];
  static manifold_face faces[MAX_FACES];
  static unsigned short triangulated_indices[MAX_FACES*6];
  /* A bitset indicating whether each voxel in the column [z][x] has a graphic
   * blob.
   *
   * This is rather dependent on ENV_VMAP_H being 32.
   */
  static unsigned has_graphic_blob[2+MHIVE_SZ][2+MHIVE_SZ];
  unsigned short num_vertices;
  unsigned short num_faces;
  unsigned num_triangulated_indices;

  signed cx, cy, cz, ocx, ocy, ocz, vcx, vcy, vcz;
  coord x, y, z, xmask, zmask, vx, vz;
  unsigned ck, cv, i, op, haystack, needle;
  const env_voxel_graphic_blob* graphic_blobs[256];
  const env_voxel_graphic_blob* graphic;
  unsigned num_graphic_blobs;

  env_vmap_manifold_render_mhive* mhive;

  static const struct {
    signed char ox, oy, oz;
    struct {
      unsigned char rx, ry, rz;
    } v[4];
  } voxel_checks[6] = {
    { +0, -1, +0, { { 0, 0, 0 }, { 1, 0, 0 }, { 1, 0, 1 }, { 0, 0, 1 } } },
    { +0, +1, +0, { { 0, 1, 0 }, { 0, 1, 1 }, { 1, 1, 1 }, { 1, 1, 0 } } },
    { -1, +0, +0, { { 0, 0, 0 }, { 0, 0, 1 }, { 0, 1, 1 }, { 0, 1, 0 } } },
    { +1, +0, +0, { { 1, 0, 0 }, { 1, 1, 0 }, { 1, 1, 1 }, { 1, 0, 1 } } },
    { +0, +0, -1, { { 0, 0, 0 }, { 0, 1, 0 }, { 1, 1, 0 }, { 1, 0, 0 } } },
    { +0, +0, +1, { { 0, 0, 1 }, { 1, 0, 1 }, { 1, 1, 1 }, { 0, 1, 1 } } },
  };

  /* Check assumption about elements has_graphic_blob being the correct size */
  switch (0) {
  case 0:
  case ENV_VMAP_H == 8*sizeof(has_graphic_blob[0][0]):;
  }

  memset(base_y, ~0, sizeof(base_y));
  memset(vertex_indices, ~0, sizeof(vertex_indices));
  memset(vertex_adjacency, ~0, sizeof(vertex_adjacency));
  memset(graphic_blobs, 0, sizeof(graphic_blobs));
  num_vertices = 0;
  num_faces = 0;
  num_triangulated_indices = 0;

  if (r->vmap->is_toroidal) {
    xmask = r->vmap->xmax - 1;
    zmask = r->vmap->zmax - 1;
  } else {
    xmask = zmask = ~0u;
  }

  /* Build bitset of grahpic blob presence */
  for (cz = -1; cz <= MHIVE_SZ>>lod; ++cz) {
    z = (z0 + (cz<<lod)) & zmask;
    if (z >= r->vmap->zmax) {
      memset(has_graphic_blob[cz+1], 0, sizeof(has_graphic_blob[cz+1]));
      continue;
    }

    for (cx = -1; cx <= MHIVE_SZ>>lod; ++cx) {
      has_graphic_blob[cz+1][cx+1] = 0;

      x = (x0 + (cx<<lod)) & xmask;
      if (x >= r->vmap->xmax) continue;

       for (cy = 0; cy < ENV_VMAP_H>>lod; ++cy) {
        y = cy<<lod;
        graphic = env_vmap_manifold_renderer_get_graphic_blob(
          r->graphics, r->vmap, x, y, z, lod);
        if (graphic) {
          has_graphic_blob[cz+1][cx+1] |= 1 << cy;
        }
      }
    }
  }

  /* Generate faces */
  for (cz = -1; cz <= MHIVE_SZ>>lod; ++cz) {
    z = (z0 + (cz<<lod)) & zmask;
    if (z >= r->vmap->zmax) continue;

    for (cx = -1; cx <= MHIVE_SZ>>lod; ++cx) {
      x = (x0 + (cx<<lod)) & xmask;
      if (x >= r->vmap->xmax) continue;
      /* Skip the inner loop if we know there's nothing in this column */
      if (!has_graphic_blob[cz+1][cx+1]) continue;

      for (cy = 0; cy < ENV_VMAP_H>>lod; ++cy) {
        if (!(has_graphic_blob[cz+1][cx+1] & (1 << cy))) continue;
        y = cy<<lod;

        graphic = NULL;

        for (ck = 0; ck < lenof(voxel_checks); ++ck) {
          ocx = cx + voxel_checks[ck].ox;
          ocy = cy + voxel_checks[ck].oy;
          ocz = cz + voxel_checks[ck].oz;

          if (ocx < -1 || ocx >  MHIVE_SZ>>lod ||
              ocy <  0 || ocy >= ENV_VMAP_H ||
              ocz < -1 || ocz >  MHIVE_SZ>>lod ||
              (has_graphic_blob[ocz+1][ocx+1] & (1 << ocy)))
            continue;

          /* Need a face here */
          /* First make sure there's room */
          if (num_faces >= MAX_FACES) goto face_generation_done;

          if (!graphic)
            graphic = env_vmap_manifold_renderer_get_graphic_blob(
              r->graphics, r->vmap, x, y, z, lod);

          faces[num_faces].graphic = graphic->ordinal;
          faces[num_faces].is_extraneous =
            (-1 == cx || MHIVE_SZ>>lod == cx ||
             -1 == cz || MHIVE_SZ>>lod == cz);
          if (!faces[num_faces].is_extraneous)
            graphic_blobs[graphic->ordinal] = graphic;

          for (cv = 0; cv < 4; ++cv) {
            vcx = cx + voxel_checks[ck].v[cv].rx;
            vcy = cy + voxel_checks[ck].v[cv].ry;
            vcz = cz + voxel_checks[ck].v[cv].rz;

            /* Generate the vertex if needed */
            if (0xFFFF == vertex_indices[vcz+1][vcx+1][vcy]) {
              /* Abort if insufficient space */
              if (num_vertices >= MAX_VERTICES)
                goto face_generation_done;

              if (!~base_y[vcz+1][vcx+1]) {
                vx = (((vcx<<lod) + x0) & xmask) * TILE_SZ;
                vz = (((vcz<<lod) + z0) & zmask) * TILE_SZ;
                base_y[vcz+1][vcx+1] = (*r->get_y_offset)(
                  r->base_object, vx, vz);
              }

              /* Exclude shared offsets (x0, z0, base_coordinate) from the
               * vertices so we don't lose FP precision. They'll be added back
               * in by the vertex shader.
               */
              vertices[num_vertices] =
                sse_psof((vcx<<lod) * TILE_SZ,
                         (vcy<<lod) * TILE_SZ + base_y[vcz+1][vcx+1],
                         (vcz<<lod) * TILE_SZ, 1.0f);
              vertex_indices[vcz+1][vcx+1][vcy] = num_vertices++;
            }

            faces[num_faces].vertices[cv] = vertex_indices[vcz+1][vcx+1][vcy];
          }

          /* Ensure vertex adjacencies are recorded */
          for (cv = 0; cv < 4; ++cv) {
            haystack = faces[num_faces].vertices[cv];

            needle = faces[num_faces].vertices[(cv+1) & 3];
            for (i = 0; ; ++i) {
              assert(i < lenof(vertex_adjacency[haystack]));

              if (needle == vertex_adjacency[haystack][i])
                break;

              if (0xFFFF == vertex_adjacency[haystack][i]) {
                vertex_adjacency[haystack][i] = needle;
                break;
              }
            }

            needle = faces[num_faces].vertices[(cv-1) & 3];
            for (i = 0; ; ++i) {
              assert(i < lenof(vertex_adjacency[haystack]));

              if (needle == vertex_adjacency[haystack][i])
                break;

              if (0xFFFF == vertex_adjacency[haystack][i]) {
                vertex_adjacency[haystack][i] = needle;
                break;
              }
            }
          }

          ++num_faces;
        }
      }
    }
  }

  face_generation_done:

  /* TODO: Subdivision */

  num_graphic_blobs = 0;
  for (i = 0; i < lenof(graphic_blobs); ++i)
    if (graphic_blobs[i])
      ++num_graphic_blobs;

  mhive = xmalloc(offsetof(env_vmap_manifold_render_mhive, operations) +
                  num_graphic_blobs *
                  sizeof(env_vmap_manifold_render_operation));
  mhive->num_operations = num_graphic_blobs;
  mhive->lod = lod;
  mhive->base_coordinate[0] = x0 * TILE_SZ + r->base_coordinate[0];
  mhive->base_coordinate[1] = r->base_coordinate[1];
  mhive->base_coordinate[2] = z0 * TILE_SZ + r->base_coordinate[2];
  glGenBuffers(2, mhive->buffers);

  op = 0;
  for (i = 0; i < lenof(graphic_blobs); ++i) {
    if (graphic_blobs[i]) {
      mhive->operations[op].graphic = graphic_blobs[i];
      mhive->operations[op].offset = num_triangulated_indices;

      for (ck = 0; ck < num_faces; ++ck) {
        if (i == faces[ck].graphic && !faces[ck].is_extraneous) {
          triangulated_indices[num_triangulated_indices++] = faces[ck].vertices[0];
          triangulated_indices[num_triangulated_indices++] = faces[ck].vertices[1];
          triangulated_indices[num_triangulated_indices++] = faces[ck].vertices[2];
          triangulated_indices[num_triangulated_indices++] = faces[ck].vertices[0];
          triangulated_indices[num_triangulated_indices++] = faces[ck].vertices[2];
          triangulated_indices[num_triangulated_indices++] = faces[ck].vertices[3];
        }
      }

      mhive->operations[op].length =
        num_triangulated_indices - mhive->operations[op].offset;

      ++op;
    }
  }

  glBindBuffer(GL_ARRAY_BUFFER, mhive->buffers[0]);
  glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, mhive->buffers[1]);
  glBufferData(GL_ARRAY_BUFFER, num_vertices * sizeof(vertices[0]),
               vertices, GL_STATIC_DRAW);
  glBufferData(GL_ELEMENT_ARRAY_BUFFER,
               num_triangulated_indices * sizeof(triangulated_indices[0]),
               triangulated_indices, GL_STATIC_DRAW);
  return mhive;
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
  const rendering_context_invariant*restrict context = CTXTINV(ctxt);
  shader_manifold_uniform uniform;
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
    effective_camera -= mhive->base_coordinate[i];
    switch (i) {
    case 0:
      effective_camera &= context->proj->torus_w - 1;
      break;
    case 1:
      break;
    case 2:
      effective_camera &= context->proj->torus_h - 1;
      break;
    }

    uniform.camera_integer[i]    = effective_camera & 0xFFFF0000;
    uniform.camera_fractional[i] = effective_camera & 0x0000FFFF;
  }

  glBindBuffer(GL_ARRAY_BUFFER, mhive->buffers[0]);
  glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, mhive->buffers[1]);

  for (i = 0; i < mhive->num_operations; ++i) {
    shader_manifold_activate(&uniform);
    shader_manifold_configure_vbo();
    glDrawElements(GL_TRIANGLES, mhive->operations[i].length,
                   GL_UNSIGNED_SHORT,
                   (GLvoid*)(mhive->operations[i].offset * sizeof(unsigned short)));
  }
}
