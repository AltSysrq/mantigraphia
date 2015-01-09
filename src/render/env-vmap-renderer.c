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
#include "env-vmap-renderer.h"

#define CELL_SZ ENV_VMAP_RENDERER_CELL_SZ
#define DRAW_DISTANCE 16 /* cells */

/**
 * A render operation is a set of polygons inside a render cell which share the
 * same graphic plane.
 */
typedef struct {
  const env_voxel_graphic_plane* graphic;
  /**
   * The offset of the first index to render, and the number of indices in this
   * set.
   */
  unsigned offset, length;
} env_vmap_render_operation;

/**
 * A render cell holds on-GPU precomputed data used to render some part
 * (possibly all) of an env_vmap. It is composed of zero or more operations;
 * all operations share the same vertex and index buffers.
 *
 * Render cells are created on-demand and destroyed when no longer needed.
 */
struct env_vmap_render_cell_s {
  /**
   * The level of detail of this cell (0 = maximum).
   */
  unsigned char lod;
  /**
   * The vertex and index buffers for this cell.
   */
  GLuint buffers[2];
  /**
   * The length of the operations array.
   */
  unsigned num_operations;
  env_vmap_render_operation operations[FLEXIBLE_ARRAY_MEMBER];
};

typedef struct {
  env_vmap_renderer*restrict this;
  const rendering_context*restrict ctxt;
} env_vmap_render_op;

typedef struct env_voxel_graphic_presence_s {
  const env_voxel_graphic_plane* graphic;
  unsigned num_tiles;
  unsigned index_offset;

  SPLAY_ENTRY(env_voxel_graphic_presence_s) set;
} env_voxel_graphic_presence;
SPLAY_HEAD(env_voxel_graphic_presence_set,env_voxel_graphic_presence_s);

static inline int compare_env_voxel_graphic_presence(
  const env_voxel_graphic_presence* a,
  const env_voxel_graphic_presence* b
) {
  return (a->graphic < b->graphic) - (b->graphic < a->graphic);
}
SPLAY_PROTOTYPE(env_voxel_graphic_presence_set, env_voxel_graphic_presence_s,
                set, compare_env_voxel_graphic_presence);
SPLAY_GENERATE(env_voxel_graphic_presence_set, env_voxel_graphic_presence_s,
               set, compare_env_voxel_graphic_presence);

static const env_voxel_graphic* env_vmap_renderer_get_graphic(
  const env_vmap_renderer* this, coord x, coord y, coord z,
  unsigned char lod);
static env_vmap_render_cell* env_vmap_render_cell_new(
  const env_vmap_renderer* parent, coord x0, coord z0,
  unsigned char lod);
static void env_vmap_render_cell_delete(env_vmap_render_cell*);
static void render_env_vmap_impl(env_vmap_render_op*);
static void env_vmap_render_cell_render(const env_vmap_render_cell*restrict,
                                        const rendering_context*restrict);

env_vmap_renderer* env_vmap_renderer_new(
  const env_vmap* vmap,
  const env_voxel_graphic*const graphics[NUM_ENV_VOXEL_CONTEXTUAL_TYPES],
  const vc3 base_coordinate,
  const void* base_object,
  coord (*get_y_offset)(const void*, coord, coord)
) {
  size_t cells_sz = sizeof(env_vmap_render_cell*) *
    (vmap->xmax / CELL_SZ) * (vmap->zmax / CELL_SZ);
  env_vmap_renderer* this = xmalloc(
    offsetof(env_vmap_renderer, cells) +
    cells_sz);

  this->vmap = vmap;
  this->graphics = graphics;
  memcpy(this->base_coordinate, base_coordinate, sizeof(vc3));
  this->base_object = base_object;
  this->get_y_offset = get_y_offset;
  memset(this->cells, 0, cells_sz);

  return this;
}

