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

#include <SDL.h>

#include <assert.h>
#include <stdlib.h>
#include <string.h>
#include <stddef.h>

#include "bsd.h"
#include "../alloc.h"
#include "../defs.h"
#include "../micromp.h"
#include "../math/coords.h"
#include "../math/sse.h"
#include "../math/rand.h"
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
#define NOISETEX_SZ 64

/**
 * The data-intensive operations use static storage for their temporary data.
 * This both makes programming simpler, and makes it easier to ensure that that
 * memory is available. It also permits a few GCC optimisations that otherwise
 * wouldn't happen.
 *
 * To permit multi-threading, each of these is duplicated THREADS times,
 * permitting up to THREADS threads to run at once.
 */
#define THREADS 4

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
   * The VAO for this mhive.
   */
  GLuint vao;
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

static env_vmap_manifold_render_mhive* env_vmap_manifold_render_mhive_new(
  const env_vmap_manifold_renderer* parent, coord x0, coord z0,
  unsigned char lod, unsigned thread_ordinal);
static void env_vmap_manifold_render_mhive_delete(env_vmap_manifold_render_mhive*);
static void env_vmap_manifold_render_mhive_render(
  const env_vmap_manifold_render_mhive*restrict,
  const rendering_context*restrict);

env_vmap_manifold_renderer* env_vmap_manifold_renderer_new(
  const env_vmap* vmap,
  const env_voxel_graphic*const graphics[NUM_ENV_VOXEL_TYPES],
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

static void render_env_vmap_manifolds_glprepare(void* ignored) {
  glPushAttrib(GL_ENABLE_BIT);
  glEnable(GL_CULL_FACE);
}

static void render_env_vmap_manifolds_glfinish(void* ignore) {
  glPopAttrib();
}

typedef struct {
  GLuint* vao;
  GLuint* buffers;
  void* vertex_data;
  unsigned vertex_data_size;
  void* index_data;
  unsigned index_data_size;
  SDL_sem* not_busy;
} render_env_vmap_manifolds_put_buffer_data_op;

static void render_env_vmap_manifolds_put_buffer_data(
  render_env_vmap_manifolds_put_buffer_data_op* op
) {
  glGenVertexArrays(1, op->vao);
  glGenBuffers(2, op->buffers);
  glBindVertexArray(*op->vao);
  glBindBuffer(GL_ARRAY_BUFFER, op->buffers[0]);
  glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, op->buffers[1]);
  glBufferData(GL_ARRAY_BUFFER, op->vertex_data_size,
               op->vertex_data, GL_STATIC_DRAW);
  glBufferData(GL_ELEMENT_ARRAY_BUFFER, op->index_data_size,
               op->index_data, GL_STATIC_DRAW);

  if (SDL_SemPost(op->not_busy))
    errx(EX_SOFTWARE, "Failed to post semaphore for manifold thread: %s",
         SDL_GetError());

  shader_manifold_configure_vbo();
}

static void render_env_vmap_manifolds_impl(
  unsigned thread_ordinal, unsigned threads);

static canvas* render_env_vmap_manifolds_dst;
static env_vmap_manifold_renderer*restrict render_env_vmap_manifolds_this;
static const rendering_context*restrict render_env_vmap_manifolds_ctxt;
static ump_task render_env_vmap_manifolds_task = {
  render_env_vmap_manifolds_impl,
  THREADS,
  0, /* sync */
};

void render_env_vmap_manifolds(
  canvas* dst,
  env_vmap_manifold_renderer*restrict this,
  const rendering_context*restrict ctxt
) {
  glm_do(render_env_vmap_manifolds_glprepare, NULL);

  render_env_vmap_manifolds_dst = dst;
  render_env_vmap_manifolds_this = this;
  render_env_vmap_manifolds_ctxt = ctxt;
  ump_run_sync(&render_env_vmap_manifolds_task);

  glm_do(render_env_vmap_manifolds_glfinish, NULL);
}

