/*-
 * Copyright (c) 2013 Jason Lingle
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
#ifndef WORLD_BASIC_WORLD_H_
#define WORLD_BASIC_WORLD_H_

#include <assert.h>
#include <stdio.h>

#include "../bsd.h"
#include "../coords.h"

/**
 * @file
 * Provides types and definitions used to describe the game world. The game
 * world consists of a square grid of tiles. Multiple versions of the world at
 * different granularities may be maintained.
 */

/**
 * Each tile in the world is a square, with sides of length TILE_SZ. Lower
 * granularities will bitshift this value up.
 */
#define TILE_SZ METRE
/**
 * In order to save space, vertical coordinates in tiles are specified at a
 * lower granularity. They can be converted to world coordinates by multiplying
 * by this scaling factor.
 */
#define TILE_YMUL (METRE/8)
/**
 * Each tile consists of TILE_NELT independent elements.
 *
 * This is currently 1 since plans are now to have things that would have been
 * other elements instead be static objects, since the "grid of cubes" model
 * this was designed for doesn't really mesh with the artistic style.
 */
#define TILE_NELT 1

/**
 * The type of a tile is used to determine how it is drawn and how it behaves
 * in the world. This value is used as a look-up index into a table. Every
 * element index may have a separate table. Types with lower values are
 * considered "stronger"; when reducing granularity of the main world, upper
 * worlds prefer stronger tiles.
 */
typedef unsigned char tile_type;
/**
 * The thickness of the tile, in terms of TILE_YMUL. The Y extent of a tile
 * ranges from altitude to altitude+thickness. A thickness of zero indicates
 * the absence of the element, except for element zero. For element zero,
 * thickness is irrelevant.
 */
typedef unsigned char tile_thickness;
/**
 * The base altitude of the tile, in terms of TILE_YMUL.
 */
typedef unsigned short tile_altitude;

/**
 * Each element in a tile defines its own, independent type, thickness, and
 * base altitude.
 */
typedef struct {
  tile_type      type;
  tile_thickness thickness;
  tile_altitude  altitude;
} tile_element;

/**
 * Each tile consists of TILE_NELT elements, which are to be sorted in
 * ascending order by altitude. Element zero always exists, and is the
 * terrain. Other elements may or may not exist (as determined by their
 * thickness), and existing elements need not be contiguous.
 */
typedef struct {
  tile_element elts[TILE_NELT];
} tile_info;

/**
 * The basic information for a world is simply a grid of tiles, and maximum
 * values for the X and Z coordinates (beyond which they wrap). Additionally,
 * each world may have a "next" world, which is an identical world with half
 * granularity on both X and Z axes.
 */
typedef struct basic_world_s {
  /**
   * Maximum dimensions of the world along the X and Z axes.
   */
  coord xmax, zmax;
  /**
   * Entry for the "next" world of half granularity. Note that there is no
   * SLIST_HEAD for this; each world simply refers to the next, if one exists.
   */
  SLIST_ENTRY(basic_world_s) next;
  /**
   * Tiles describing the world. The actual length of this array is xmax*zmax.
   */
  tile_info tiles[FLEXIBLE_ARRAY_MEMBER];
} basic_world;

/**
 * Calculates the offset within the tiles array of the given coordinates within
 * the given world.
 */
static inline unsigned basic_world_offset(const basic_world* world,
                                          coord x, coord z) {
  return x + world->xmax * z;
}

/**
 * Allocates a chain of worlds with granularities ranging from the given max to
 * the given min coordinates. The first world has max coordinates (xmax,zmax),
 * and each subsequent world has each maximum coordinate halved. The tiles for
 * each world are not initialised.
 */
basic_world* basic_world_new(coord xmax, coord zmax, coord xmin, coord zmin);
/**
 * Deletes all memory held by the given world chain. The pointer given must be
 * the first world in the chain.
 */
void basic_world_delete(basic_world*);

/**
 * Recalculates the tiles of all worlds of lower granularity than the one
 * given. This is a fairly expensive operation, and should only be called after
 * the initial world generation.
 */
void basic_world_calc_next(basic_world*);
/**
 * Patches all worlds subsequent to the given world, assuming that only the
 * tile at (x,z) has changed.
 */
void basic_world_patch_next(basic_world*, coord x, coord z);

/**
 * Dumps a graphical representation of the map to a BMP-format image in the
 * given stream, which is not closed.
 *
 * This is only useful for debugging / investigation / demonstrational
 * purposes.
 */
void basic_world_bmp_dump(FILE*, const basic_world*);

/**
 * Deserialises a basic_world from the given input file. The format of this
 * file is undefined, and is both architecture- and compiler-specific. This
 * function blindly trusts that everything works perfectly.
 */
basic_world* basic_world_deserialise(FILE*);
/**
 * Serialises the basic_world into the given output file. The format of this
 * file is undefined, and is both architecture- and compiler-specific. If
 * writing to the file fails, a diagnostic is printed. This file can
 * potentially contain unrelated information about the process.
 */
void basic_world_serialise(FILE*, const basic_world*);

#endif /* WORLD_BASIC_WORLD_H_ */