void env_vmap_renderer_delete(env_vmap_renderer* this) {
  unsigned num_cells =
    this->vmap->xmax / CELL_SZ * this->vmap->zmax / CELL_SZ;
  unsigned i;

  for (i = 0; i < num_cells; ++i)
    if (this->cells[i])
      env_vmap_render_cell_delete(this->cells[i]);

  free(this);
}

void render_env_vmap(canvas* dst,
                     env_vmap_renderer*restrict this,
                     const rendering_context*restrict ctxt) {
  env_vmap_render_op* op = xmalloc(sizeof(env_vmap_render_op));

  op->this = this;
  op->ctxt = ctxt;
  glm_do((void(*)(void*))render_env_vmap_impl, op);
}

void render_env_vmap_impl(env_vmap_render_op* op) {
  env_vmap_renderer* this = op->this;
  const rendering_context*restrict ctxt = op->ctxt;
  const rendering_context_invariant*restrict context = CTXTINV(ctxt);
  unsigned x, z, xmax, zmax, cx, cz;
  signed dx, dz;
  unsigned d;
  unsigned char desired_lod;

  free(op);

  xmax = this->vmap->xmax / CELL_SZ;
  zmax = this->vmap->zmax / CELL_SZ;
  cx = context->proj->camera[0] / TILE_SZ / CELL_SZ;
  cz = context->proj->camera[2] / TILE_SZ / CELL_SZ;

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

        if (this->cells[z*xmax + x] &&
            this->cells[z*xmax + x]->lod != desired_lod) {
          env_vmap_render_cell_delete(this->cells[z*xmax + x]);
          this->cells[z*xmax + x] = NULL;
        }

        if (!this->cells[z*xmax + x])
          this->cells[z*xmax + x] = env_vmap_render_cell_new(
            this, x*CELL_SZ, z*CELL_SZ, desired_lod);

        env_vmap_render_cell_render(this->cells[z*xmax + x], ctxt);
      } else {
        if (this->cells[z*xmax + x]) {
          env_vmap_render_cell_delete(this->cells[z*xmax + x]);
          this->cells[z*xmax + x] = NULL;
        }
      }
    }
  }
}

/* This uses an O(n) algorithm to find the majority component as an
 * approximation of the statistical mode of a cell (read in as a single qword).
 *
 * Note that processors of different endianness will produce slightly different
 * results in cases where there is no majority component. Since this is only
 * used for graphics, this isn't really an issue.
 */
static unsigned env_vmap_renderer_ll_majority_component(
  unsigned long long bytes
) {
  unsigned m, n, b;

  if (!bytes) return 0;

  m = bytes & 0xFF;
  n = 1;
#define BYTE()                                  \
  bytes >>= 8;                                  \
  b = bytes & 0xFF;                             \
  if (m == b) ++n;                              \
  else if (!--n) m = b, n = 1
  BYTE(); /* 1 */
  BYTE(); /* 2 */
  BYTE(); /* 3 */
  BYTE(); /* 4 */
  BYTE(); /* 5 */
  BYTE(); /* 6 */
  BYTE(); /* 7 */
#undef BYTE
  return m;
}

static const env_voxel_graphic* env_vmap_renderer_get_graphic(
  const env_vmap_renderer* this, coord x, coord y, coord z,
  unsigned char lod
) {
  const unsigned long long* cell;

  switch (lod) {
  case 0:
    return this->graphics[env_vmap_expand_type(this->vmap, x, y, z)];

  case 1:
  case 2:
  case 3:
    cell = (unsigned long long*)
      (this->vmap->voxels + env_vmap_offset(this->vmap, x, y, z));
    return this->graphics[
      env_vmap_renderer_ll_majority_component(*cell) <<
      ENV_VOXEL_CONTEXT_BITS];
  }

  /* unreachable */
  abort();
}

