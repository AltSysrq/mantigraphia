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
#ifndef RENDER_ENV_MAP_RENDERER_H_
#define RENDER_ENV_MAP_RENDERER_H_

#include "../math/coords.h"
#include "../world/env-vmap.h"
#include "../graphics/canvas.h"
#include "context.h"

/**
 * Describes the parameters used to draw a graphic plane of a voxel.
 */
typedef struct {
  /**
   * The texture to use for rendering. A value of zero indicates that this
   * plane is not drawn.
   *
   * This is an RGB texture. The R component is used to index the S axis of the
   * palette texture (the T axis representing calendar time, ie, to rotate
   * through palettes). The G component determines the A component of the
   * fragment. B is used to control transparency, and is tested against the A
   * of the palette.
   */
  unsigned texture;
  /**
   * The texture to use as a colour palette when rendering the main texture for
   * this plane. Meaningless if this plane is not drawn.
   *
   * This is an RGBA texture. The RGB components are directly copied into
   * rendered fragments. A values determine fragment visibility; a fragment is
   * visible if the palette A is greater than or equal to the texture B.
   */
  unsigned palette;
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
   */
  env_voxel_graphic_plane planes[3];
} env_voxel_graphic;

/**
 * Internal structure storing precalculated information and heavyweight
 * handles for rendering all or part of a vmap.
 */
typedef struct env_vmap_render_cell_s env_vmap_render_cell;

/**
 * Renders voxel maps.
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
  env_vmap_render_cell* cells;
} env_vmap_renderer;

/**
 * Allocates a new env_vmap_renderer.
 *
 * @param subdivide Whether the vmap should be subdivided into multiple cells.
 * If true, the size of the vmap must be a sufficiently large power of 2.
 */
env_vmap_renderer* env_vmap_renderer_new(
  const env_vmap* vmap,
  const env_voxel_graphic*const graphics[NUM_ENV_VOXEL_CONTEXTUAL_TYPES],
  const vc3 base_coordinate,
  const void* base_object,
  coord (*get_y_offset)(const void*, coord, coord),
  int subdivide);

/**
 * Releases the resources held by the given env_vmap_renderer.
 */
void env_vmap_renderer_delete(env_vmap_renderer*);

/**
 * Renders the vmap of the given renderer.
 */
void render_env_vmap(canvas* dst, env_vmap_renderer*restrict,
                     const rendering_context*restrict context);

#endif /* RENDER_ENV_MAP_RENDERER_H_ */
