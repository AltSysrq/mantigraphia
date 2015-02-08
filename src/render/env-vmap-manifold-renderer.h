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
#ifndef RENDER_ENV_MAP_MANIFOLD_RENDERER_H_
#define RENDER_ENV_MAP_MANIFOLD_RENDERER_H_

#include "../math/coords.h"
#include "../world/env-vmap.h"
#include "../graphics/canvas.h"
#include "context.h"
#include "env-voxel-graphic.h"

/**
 * The number of tiles in each dimension comprising a single mhive in an
 * env_vmap_manifold_renderer.
 */
#define ENV_VMAP_MANIFOLD_RENDERER_MHIVE_SZ 64

/**
 * Internal structure storing precalculated information and heavyweight
 * handles for rendering all or part of a vmap.
 */
typedef struct env_vmap_manifold_render_mhive_s env_vmap_manifold_render_mhive;

/**
 * Renders voxel maps by grouping blob-style voxels into solid manifold meshes.
 *
 * This struct should be considered immutable by external code.
 */
typedef struct {
  /**
   * The voxel map that this renderer renders.
   */
  const env_vmap* vmap;
  /**
   * An array of size NUM_ENV_VOXEL_CONTEXTUAL_TYPES which describes how each
   * contextual type is to be rendered.
   *
   * This is an array of pointers so that graphical equivalence between
   * contextual types can be indicated, which permits some optimisations.
   */
  const env_voxel_graphic*const* graphics;

  /**
   * The base coordinate value added to all coordinates produced by this
   * renderer.
   */
  vc3 base_coordinate;
  /**
   * The object from which all Y coordinates in the vmap are relative.
   */
  const void* base_object;
  /**
   * Function used to query base_object for its base Y offset at the given
   * (X,Z) location. The X and Z coords are relative to vmap (ie, before
   * base_coordinate is applied).
   */
  coord (*get_y_offset)(const void* base_object, coord x, coord z);

  /**
   * Internal state.
   */
  env_vmap_manifold_render_mhive* mhives[FLEXIBLE_ARRAY_MEMBER];
} env_vmap_manifold_renderer;

/**
 * Allocates a new env_vmap_manifold_renderer.
 *
 * @param vmap The vmap to be rendered by this renderer. Its X and Z dimensions
 * must be multiples of ENV_VMAP_MANIFOLD_RENDERER_MHIVE_SZ.
 */
env_vmap_manifold_renderer* env_vmap_manifold_renderer_new(
  const env_vmap* vmap,
  const env_voxel_graphic*const graphics[NUM_ENV_VOXEL_CONTEXTUAL_TYPES],
  const vc3 base_coordinate,
  const void* base_object,
  coord (*get_y_offset)(const void*, coord, coord));

/**
 * Releases the resources held by the given env_vmap_manifold_renderer.
 */
void env_vmap_manifold_renderer_delete(env_vmap_manifold_renderer*);

/**
 * Renders the vmap of the given renderer.
 */
void render_env_vmap_manifolds(canvas* dst, env_vmap_manifold_renderer*restrict,
                               const rendering_context*restrict context);

#endif /* RENDER_ENV_MAP_RENDERER_H_ */