static env_vmap_render_cell* env_vmap_render_cell_new(
  const env_vmap_renderer* r, coord x0, coord z0,
  unsigned char lod
) {
  /* Vertices are handed to the GPU all in one buffer, approximately in
   * ascending (Z,X,Y) order, rotating between those for X plane polygons, then
   * Y, then Z. (Polygons for different planes never share vertices.)
   * Obviously, we don't want to supply *all possible* vertices (about 31MB,
   * the vast majority of which would be unused), so only vertices that will
   * actually be used are sent to the GPU.
   *
   * The two-pass algorthim works as follows:
   *
   * State:
   * - A vertex array big enough to hold every possible vertex is allocated,
   *   and the number of elements used therein tracked, starting at zero.
   * - An array, indexed by (Z,X,Y,plane), is created, holding indices into
   *   the vertex array for the vertex data at each location. Values are set to
   *   ~0 to indicate that a vertex for that index has not yet been generated.
   * - An array of graphic_presence elements, big enough to hold the worst case
   *   of NUM_ENV_VOXEL_CONTEXTUAL_TYPES distinct graphics being encountered,
   *   is allocated, and an empty splay tree referencing it is prepared. The
   *   size of this tree is also tracked.
   *
   * Pass 1: Discovery
   * The voxels covered by this cell are iterated in (Z,X,Y) order. If a voxel
   * plane has an associated graphic, the graphic splay tree is traversed, and
   * a new node created if there isn't one already. If the plane requires
   * vertices that don't have indices yet, they are generated at the end of the
   * vertex array. In any case, the number of tiles used by the graphic plane
   * is incremented.
   *
   * After the discovery pass, the number of vertices, indices, and operations
   * is known exactly, so memory for the index buffer can be allocated, as well
   * as the cell itself. Ranges within the index buffer are assigned to each
   * operation.
   *
   * Pass 2: Index Generation
   * The voxels are retraversed as in pass 1. When a plane is to be drawn, the
   * indices for its component vertices are looked up in the index table, and
   * appended to that graphic's section of the index buffer.
   *
   * After pass 2, the used portion of the vertex and index buffers can be sent
   * to the GPU, and then all the temporary memory discarded.
   *
   * Since this is all run on the OpenGL thread, there will never be more than
   * one concurrent invocation of the function per process, so the temporary
   * data can just live in the data segment as static variables.
   */
#define NVXZ (CELL_SZ+1) /* +1 for terminal fencepost */
#define NVY (ENV_VMAP_H+1)
  static unsigned vertex_indices[NVXZ][NVXZ][NVY][3];
  static shader_voxel_vertex vertex_data[NVXZ*NVXZ*NVY*6];
  static unsigned index_data[CELL_SZ*CELL_SZ*ENV_VMAP_H*18];
  static env_voxel_graphic_presence graphic_presence[
    NUM_ENV_VOXEL_CONTEXTUAL_TYPES];

  struct env_voxel_graphic_presence_set graphics;
  unsigned vertex_data_len = 0, graphic_presence_len = 0;
  unsigned required_indices = 0;

  coord x, y, z;
  vc3 pos;
  unsigned i, v, index_off;
  const env_voxel_graphic* graphic;
  env_voxel_graphic_presence gp_example, * gp;
  env_vmap_render_cell* cell;

  SPLAY_INIT(&graphics);
  memset(vertex_indices, ~0, sizeof(vertex_indices));

  /* PASS 1 */
  for (z = 0; z < (unsigned)(CELL_SZ >> lod); ++z) {
    for (x = 0; x < (unsigned)(CELL_SZ >> lod); ++x) {
      for (y = 0; y < (unsigned)(ENV_VMAP_H >> lod); ++y) {
        if (lod && !env_vmap_is_visible(
              r->vmap, x0+(x<<lod), y<<lod, z0+(z<<lod), lod))
          continue;

        graphic = env_vmap_renderer_get_graphic(
          r, x0+(x<<lod), y<<lod, z0+(z<<lod), lod);
        if (!graphic) continue;

        if (graphic->planes[0]) {
#define VERTEX(vx,vy,vz,p)                                              \
          do {                                                          \
            if (!~vertex_indices[vz][vx][vy][p]) {                      \
              vertex_indices[vz][vx][vy][p] = v = vertex_data_len++;    \
              pos[0] = (((vx)<<lod)+x0)*TILE_SZ + (0==p)*TILE_SZ/2;     \
              pos[2] = (((vz)<<lod)+z0)*TILE_SZ + (2==p)*TILE_SZ/2;     \
              pos[1] = ((vy)<<lod)*TILE_SZ + (1==p)*TILE_SZ/2 +         \
                (*r->get_y_offset)(r->base_object, pos[0], pos[2]);     \
              vertex_data[v].v[0] = pos[0] + r->base_coordinate[0];     \
              vertex_data[v].v[1] = pos[1] + r->base_coordinate[1];     \
              vertex_data[v].v[2] = pos[2] + r->base_coordinate[2];     \
              vertex_data[v].tc[0] = (0==p? vy : vx) << lod;            \
              vertex_data[v].tc[1] = (0==p || 1==p? vz : vy) << lod;    \
            }                                                           \
          } while (0)

          VERTEX(x,y+0,z+0,0);
          VERTEX(x,y+1,z+0,0);
          VERTEX(x,y+1,z+1,0);
          VERTEX(x,y+0,z+1,0);

#define ADDPLANE(p)                                             \
          do {                                                  \
            gp_example.graphic = graphic->planes[p];            \
            gp = SPLAY_FIND(env_voxel_graphic_presence_set,     \
                            &graphics, &gp_example);            \
            if (!gp) {                                          \
              gp = graphic_presence + graphic_presence_len++;   \
              gp->graphic = graphic->planes[p];                 \
              gp->num_tiles = 0;                                \
              SPLAY_INSERT(env_voxel_graphic_presence_set,      \
                           &graphics, gp);                      \
            }                                                   \
            ++gp->num_tiles;                                    \
            required_indices += 6;                              \
          } while (0)

          ADDPLANE(0);
        }

        if (graphic->planes[1]) {
          VERTEX(x+0,y,z+0,1);
          VERTEX(x+1,y,z+0,1);
          VERTEX(x+1,y,z+1,1);
          VERTEX(x+0,y,z+1,1);
          ADDPLANE(1);
        }

        if (graphic->planes[2]) {
          VERTEX(x+0,y+0,z,2);
          VERTEX(x+1,y+0,z,2);
          VERTEX(x+1,y+1,z,2);
          VERTEX(x+0,y+1,z,2);
          ADDPLANE(2);
        }
      }
    }
  }

  /* Set up for pass 2 */
  cell = xmalloc(offsetof(env_vmap_render_cell, operations) +
                 graphic_presence_len * sizeof(env_vmap_render_operation));
  cell->num_operations = graphic_presence_len;
  cell->lod = lod;
  glGenBuffers(2, cell->buffers);

  index_off = 0;
  i = 0;
  SPLAY_FOREACH(gp, env_voxel_graphic_presence_set, &graphics) {
    cell->operations[i].graphic = gp->graphic;
    cell->operations[i].offset = index_off;
    cell->operations[i].length = 6 * gp->num_tiles;
    gp->index_offset = index_off;

    index_off += 6 * gp->num_tiles;
    ++i;
  }

  /* PASS 2 */
  for (z = 0; z < (unsigned)(CELL_SZ >> lod); ++z) {
    for (x = 0; x < (unsigned)(CELL_SZ >> lod); ++x) {
      for (y = 0; y < (unsigned)(ENV_VMAP_H >> lod); ++y) {
        if (lod && !env_vmap_is_visible(
              r->vmap, x0+(x<<lod), y<<lod, z0+(z<<lod), lod))
          continue;

        graphic = env_vmap_renderer_get_graphic(
          r, x0+(x<<lod), y<<lod, z0+(z<<lod), lod);
        if (!graphic) continue;

#define DOPLANE(p)                                              \
        do {                                                    \
          if (graphic->planes[p]) {                             \
            gp_example.graphic = graphic->planes[p];            \
            gp = SPLAY_FIND(env_voxel_graphic_presence_set,     \
                            &graphics, &gp_example);            \
            index_data[gp->index_offset++] =                    \
              vertex_indices[z][x][y][p];                       \
            index_data[gp->index_offset++] =                    \
              vertex_indices[z][x+(0!=p)][y+(0==p)][p];         \
            index_data[gp->index_offset++] =                    \
              vertex_indices[z+(2!=p)][x][y+(2==p)][p];         \
            index_data[gp->index_offset++] =                    \
              vertex_indices[z][x+(0!=p)][y+(0==p)][p];         \
            index_data[gp->index_offset++] =                    \
              vertex_indices[z+(2!=p)][x][y+(2==p)][p];         \
            index_data[gp->index_offset++] =                    \
              vertex_indices[z+(2!=p)][x+(0!=p)][y+(1!=p)][p];  \
          }                                                     \
        } while (0)

        DOPLANE(0);
        DOPLANE(1);
        DOPLANE(2);
      }
    }
  }

  /* Send stuff to GL and we're done */
  glBindBuffer(GL_ARRAY_BUFFER, cell->buffers[0]);
  glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, cell->buffers[1]);
  glBufferData(GL_ARRAY_BUFFER, vertex_data_len * sizeof(vertex_data[0]),
               vertex_data,  GL_STATIC_DRAW);
  glBufferData(GL_ELEMENT_ARRAY_BUFFER, required_indices * sizeof(unsigned),
               index_data, GL_STATIC_DRAW);

  return cell;
}

