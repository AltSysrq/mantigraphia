/*-
 * Copyright (c) 2014, 2015 Jason Lingle
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
#ifndef RENDER_ENV_MAP_VOXEL_RENDERER_H_
#define RENDER_ENV_MAP_VOXEL_RENDERER_H_

#include "../math/coords.h"
#include "../world/env-vmap.h"
#include "../graphics/canvas.h"
#include "context.h"

/**
 * The number of tiles in each dimension comprising a single cell in an
 * env_vmap_voxel_renderer.
 */
#define ENV_VMAP_VOXEL_RENDERER_CELL_SZ 64

/**
 * Describes the parameters used to draw a graphic plane of a voxel.
 *
 * Graphic planes are considered equivalent only if they exist at the same
 * memory address.
 */
typedef struct {
  /**
   * The primary texture to use for rendering.
   *
   * This is an RGBA texture. The RGB components represent true colour; they
   * are passed through to the final colour verbatim. The A component controls
   * fragment visibility; a fragment is visible if the A component of this
   * texture is greater than the G component of the control texture at the same
   * location.
   */
  unsigned texture;

  /**
   * The "control" texture to use for rendering.
   *
   * This is an RG texture. The R component is passed through to the alpha
   * value of the fragment (controlling brush shape and direction). The G
   * component controls visibility according to the A value of the primary
   * texture.
   */
  unsigned control;

  /**
   * Factor by which texture coordinates for this plane shall be multiplied,
   * allowing arbitrary affine transforms.
   *
   * tex.t = x * texture_scale[0][0] + y * texture_scale[0][1]
   * tex.t = y * texture_scale[1][0] + x * texture_scale[1][1]
   *
   * This is 16.16 fixed-point, so 1.0 == 65536.
   */
  signed texture_scale[2][2];
} env_voxel_graphic_plane;

/**
 * Describes how a voxel (more specifically, a contextual voxel type) is
 * represented graphically.
 *
 * Two env_voxel_graphic values are considered equivalent only if they exist at
 * the same memory address.
 */
typedef struct {
  /**
   * In the general case, voxels are rendered as three axis-aligned planes
   * centred on the centre of the voxel. These indicate the parameters for the
   * X, Y, and Z planes, respectively.
   *
   * These are pointers so that plane equivalence can be efficiently detected.
   *
   * NULL values indicate planes that should not be drawn.
   */
  const env_voxel_graphic_plane* planes[3];
} env_voxel_graphic;

/**
 * Internal structure storing precalculated information and heavyweight
 * handles for rendering all or part of a vmap.
 */
typedef struct env_vmap_voxel_render_cell_s env_vmap_voxel_render_cell;

/**
 * Renders voxel maps by simply rendering each voxel as a cube containing three
 * surfaces, as described by env_voxel_graphic.
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
  env_vmap_voxel_render_cell* cells[FLEXIBLE_ARRAY_MEMBER];
} env_vmap_voxel_renderer;

/**
 * Allocates a new env_vmap_voxel_renderer.
 *
 * @param vmap The vmap to be rendered by this renderer. Its X and Z dimensions
 * must be multiples of ENV_VMAP_VOXEL_RENDERER_CELL_SZ.
 */
env_vmap_voxel_renderer* env_vmap_voxel_renderer_new(
  const env_vmap* vmap,
  const env_voxel_graphic*const graphics[NUM_ENV_VOXEL_CONTEXTUAL_TYPES],
  const vc3 base_coordinate,
  const void* base_object,
  coord (*get_y_offset)(const void*, coord, coord));

/**
 * Releases the resources held by the given env_vmap_voxel_renderer.
 */
void env_vmap_voxel_renderer_delete(env_vmap_voxel_renderer*);

/**
 * Renders the vmap of the given renderer.
 */
void render_env_vmap_voxels(canvas* dst, env_vmap_voxel_renderer*restrict,
                            const rendering_context*restrict context);

#endif /* RENDER_ENV_MAP_RENDERER_H_ */