static void render_env_vmap_manifolds_impl(
  unsigned thread_ordinal, unsigned ignore
) {
  env_vmap_manifold_renderer*restrict this = render_env_vmap_manifolds_this;
  const rendering_context*restrict ctxt = render_env_vmap_manifolds_ctxt;

  const rendering_context_invariant*restrict context = CTXTINV(ctxt);
  unsigned x, z, xmax, zmax, cx, cz;
  signed dx, dz;
  unsigned d;
  signed dot;
  unsigned char desired_lod;

  xmax = this->vmap->xmax / MHIVE_SZ;
  zmax = this->vmap->zmax / MHIVE_SZ;
  cx = context->proj->camera[0] / TILE_SZ / MHIVE_SZ;
  cz = context->proj->camera[2] / TILE_SZ / MHIVE_SZ;

  for (z = 0; z < zmax; ++z) {
    for (x = 0; x < xmax; ++x) {
      if ((x + z) % THREADS != thread_ordinal)
        continue;

      dx = x - cx;
      dz = z - cz;
      if (this->vmap->is_toroidal) {
        dx = torus_dist(dx, xmax);
        dz = torus_dist(dz, zmax);
      }

      d = umax(abs(dx), abs(dz));

      if (d < DRAW_DISTANCE) {
        if (d <= DRAW_DISTANCE/4)
          desired_lod = 0;
        else if (d < DRAW_DISTANCE/2)
          desired_lod = 1;
        else
          desired_lod = 2;

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
            this, x*MHIVE_SZ, z*MHIVE_SZ, desired_lod, thread_ordinal);

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

#define MAX_VERTICES 65535
#define MAX_FACES 65535
#define MAX_FACES_PER_VERTEX 12
#define MAX_EDGES_PER_VERTEX 8
#define NVX (5+MHIVE_SZ)
#define NVY (1+ENV_VMAP_H)
#define NVZ (5+MHIVE_SZ)

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

static unsigned record_vertex_link(
  unsigned short vertex_adjacency[MAX_VERTICES][MAX_EDGES_PER_VERTEX],
  unsigned short a,
  unsigned short b
) {
  unsigned i;

  for (i = 0; ; ++i) {
    assert(i < lenof(vertex_adjacency[a]));
    /* If the link already exists, we know it's mutual, so stop immediately. */
    if (b == vertex_adjacency[a][i]) return 0;
    /* Check for free slot */
    if (0xFFFF == vertex_adjacency[a][i])
      break;
  }
  vertex_adjacency[a][i] = b;

  for (i = 0; ; ++i) {
    assert(i < lenof(vertex_adjacency[b]));
    if (0xFFFF == vertex_adjacency[b][i])
      break;
  }
  vertex_adjacency[b][i] = a;

  return 1;
}

static ssepi perturb(ssepi v, unsigned perturbation) {
  ssepi p;
  unsigned chaos = 0;
  signed short xp, yp, zp;

  chaos = chaos_accum(chaos, ((int)SSE_VS(v, 0)) % (MHIVE_SZ*METRE));
  chaos = chaos_accum(chaos, ((int)SSE_VS(v, 1)) % (MHIVE_SZ*METRE));
  chaos = chaos_accum(chaos, ((int)SSE_VS(v, 2)) % (MHIVE_SZ*METRE));
  chaos = chaos_of(chaos);

  xp = lcgrand(&chaos);
  yp = lcgrand(&chaos);
  zp = lcgrand(&chaos);
  p = sse_piof(xp, yp, zp, 0);
  p *= sse_piof1(perturbation);
  p = sse_sradi(p, 15);
  return p;
}

static void catmull_clark_subdivide(
  ssepi vertices[MAX_VERTICES],
  unsigned short vertex_adjacency[MAX_VERTICES][MAX_EDGES_PER_VERTEX],
  manifold_face faces[MAX_FACES],
  const env_voxel_graphic_blob*const* graphics,
  unsigned num_orig_vertices,
  unsigned num_orig_faces,
  unsigned level,
  unsigned thread_ordinal
) {
  /*
    See also
    http://en.wikipedia.org/wiki/Catmull%E2%80%93Clark_subdivision_surface

    The steps in this implementation are as follow:

    - Face points. Face vertices are addressed by original face index, starting
      at num_orig_vertices and extending to num_orig_vertices+num_orig_faces.
      Each face point is positioned at the average of the four original
      vertices comprising its face. Original vertices are associated with lists
      of new midpoints via the new_face_vertices table; entries of ~0 in this
      table are unused.

    - Edge splitting. For each original edge, a new vertex is generated whose
      position is the average of the original vertices of the edge, and the new
      face vertices shared by those two original vertices. Missing face
      vertices are assumed zero, since this will only happen for extraneous
      faces, so the denominator of the average is always 4. Split edges are
      recorded in edge_splits, which is parallel to vertex_adjacency on
      0..num_orig_vertices. As with vertex_adjacency, unused entries are
      indicated with ~0u.

    - Original point adjustment. For each original point, the number of new
      associated face vertices for that vertex is counted, resulting in a
      denominator N. The average F of all these face points is calculated, as
      well as the average R of all newly adjacent midpoints. The point is moved
      to the location (F + 2R + (n-3)P)/n, where P is the original location.

    - Face duplication and spreading. For each face at index i, the elements in
      faces from i*4 up to (i+1)*4 are set to exact copies of the input face.
      The faces are spread like this so as to get better utilisation of OpenGL
      vertex shader caching, since subdivided faces will share vertices.

    - Face separation. For each face at location i*4+k, the following is done:

      - Rotate the vertex array of the face k elements left, and name the four
        elements A,B,C,D.
      - Leave A unchanged.
      - Replace B with the midpoint of edge AB.
      - Replace C with the face point of face i.
      - Replace D with the midpoint of edge DA.
      - Record the new BC, CD adjacencies. The new AB, AD adjacencies will be
        recorded implicitly by copying the used portion of new_face_vertices
        onto the head of vertex_adjacency.
   */
  static struct {
    unsigned short _new_face_vertices[MAX_VERTICES][MAX_FACES_PER_VERTEX];
    unsigned short _edge_splits[MAX_VERTICES][MAX_EDGES_PER_VERTEX];
    volatile unsigned char padding[UMP_CACHE_LINE_SZ];
  } threads[THREADS];
#define new_face_vertices threads[thread_ordinal]._new_face_vertices
#define edge_splits threads[thread_ordinal]._edge_splits

  const ssepi one   = sse_piof1(1), two  = sse_piof1(2),
              three = sse_piof1(3), four = sse_piof1(4),
              zero  = sse_piof1(0);
  ssepi vf, vfn, vr, vrn, vp;
  unsigned num_vertices = num_orig_vertices;
  unsigned i, j, k, l, v;

  memset(new_face_vertices, ~0,
         num_orig_vertices * sizeof(new_face_vertices[0]));
  memset(edge_splits, ~0,
         num_orig_vertices * sizeof(edge_splits[0]));

  /* Face vertices */
  for (i = 0; i < num_orig_faces; ++i) {
    vp = zero;
    for (j = 0; j < 4; ++j) {
      v = faces[i].vertices[j];
      vp = sse_addpi(vp, vertices[v]);
      for (k = 0; ; ++k) {
        assert(k < lenof(new_face_vertices[v]));
        if (0xFFFF == new_face_vertices[v][k]) {
          new_face_vertices[v][k] = num_vertices;
          break;
        }
      }
    }
    vp = sse_divpi(vp, four);
    vp = sse_addpi(
      vp, perturb(vp, graphics[faces[i].graphic]->perturbation >> level));
    vertices[num_vertices++] = vp;
  }

  /* Edge splits */
  for (i = 0; i < num_orig_vertices; ++i) {
    for (j = 0; j < lenof(vertex_adjacency[i]); ++j) {
      if (0xFFFF == vertex_adjacency[i][j]) break;
      /* Make sure we haven't already split this edge */
      if (0xFFFF != edge_splits[i][j]) continue;

      /* Ok, split this edge */
      v = vertex_adjacency[i][j];
      vp = vertices[i];
      vp = sse_addpi(vp, vertices[v]);
      vrn = two;
      for (k = 0; k < lenof(new_face_vertices[i]); ++k) {
        if (0xFFFF == new_face_vertices[i][k]) break;
        for (l = 0; l < lenof(new_face_vertices[v]); ++l) {
          if (0xFFFF == new_face_vertices[v][l]) break;

          if (new_face_vertices[i][k] == new_face_vertices[v][l]) {
            vp = sse_addpi(vp, vertices[new_face_vertices[i][k]]);
            vrn = sse_addpi(vrn, one);
          }
        }
      }

      vertices[num_vertices] = sse_divpi(vp, vrn);

      /* Record split */
      edge_splits[i][j] = num_vertices;
      for (k = 0; k < lenof(vertex_adjacency[v]); ++k) {
        if (i == vertex_adjacency[v][k]) {
          edge_splits[v][k] = num_vertices;
          break;
        }
      }

      ++num_vertices;
    }
  }

  /* Original point adjustment */
  for (i = 0; i < num_orig_vertices; ++i) {
    vp = vertices[i];

    vf = zero;
    vfn = zero;
    for (j = 0; j < lenof(new_face_vertices[i]); ++j) {
      if (0xFFFF == new_face_vertices[i][j]) break;

      vf = sse_addpi(vf, vertices[new_face_vertices[i][j]]);
      vfn = sse_addpi(vfn, one);
    }
    vf = sse_divpi(vf, vfn);

    vr = zero;
    vrn = zero;
    for (j = 0; j < lenof(edge_splits[i]); ++j) {
      if (0xFFFF == edge_splits[i][j]) break;

      vr = sse_addpi(vr, vertices[edge_splits[i][j]]);
      vrn = sse_addpi(vrn, one);
    }
    vr = sse_divpi(vr, vrn);

    vertices[i] = sse_divpi(
      sse_addpi(vf, sse_addpi(sse_mulpi(two, vr),
                              sse_mulpi(sse_subpi(vfn, three), vp))),
      vfn);
  }

  /* Initialise new adjacencies */
  memset(vertex_adjacency + num_orig_vertices, ~0,
         (num_vertices - num_orig_vertices) * sizeof(vertex_adjacency[0]));

  /* Duplicate/spread and separate faces */
  for (i = num_orig_faces - 1; i < num_orig_faces; --i) {
    for (j = 3; j < 4; --j) {
      faces[i*4+j] = faces[i];

#define A (faces[i*4+j].vertices[(j+0)&3])
#define B (faces[i*4+j].vertices[(j+1)&3])
#define C (faces[i*4+j].vertices[(j+2)&3])
#define D (faces[i*4+j].vertices[(j+3)&3])
      /* Replace B with the edge point between A and B */
      for (k = 0; ; ++k) {
        assert(k < lenof(vertex_adjacency[A]));
        if (B == vertex_adjacency[A][k])
          break;
      }
      B = edge_splits[A][k];

      /* Replace C with the midpoint created for this original face */
      C = num_orig_vertices + i;

      /* Replace D with the edge point between A and D */
      for (k = 0; ; ++k) {
        assert(k < lenof(vertex_adjacency[A]));
        if (D == vertex_adjacency[A][k])
          break;
      }
      D = edge_splits[A][k];

      /* Record B->C, C->B, C->D, D->C links */
      record_vertex_link(vertex_adjacency, B, C);
      record_vertex_link(vertex_adjacency, C, D);
#undef D
#undef C
#undef B
#undef A
    }
  }

  /* Record links from original vertices to edge midpoints */
  memset(vertex_adjacency, ~0, num_orig_vertices * sizeof(vertex_adjacency[0]));
  for (i = 0; i < num_orig_vertices; ++i) {
    for (k = 0; k < lenof(edge_splits[i]); ++k) {
      if (0xFFFF == edge_splits[i][k]) break;
      record_vertex_link(vertex_adjacency, i, edge_splits[i][k]);
    }
  }

#undef edge_splits
#undef new_face_vertices
}

static env_vmap_manifold_render_mhive* env_vmap_manifold_render_mhive_new(
  const env_vmap_manifold_renderer* r, coord x0, coord z0,
  unsigned char lod,
  unsigned thread_ordinal
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
    correctly, faces are generated for voxels two coordinates _outside_ the
    mhive. These faces are considered extraneous, and should not be included in
    rendering proper, since their subdivision is not fully defined, and they
    are rendered by other mhives anyway. However, due to precision issues in
    the subdivision (likely because it isn't truly a manifold, eg, it can be
    self intersecting or have degenerate shared edges), excluding all
    extraneous faces results in occasional gaps; to patch around this, the
    nearer extraneous face is rendered anyway. This trades the gaps for
    occasional Z-fighting since the extraneous face will take a slightly
    different path, but this is not generally noticeable after the paint
    overlay effect has been applied.

    Faces are generated into the `faces` array linearly, and are not spatially
    addressible; they do not store their own adjacencies. They do track the
    ordinal of their graphic blob, which is the graphic blob of the vertex from
    which they were generated.

    Adjacencies for vertices are stored in the `vertex_adjacency` array,
    indexed by vertex index. Elements for each vertex are in no particular
    order; values of ~0 indicate an empty slot.
   */
  static struct {
    coord _base_y[NVZ][NVX];
    ssepi _svertices[MAX_VERTICES];
    float _glvertices[MAX_VERTICES][4];
    unsigned short _vertex_indices[NVZ][NVX][NVY];
    unsigned short _vertex_adjacency[MAX_VERTICES][MAX_EDGES_PER_VERTEX];
    manifold_face _faces[MAX_FACES];
    unsigned short _triangulated_indices[MAX_FACES*6];
    /* A bitset indicating whether each voxel in the column [z][x] has a
     * graphic blob.
     *
     * This is rather dependent on ENV_VMAP_H being 32.
     */
    unsigned _has_graphic_blob[NVZ][NVX];
    /* The minimum Y coordinate (in voxels) in each column which receives
     * light. A generated face is lit if the Y offset of the empty adjacent
     * voxel is greater than this value.
     */
    signed char _light_y[NVZ][NVX];

    /* To track whether the data in glvertices has finished being sent to the
     * GPU.
     */
    SDL_sem* not_busy;

    render_env_vmap_manifolds_put_buffer_data_op _glm_op;

    volatile unsigned char padding[UMP_CACHE_LINE_SZ];
  } threads[THREADS];
#define base_y threads[thread_ordinal]._base_y
#define svertices threads[thread_ordinal]._svertices
#define glvertices threads[thread_ordinal]._glvertices
#define vertex_indices threads[thread_ordinal]._vertex_indices
#define vertex_adjacency threads[thread_ordinal]._vertex_adjacency
#define faces threads[thread_ordinal]._faces
#define triangulated_indices threads[thread_ordinal]._triangulated_indices
#define has_graphic_blob threads[thread_ordinal]._has_graphic_blob
#define light_y threads[thread_ordinal]._light_y
#define glm_op threads[thread_ordinal]._glm_op

  unsigned short num_vertices;
  unsigned short num_faces;
  unsigned num_triangulated_indices;
  unsigned num_edges;
  unsigned num_subdivision_iterations;

  signed cx, cy, cz, ocx, ocy, ocz, vcx, vcy, vcz;
  coord x, y, z, xmask, zmask, vx, vz;
  unsigned ck, cv, i, op;
  const env_voxel_graphic_blob* graphic_blobs[256], * all_graphic_blobs[256];
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

  if (!threads[thread_ordinal].not_busy) {
    threads[thread_ordinal].not_busy = SDL_CreateSemaphore(1);
    if (!threads[thread_ordinal].not_busy)
      errx(EX_SOFTWARE,
           "Unable to create semaphore for manifold thread %d: %s",
           thread_ordinal, SDL_GetError());
  }

  memset(base_y, ~0, sizeof(base_y));
  memset(vertex_indices, ~0, sizeof(vertex_indices));
  memset(vertex_adjacency, ~0, sizeof(vertex_adjacency));
  memset(graphic_blobs, 0, sizeof(graphic_blobs));
  memset(light_y, 0, sizeof(light_y));
  num_vertices = 0;
  num_faces = 0;
  num_triangulated_indices = 0;
  num_edges = 0;

  if (r->vmap->is_toroidal) {
    xmask = r->vmap->xmax - 1;
    zmask = r->vmap->zmax - 1;
  } else {
    xmask = zmask = ~0u;
  }

  /* Build bitset of grahpic blob presence and determine lighting */
  for (cz = -2; cz <= 1+(MHIVE_SZ>>lod); ++cz) {
    z = (z0 + (cz<<lod)) & zmask;
    if (z >= r->vmap->zmax) {
      memset(has_graphic_blob[cz+2], 0, sizeof(has_graphic_blob[cz+2]));
      continue;
    }

    for (cx = -2; cx <= 1+(MHIVE_SZ>>lod); ++cx) {
      has_graphic_blob[cz+2][cx+2] = 0;

      x = (x0 + (cx<<lod)) & xmask;
      if (x >= r->vmap->xmax) continue;

      for (cy = 0; cy < ENV_VMAP_H>>lod; ++cy) {
        y = cy<<lod;
        graphic = env_vmap_manifold_renderer_get_graphic_blob(
          r->graphics, r->vmap, x, y, z, lod);
        if (graphic) {
          has_graphic_blob[cz+2][cx+2] |= 1 << cy;
          all_graphic_blobs[graphic->ordinal] = graphic;
          light_y[cz+2][cx+2] = cy;
        }
      }
    }
  }

  /* Generate faces */
  for (cz = -2; cz <= 1+(MHIVE_SZ>>lod); ++cz) {
    z = (z0 + (cz<<lod)) & zmask;
    if (z >= r->vmap->zmax) continue;

    for (cx = -2; cx <= 1+(MHIVE_SZ>>lod); ++cx) {
      x = (x0 + (cx<<lod)) & xmask;
      if (x >= r->vmap->xmax) continue;
      /* Skip the inner loop if we know there's nothing in this column */
      if (!has_graphic_blob[cz+2][cx+2]) continue;

      for (cy = 0; cy < ENV_VMAP_H>>lod; ++cy) {
        if (!(has_graphic_blob[cz+2][cx+2] & (1 << cy))) continue;
        y = cy<<lod;

        graphic = NULL;

        for (ck = 0; ck < lenof(voxel_checks); ++ck) {
          ocx = cx + voxel_checks[ck].ox;
          ocy = cy + voxel_checks[ck].oy;
          ocz = cz + voxel_checks[ck].oz;

          /* The Y extrema always get faces, so always continue if at such an
           * extreme.
           */
          if (ocy >= 0 && ocy < ENV_VMAP_H) {
            /* Else, see whether a face needs to be inserted here. */
            if (ocx < -2 || ocx >  1+(MHIVE_SZ>>lod) ||
                ocz < -2 || ocz >  1+(MHIVE_SZ>>lod) ||
                (has_graphic_blob[ocz+2][ocx+2] & (1 << ocy)))
              continue;
          }

          /* Need a face here */
          /* First make sure there's room */
          if (num_faces >= MAX_FACES) goto face_generation_done;

          if (!graphic)
            graphic = env_vmap_manifold_renderer_get_graphic_blob(
              r->graphics, r->vmap, x, y, z, lod);

          faces[num_faces].graphic = graphic->ordinal;
          faces[num_faces].is_extraneous =
            (cx <= -2 || cx >= 1+(MHIVE_SZ>>lod) ||
             cz <= -2 || cz >= 1+(MHIVE_SZ>>lod));
          if (!faces[num_faces].is_extraneous)
            graphic_blobs[graphic->ordinal] = graphic;

          for (cv = 0; cv < 4; ++cv) {
            vcx = cx + voxel_checks[ck].v[cv].rx;
            vcy = cy + voxel_checks[ck].v[cv].ry;
            vcz = cz + voxel_checks[ck].v[cv].rz;

            /* Generate the vertex if needed */
            if (0xFFFF == vertex_indices[vcz+2][vcx+2][vcy]) {
              /* Abort if insufficient space */
              if (num_vertices >= MAX_VERTICES)
                goto face_generation_done;

              if (!~base_y[vcz+2][vcx+2]) {
                vx = (((vcx<<lod) + x0) & xmask) * TILE_SZ;
                vz = (((vcz<<lod) + z0) & zmask) * TILE_SZ;
                base_y[vcz+2][vcx+2] = (*r->get_y_offset)(
                  r->base_object, vx, vz);
              }

              /* Exclude shared offsets (x0, z0, base_coordinate) from the
               * vertices so we don't lose FP precision. They'll be added back
               * in by the vertex shader.
               */
              svertices[num_vertices] =
                sse_piof((vcx<<lod) * TILE_SZ,
                         (vcy<<lod) * TILE_SZ + base_y[vcz+2][vcx+2],
                         (vcz<<lod) * TILE_SZ,
                         65536 * (ocy > light_y[ocz+2][ocx+2]));
              vertex_indices[vcz+2][vcx+2][vcy] = num_vertices++;
            }

            faces[num_faces].vertices[cv] = vertex_indices[vcz+2][vcx+2][vcy];
          }

          /* Ensure vertex adjacencies are recorded */
          for (cv = 0; cv < 4; ++cv) {
            num_edges += record_vertex_link(
              vertex_adjacency,
              faces[num_faces].vertices[cv],
              faces[num_faces].vertices[(cv+1) & 3]);
            num_edges += record_vertex_link(
              vertex_adjacency,
              faces[num_faces].vertices[cv],
              faces[num_faces].vertices[(cv-1) & 3]);
          }

          ++num_faces;
        }
      }
    }
  }

  face_generation_done:

  for (num_subdivision_iterations = 0;
       (signed)num_subdivision_iterations < 2 - lod &&
         num_vertices + num_edges + num_faces < MAX_VERTICES &&
         num_faces * 4 < MAX_FACES;
       ++num_subdivision_iterations) {
    catmull_clark_subdivide(svertices, vertex_adjacency, faces,
                            all_graphic_blobs,
                            num_vertices, num_faces,
                            num_subdivision_iterations,
                            thread_ordinal);
    /* Each iteration creates:
     * - One vertex per face
     * - One vertex per edge
     * - One edge per edge (ie, all edges are split in two)
     * - Four edges per face
     * - Three new faces per face (ie, all faces are split in four)
     */
    num_vertices += num_edges + num_faces;
    num_edges = 4 * num_faces + 2 * num_edges;
    num_faces *= 4;
  }

  /* Ensure that all the data from the prior run has been sent to the GPU
   * before we start overwriting it.
   */
  if (SDL_SemWait(threads[thread_ordinal].not_busy))
    errx(EX_SOFTWARE, "Failed to wait for semaphore on manifold thread %d: %s",
         thread_ordinal, SDL_GetError());

  for (i = 0; i < num_vertices; ++i) {
    glvertices[i][0] = SSE_VS(svertices[i], 0);
    glvertices[i][1] = SSE_VS(svertices[i], 1);
    glvertices[i][2] = SSE_VS(svertices[i], 2);
    glvertices[i][3] = SSE_VS(svertices[i], 3);
  }

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

  glm_op.vao = &mhive->vao;
  glm_op.buffers = mhive->buffers;
  glm_op.vertex_data_size = num_vertices * sizeof(glvertices[0]);
  glm_op.vertex_data = glvertices;
  glm_op.index_data_size =
    num_triangulated_indices * sizeof(triangulated_indices[0]);
  glm_op.index_data = triangulated_indices;
  glm_op.not_busy = threads[thread_ordinal].not_busy;
  glm_do((void(*)(void*))render_env_vmap_manifolds_put_buffer_data, &glm_op);
  return mhive;
#undef base_y
#undef svertices
#undef glvertices
#undef vertex_indices
#undef vertex_adjacency
#undef faces
#undef triangulated_indices
#undef has_graphic_blob
#undef light_y
#undef glm_op
}

static void env_vmap_manifold_render_mhive_delete_impl(
  env_vmap_manifold_render_mhive* this
) {
  glDeleteBuffers(2, this->buffers);
  glDeleteVertexArrays(1, &this->vao);
  free(this);
}

static void env_vmap_manifold_render_mhive_delete(
  env_vmap_manifold_render_mhive* this
) {
  glm_do((void(*)(void*))env_vmap_manifold_render_mhive_delete_impl, this);
}

typedef struct {
  const env_vmap_manifold_render_mhive*restrict mhive;
  const rendering_context*restrict ctxt;
} env_vmap_manifold_render_mhive_render_op;

static void env_vmap_manifold_render_mhive_render_impl(
  env_vmap_manifold_render_mhive_render_op* op
) {
  const env_vmap_manifold_render_mhive*restrict mhive = op->mhive;
  const rendering_context*restrict ctxt = op->ctxt;

  const rendering_context_invariant*restrict context = CTXTINV(ctxt);
  shader_manifold_uniform uniform;
  unsigned i;
  coord effective_camera;

  free(op);

  uniform.torus_sz[0] = context->proj->torus_w;
  uniform.torus_sz[1] = context->proj->torus_h;
  uniform.yrot[0] = zo_float(context->proj->yrot_cos);
  uniform.yrot[1] = zo_float(context->proj->yrot_sin);
  uniform.rxrot[0] = zo_float(context->proj->rxrot_cos);
  uniform.rxrot[1] = zo_float(context->proj->rxrot_sin);
  uniform.zscale = zo_float(context->proj->zscale);
  uniform.soff[0] = context->proj->sxo;
  uniform.soff[1] = context->proj->syo;
  uniform.noisetex = 0;
  uniform.palette = 1;
  /* There are a total of nine nominal months, 0..9. However, the final one is
   * simply a terminal fencepost which should map to 1.0, so the divisor would
   * be nine.
   *
   * If OpenGL texture coordinates worked like pixel coordinates, that is.
   * OpenGL interpretes the coordinates to refer to the _centre_ of pixels,
   * however; thus, time 0.0 needs to map to 0.5/10 (assuming 10 pixels in the
   * palette), and time 1.0 needs to map to 9.5/10. Thus the full range is 0.9
   * (accomplished by increasing the divisor back up to 10) with an offset of
   * 0.05. (This does mean that palettes with more than 10 rows will not use
   * some of the rows.)
   */
  uniform.palette_t = (context->month_integral +
                       context->month_fraction / (float)fraction_of(1)) / 10.0f
                    + 0.05f;
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

  glBindVertexArray(mhive->vao);

  for (i = 0; i < mhive->num_operations; ++i) {
    uniform.noise_bias = mhive->operations[i].graphic->noise_bias / 65536.0f;
    uniform.noise_amplitude =
      mhive->operations[i].graphic->noise_amplitude / 65536.0f;
    uniform.noise_freq[0] =
      mhive->operations[i].graphic->noise_xfreq / 65536.0f;
    uniform.noise_freq[1] =
      mhive->operations[i].graphic->noise_yfreq / 65536.0f;

    glBindTexture(GL_TEXTURE_2D, mhive->operations[i].graphic->noise);
    glActiveTexture(GL_TEXTURE1);
    glBindTexture(GL_TEXTURE_2D, mhive->operations[i].graphic->palette);
    glActiveTexture(GL_TEXTURE0);

    shader_manifold_activate(&uniform);
    /*
    shader_manifold_configure_vbo();
    */
    glDrawElements(GL_TRIANGLES, mhive->operations[i].length,
                   GL_UNSIGNED_SHORT,
                   (GLvoid*)(mhive->operations[i].offset * sizeof(unsigned short)));
  }
}

static void env_vmap_manifold_render_mhive_render(
  const env_vmap_manifold_render_mhive*restrict mhive,
  const rendering_context*restrict ctxt
) {
  env_vmap_manifold_render_mhive_render_op* op =
    xmalloc(sizeof(env_vmap_manifold_render_mhive_render_op));
  op->mhive = mhive;
  op->ctxt = ctxt;
  glm_do((void(*)(void*))env_vmap_manifold_render_mhive_render_impl, op);
}
