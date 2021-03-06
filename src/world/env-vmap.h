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
 * This value indicates the logical type of a particular voxel.
 *
 * A value of zero always indicates empty space. Non-zero values define actual
 * types (though the semantics of each type are defined externally).
 */
typedef unsigned char env_voxel_type;
#define NUM_ENV_VOXEL_TYPES (1 << (8*sizeof(env_voxel_type)))

/**
 * The fixed number of vertical elements in a voxel map.
 */
#define ENV_VMAP_H 32

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
 *
 * The vmap also stores approximate maximum draw distance per voxel. This is a
 * two-bit field which is not directly addressable, and has lower resolution
 * than the vmap as a whole.
 *
 * Terminology notes:
 * voxel: A single entry in a vmap, ie, the value at a discrete coordinate.
 * cell: A 2x2x2 group of voxels with 2-voxel alignment.
 * supercell: A 4x4x4 group of voxels with 4-voxel alignment.
 * hive: A region of voxels in the vmap deliniated by some bounding box in the
 *       X,Z plane (ie, it covers the entire Y axis in that region). Both ends
 *       of a hive are supercell-aligned. There are multiple types of hives
 *       used by different components; they are distinguished by being prefixed
 *       to indicate their use (ie, vhive is used for voxel-style rendering).
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
   * An array of voxels in this vmap.
   *
   * The head of the array is cache-line aligned.
   *
   * Voxels within the array are arranged in a somewhat peculiar fashion. Space
   * is divided into 4x4x4 (64-byte) "supercells". Supercells are layed out in
   * (Z,X,Y) order. Each supercell is divided into 8 2x2x2 (8-byte) "cells",
   * which themselves are layed out in (Z,X,Y) order within the supercell. The
   * 8 voxels within the cell are layed out in (Z,X,Y) order within the cell.
   *
   * This layout provides a number of benefits:
   *
   * - Each supercell lies on exactly one cache line. Accessing a voxel and all
   *   its neighbours thus has 4 cache misses worst-case and 1 cache miss
   *   best-case, whereas a linear layout has 6 cache misses worst-case and 3
   *   cache misses best-case.
   *
   * - Cells are 8-byte aligned. Therefore, a single `unsigned long long` can
   *   be read to quickly obtain a cell's contents (but in no particular
   *   order).
   *
   * - Supercells are aligned suitably for use with SSE, and can be represented
   *   with 4 SSE registers.
   */
  env_voxel_type*restrict voxels;

  /**
   * Stores the visibility distance for voxels. The format of this memory is
   * unspecified, except that it is cache-line-aligned.
   */
  unsigned char*restrict visibility;
} env_vmap;

/**
 * Creates a new, empty vmap of the given (x,z) dimensions and toroidality.
 *
 * @param xmax The size of the X axis.
 * @param zmax The size of the Y axis.
 * @param is_toroidal Whether this vmap represents toroidal (for the X and Z
 * axes) space. If true, xmax and zmax MUST be powers of two.
 */
env_vmap* env_vmap_new(coord xmax, coord zmax, int is_toroidal);
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
  unsigned supercell_offset =
    z/4 * (vmap->xmax/4) * (ENV_VMAP_H/4) + x/4 * (ENV_VMAP_H/4) + y/4;
  unsigned cell_offset =
    (z&2)*2 + (x&2) + (y&2)/2;
  unsigned voxel_offset = (z&1)*4 + (x&1)*2 + (y&1);
  return supercell_offset*64 + cell_offset*8 + voxel_offset;
}

/**
 * Modifies the vmap to ensure that the voxel at (x,y,z) has at least the given
 * visibility level. This may affect neighbouring voxels, and will never reduce
 * any voxel's visibility.
 */
void env_vmap_make_visible(env_vmap* vmap,
                           coord x, coord y, coord z,
                           unsigned char level);

/**
 * Checks whether the voxel at (x,y,z) is visible at the given level.
 *
 * The level parameter controls not only the threshold of the test, but the
 * resoultion at which the test is carried out.
 *
 * This is always true at level 0.
 */
int env_vmap_is_visible(const env_vmap* vmap,
                        coord x, coord y, coord z,
                        unsigned char level);

#endif /* WORLD_ENV_VMAP_H_ */
