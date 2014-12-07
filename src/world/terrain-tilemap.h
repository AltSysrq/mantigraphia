/*-
 * Copyright (c) 2013, 2014 Jason Lingle
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
#ifndef WORLD_TERRAIN_TILEMAP_H_
#define WORLD_TERRAIN_TILEMAP_H_

#include <assert.h>
#include <stdio.h>

#include "../bsd.h"
#include "../coords.h"

/**
 * @file
 * Provides types and definitions used to describe the terrain of the game
 * world. The terrain map consists of a square grid of tiles. Multiple versions
 * of the tilemap at different granularities may be maintained.
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
 * The type of a tile is used to determine how it is drawn and how it behaves
 * in the world. This value is used as a look-up index into a table. Every
 * element index may have a separate table. Types with lower values are
 * considered "stronger"; when reducing granularity of the main world, upper
 * worlds prefer stronger tiles.
 */
typedef unsigned char terrain_tile_type;
/**
 * The base altitude of the tile, in terms of TILE_YMUL.
 */
typedef unsigned short terrain_tile_altitude;

/**
 * Each element in a tile defines its own, independent type, thickness, and
 * base altitude.
 */
typedef struct {
  terrain_tile_type      type;
  terrain_tile_altitude  altitude;
} terrain_tile_element;

/**
 * The basic information for a world is simply a grid of tiles, and maximum
 * values for the X and Z coordinates (beyond which they wrap). Additionally,
 * each world may have a "next" world, which is an identical world with half
 * granularity on both X and Z axes.
 */
typedef struct terrain_tilemap_s {
  /**
   * Maximum dimensions of the world along the X and Z axes.
   */
  coord xmax, zmax;
  /**
   * Entry for the "next" world of half granularity. Note that there is no
   * SLIST_HEAD for this; each world simply refers to the next, if one exists.
   */
  SLIST_ENTRY(terrain_tilemap_s) next;
  /**
   * Tiles describing the world. The actual length of this array is xmax*zmax.
   */
  terrain_tile_element tiles[FLEXIBLE_ARRAY_MEMBER];
} terrain_tilemap;

/**
 * Calculates the offset within the tiles array of the given coordinates within
 * the given world.
 */
static inline unsigned terrain_tilemap_offset(
  const terrain_tilemap* world, coord x, coord z
) {
  return x + world->xmax * z;
}

/**
 * Allocates a chain of tilemaps with granularities ranging from the given max
 * to the given min coordinates. The first tilemap has max coordinates
 * (xmax,zmax), and each subsequent tilemap has each maximum coordinate halved.
 * The tiles for each tilemap are not initialised.
 */
terrain_tilemap* terrain_tilemap_new(coord xmax, coord zmax,
                                     coord xmin, coord zmin);
/**
 * Deletes all memory held by the given tilemap chain. The pointer given must
 * be the first tilemap in the chain.
 */
void terrain_tilemap_delete(terrain_tilemap*);

/**
 * Recalculates the tiles of all tilemaps of lower granularity than the one
 * given. This is a fairly expensive operation, and should only be called after
 * the initial tilemap generation.
 */
void terrain_tilemap_calc_next(terrain_tilemap*);
/**
 * Patches all tilemaps subsequent to the given tilemap, assuming that only the
 * tile at (x,z) has changed.
 */
void terrain_tilemap_patch_next(terrain_tilemap*, coord x, coord z);

/**
 * Dumps a graphical representation of the map to a BMP-format image in the
 * given stream, which is not closed.
 *
 * This is only useful for debugging / investigation / demonstrational
 * purposes.
 */
void terrain_tilemap_bmp_dump(FILE*, const terrain_tilemap*);

/**
 * Deserialises a terrain_tilemap from the given input file. The format of this
 * file is undefined, and is both architecture- and compiler-specific. This
 * function blindly trusts that everything works perfectly.
 */
terrain_tilemap* terrain_tilemap_deserialise(FILE*);
/**
 * Serialises the terrain_tilemap into the given output file. The format of this
 * file is undefined, and is both architecture- and compiler-specific. If
 * writing to the file fails, a diagnostic is printed. This file can
 * potentially contain unrelated information about the process.
 */
void terrain_tilemap_serialise(FILE*, const terrain_tilemap*);

#endif /* WORLD_TERRAIN_TILEMAP_H_ */
