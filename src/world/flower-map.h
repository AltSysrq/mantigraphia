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
#ifndef WORLD_FLOWER_MAP_H_
#define WORLD_FLOWER_MAP_H_

/**
 * @file
 *
 * Definitions for the physical representation of flowers and the flower map.
 *
 * The flower map is a strictly graphical concept; however, since world
 * generation does need to be able to support it, the definitions for the data
 * representation are here.
 *
 * As its name suggests, a flower map describes the positions and types of
 * flowers in the world. A flower map is divided into a number of fhives of
 * size FLOWER_FHIVE_SIZE. Each fhive contains some number of flowers in no
 * particular order; each flower defines its own position relative to the
 * origin of the fhive and the terrain, as well as its type.
 */

/**
 * Type used to index flower types.
 *
 * The flower map itself does not perscribe any meaning to this value.
 */
typedef unsigned char flower_type;
#define NUM_FLOWER_TYPES 256

/**
 * Type for terrain-relative height offset for flower positions. Converted to
 * standard coordinates via multiplication by FLOWER_HEIGHT_UNIT.
 */
typedef unsigned char flower_height;
#define FLOWER_HEIGHT_UNIT (8*MILLIMETRE)

/**
 * Type for fhive-relative X and Z offsets for flower positions. Converted to
 * standard coordinates via multiplication by FLOWER_COORD_UNIT.
 */
typedef unsigned short flower_coord;
#define FLOWER_COORD_UNIT (FLOWER_FHIVE_SIZE * TILE_SZ / 65536)
/**
 * The size of an fhive in the X and Z directions, in terms of tiles.
 */
#define FLOWER_FHIVE_SIZE (16)

/**
 * Describes a single flower.
 */
typedef struct {
  /**
   * The type of this flower.
   */
  flower_type type;
  /**
   * The Y coordinate of this flower, relative to the terain.
   */
  flower_height y;
  /**
   * The X and Z coordinates of this flower, relative to the fhive.
   */
  flower_coord x, z;
} flower_desc;

/**
 * Stores the flowers present in a square region of space.
 */
typedef struct {
  /**
   * Dynamic array of flowers in this fhive. Only the first size members are
   * initialised. Pointers into this array are invalidated by mutations to the
   * flower_map.
   */
  flower_desc* flowers;
  /**
   * The number of initialised elements in the array.
   */
  unsigned size;
  /**
   * The allocated size of the array.
   */
  unsigned cap;
} flower_fhive;

/**
 * Stores and manages an array of fhives that cover the whole XZ plane.
 */
typedef struct {
  /**
   * The number of fhives along the X and Z axes, respectively.
   *
   * These are both always powers of two.
   */
  unsigned fhives_w, fhives_h;
  /**
   * The fhives in this flower_map. Indexed by flower_map_fhive_offset().
   */
  flower_fhive hives[FLEXIBLE_ARRAY_MEMBER];
} flower_map;

/**
 * Allocates a new, empty flower map.
 *
 * @param tiles_w The number of tiles along the X axis.
 * @param tiles_h The number of tiles along the Z axis.
 */
flower_map* flower_map_new(unsigned tiles_w, unsigned tiles_h);
/**
 * Frees the given flower_map and all flower data contained.
 */
void flower_map_delete(flower_map*);

/**
 * Inserts a new flower into the given flower_map.
 *
 * @param map The flower_map to mutate.
 * @param type The type of the new flower.
 * @param height The height of the flower, relative to terrain and converted to
 * FLOWER_HEIGHT_UNITs.
 * @param x The world X coordinate of the flower.
 * @param z The world Z coordinate of the flower.
 */
void flower_map_put(flower_map* map, flower_type type, flower_height height,
                    coord x, coord z);
/**
 * Returns the index into flower_map::hives of the fhive at the given hive
 * coordinates.
 *
 * @param x The index of the fhive along the X axis.
 * @param z The index of the fhive along the Z axis.
 * @return The array index of the fhive.
 */
unsigned flower_map_fhive_offset(const flower_map*, unsigned x, unsigned z);

#endif /* WORLD_FLOWER_MAP_H_ */
