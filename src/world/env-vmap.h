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
#ifndef WORLD_ENV_VMAP_H_
#define WORLD_ENV_VMAP_H_

#include "../math/coords.h"

/**
 * The integer representation of an environment voxel.
 *
 * This value indicates the logical type of a particular voxel. However, the
 * exact manner in which it is displayed and interacts with its environment is
 * sensitive on the surrounding voxels (see env_voxel_context_map).
 *
 * A value of zero always indicates empty space. Non-zero values define actual
 * types (though the semantics of each type are defined externally).
 */
typedef unsigned char env_voxel_type;
#define NUM_ENV_VOXEL_TYPES (1 << (8*sizeof(env_voxel_type)))

/**
 * Bitmask constants which occupy the lower 6 bits of an
 * env_voxel_contextual_type.
 *
 * Each of these is an axis (X, Y, or Z) and a direction (Positive or
 * Negative).
 */
#define ENV_VOXEL_CONTEXT_XP 1
#define ENV_VOXEL_CONTEXT_XN 2
#define ENV_VOXEL_CONTEXT_YP 4
#define ENV_VOXEL_CONTEXT_YN 8
#define ENV_VOXEL_CONTEXT_ZP 16
#define ENV_VOXEL_CONTEXT_ZN 32

/**
 * Defines how an env_voxel_type is expanded to an env_voxel_contextual_type
 * based on its surroundings.
 */
typedef struct {
  /**
   * A bitset of which env_voxel_types are considered to be in the same
   * "family" as the one described by this value.
   *
   * This is not necessarily mutual, though in most cases it will be.
   */
  unsigned char family[NUM_ENV_VOXEL_TYPES / 8];
  /**
   * A mask composed of the ENV_VOXEL_CONTEXT_?? constants which specifies in
   * which directions the voxel is sensitive to context.
   */
  unsigned char sensitivity;
} env_voxel_context_def;

/**
 * Returns whether the given type is in the voxel family according to the given
 * context definition.
 */
static inline int env_voxel_context_def_is_in_family(
  const env_voxel_context_def* def,
  env_voxel_type type
) {
  return !!(def->family[type / 8] & (1 << (type % 8)));
}

/**
 * Sets whether the given type is in the voxel family according to the given
 * context definition.
 */
static inline void env_voxel_context_def_set_in_family(
  env_voxel_context_def* def,
  env_voxel_type type,
  int is_in_family
) {
  def->family[type / 8] &= ~(1 << (type % 8));
  def->family[type / 8] |= (!!is_in_family) << (type % 8);
}

/**
 * Describes the context sensitivity of all voxel types in a vmap.
 */
typedef struct {
  /**
   * The definition for each voxel type.
   */
  env_voxel_context_def defs[NUM_ENV_VOXEL_TYPES];
} env_voxel_context_map;

/**
 * The "extended type" of a voxel. The upper bits are derived from the original
 * env_voxel_type, whereas the lower 6 are derived from the surrounding voxels.
 */
typedef unsigned short env_voxel_contextual_type;
#define ENV_VOXEL_CONTEXT_BITS 6
#define NUM_ENV_VOXEL_CONTEXTUAL_TYPES                  \
  (NUM_ENV_VOXEL_TYPES << ENV_VOXEL_CONTEXT_BITS)

/**
 * The fixed number of vertical elements in a voxel map.
 */
#define ENV_VMAP_H 16

/**
 * Represents the static environment as a cubic voxel grid.
 *
 * Each voxel is 1x1x1 nominal metre. For the external environment, Y offsets
 * are relative to the terrain, and the map is of course toroidal on the X and
 * Z axes. In interiors, Y offsets are relative to the interior (ie, the
 * hspace), and the map is not toroidal.
 *
 * Unlike terrain maps, there is no generic way to mipmap a vmap. Instead, vmap
 * generators need to manually generate different versions. This is because
 * there is no generally good way to lower the vmap:
 *
 * - If the Y axis were also halved, there would be a max of 4 levels; the
 *   final level would reduce the Y axis to 1 element, eliminating visual
 *   distinction and making everything 16 metres tall.
 *
 * - If the Y axis were not halved, things like trees would coalesce strangely,
 *   and generally would fail to reduce the graphical primitive count as much
 *   as would be desired. Furthermore, it would also look strange, as the
 *   result would be many primitives in approximately the same space.
 */
typedef struct {
  /**
   * The dimensions of this vmap. For the external environment, this is the
   * same as the terrain, so they are guaranteed to be powers of two.
   *
   * They should in general be multiples of 64/ENV_VMAP_H/sizeof(env_voxel) to
   * ensure cache line alignment.
   */
  coord xmax, zmax;

  /**
   * Whether this vmap is toroidal on (X,Z). If true, xmax and zmax are powers
   * of two.
   */
  int is_toroidal;

  /**
   * The mapping from simple to context-sensitive extended voxel types.
   */
  const env_voxel_context_map*restrict context_map;

  /**
   * An array of voxels in this vmap, indexed by (z,x,y).
   *
   * The head of the array is cache-line aligned.
   */
  env_voxel_type*restrict voxels;
} env_vmap;

/**
 * Creates a new, empty vmap of the given (x,z) dimensions and toroidality and
 * with the given context map.
 *
 * @param xmax The size of the X axis.
 * @param zmax The size of the Y axis.
 * @param is_toroidal Whether this vmap represents toroidal (for the X and Z
 * axes) space. If true, xmax and zmax MUST be powers of two.
 * @param context_map The voxel context map to use with this vmap.
 */
env_vmap* env_vmap_new(coord xmax, coord zmax, int is_toroidal,
                       const env_voxel_context_map* context_map);
/**
 * Frees the memory held by the given vmap.
 */
void env_vmap_delete(env_vmap*);

/**
 * Returns the element offset of the voxel in the given vmap at the given
 * (x,y,z) coordinates.
 */
static inline unsigned env_vmap_offset(const env_vmap* vmap,
                                       coord x, coord y, coord z) {
  return z * vmap->xmax * ENV_VMAP_H + x * ENV_VMAP_H + y;
}

/**
 * Returns the contextual type for the voxel at the given coordinates.
 *
 * Note that tile type 0 is not special for this function. Generally, the zero
 * type should not have anything in its family, including itself, and should
 * have no sensitivity.
 */
env_voxel_contextual_type env_vmap_expand_type(
  const env_vmap* vmap, coord x, coord y, coord z);

#endif /* WORLD_ENV_VMAP_H_ */
