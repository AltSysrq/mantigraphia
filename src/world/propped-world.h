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
#ifndef WORLD_PROPPED_WORLD_H_
#define WORLD_PROPPED_WORLD_H_

#include <stdio.h>

#include "terrain-tilemap.h"
#include "props.h"

/**
 * An array of props which populates a propped_world.
 */
typedef struct {
  /**
   * The props themselves. This memory is owned by the propped_world.
   */
  world_prop* props;
  /**
   * The number of elements in props.
   */
  unsigned size;
} prop_array;

/**
 * A propped_world is the cannonical container for a terrain_tilemap plus the
 * arrays of props which populate it.
 */
typedef struct {
  /**
   * The terrain_tilemap representing the terrain and terrain types. Owned by this
   * propped_world.
   */
  terrain_tilemap* terrain;
  /**
   * The grass prop layer. Grass has a short draw distance, and doesn't
   * interact with objects within the environment.
   */
  prop_array grass;
  /**
   * The tree prop layers. Very long draw distance, and can obstruct objects in
   * the environment. The maximum draw distance of layer 0 is twice that of
   * layer 1, but the two use the same levels of detail.
   */
  prop_array trees[2];
} propped_world;

/**
 * Allocates a new propped_world. Memory for all prop arrays is allocated, but
 * not initialised.
 *
 * @param terrain The value for the terrain field of the propped_world. The
 * terrain_tilemap becomes owned by this propped_world.
 * @param num_grass The size of the grass layer prop_array.
 * @param num_trees The size of the trees layers prop_array. Each tree level
 * gets half of this value.
 */
propped_world* propped_world_new(terrain_tilemap* terrain,
                                 unsigned num_grass,
                                 unsigned num_trees);
/**
 * Frees the memory held by the given propped_world, including the associated
 * terrain_tilemap.
 */
void propped_world_delete(propped_world*);

/**
 * Deserialises a propped_world from the given input file. The format of this
 * file is undefined, and is both architecture- and compiler-specific. This
 * function blindly trusts that everything works perfectly.
 */
propped_world* propped_world_deserialise(FILE*);
/**
 * Serialises the propped_world into the given output file. The format of this
 * file is undefined, and is both architecture- and compiler-specific. If
 * writing to the file fails, a diagnostic is printed. This file can
 * potentially contain unrelated information about the process.
 */
void propped_world_serialise(FILE*, const propped_world*);

#endif /* WORLD_PROPPED_WORLD_H_ */