static void env_vmap_render_cell_delete(env_vmap_render_cell* this) {
  glDeleteBuffers(2, this->buffers);
  free(this);
}

static inline float zo_float(zo_scaling_factor f) {
  return f / (float)ZO_SCALING_FACTOR_MAX;
}

static void env_vmap_render_cell_render(
  const env_vmap_render_cell*restrict cell,
  const rendering_context*restrict ctxt
) {
  const rendering_context_invariant*restrict context = CTXTINV(ctxt);
  shader_voxel_uniform uniform;
  unsigned i;

  uniform.palette_t = context->month_integral / 9.0f +
    context->month_fraction / (float)fraction_of(1) / 9.0f;
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
    uniform.camera_integer[i]    = context->proj->camera[i] & 0xFFFF0000;
    uniform.camera_fractional[i] = context->proj->camera[i] & 0x0000FFFF;
  }
  uniform.tex = 0;
  uniform.palette = 1;

  glBindBuffer(GL_ARRAY_BUFFER, cell->buffers[0]);
  glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, cell->buffers[1]);

  for (i = 0; i < cell->num_operations; ++i) {
    glBindTexture(GL_TEXTURE_2D, cell->operations[i].graphic->texture);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST_MIPMAP_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT);
    glActiveTexture(GL_TEXTURE1);
    glBindTexture(GL_TEXTURE_2D, cell->operations[i].graphic->palette);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    glActiveTexture(GL_TEXTURE0);

    uniform.texture_scale[0] =
      cell->operations[i].graphic->texture_scale[0] / 65536.0f;
    uniform.texture_scale[1] =
      cell->operations[i].graphic->texture_scale[1] / 65536.0f;

    shader_voxel_activate(&uniform);
    shader_voxel_configure_vbo();
    glDrawElements(GL_TRIANGLES, cell->operations[i].length,
                   GL_UNSIGNED_INT,
                   (GLvoid*)(cell->operations[i].offset * sizeof(unsigned)));
  }
}
