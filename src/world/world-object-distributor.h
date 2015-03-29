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
#ifndef WORLD_WORLD_OBJECT_DISTRIBUTOR_H_
#define WORLD_WORLD_OBJECT_DISTRIBUTOR_H_

/**
 * @file
 *
 * Provides facilities to efficiently create large number of objects within a
 * vmap using various distributions. It is intended to be called from Llua, and
 * thus uses single-instance global state for tracking most things.
 */

#include "terrain-tilemap.h"
#include "flower-map.h"

/**
 * Initialises the WOD for the given tilemap and flower map, using the given
 * initial random number seed.
 */
void wod_init(const terrain_tilemap*, flower_map*, unsigned seed);

/* Functions below this point are Llua-callable */

/**
 * Zeroes the current distribution, clears altitude restrictions, forbids all
 * terrain types, and removes all elements.
 */
void wod_clear(void);
/**
 * Generates perlin noise with the given amplitude and wavelength (in metres),
 * adding it to the current distribution.
 */
void wod_add_perlin(unsigned wavelength, unsigned amp);
/**
 * Permits objects to be created on the given terrain type (which is a simple
 * type, excluding shadow bits).
 */
void wod_permit_terrain_type(unsigned);
/**
 * Forbids objects from being created at locations where the terrain has an
 * altitude less than min or greater than max.
 */
void wod_restrict_altitude(coord min, coord max);
/**
 * Adds an element type to be generated with an NTVP identified by nfa. The
 * size of the bounding box is given by (w,h); max_iterations is passed through
 * to ntvp_paint(). seed_y for all instances will be zero.
 *
 * @return Whether successful; failure means that too many elements have been
 * added.
 */
unsigned wod_add_ntvp(unsigned nfa, unsigned w, unsigned h,
                      unsigned max_iterations);
/**
 * Adds an element type corresponding to a single flower to be added to the
 * flower map.
 *
 * @param type The flower type to place.
 * @param h0 The minimum height of the flower, in standard coordinates. Must be
 * at least 1 after truncation to flower height units.
 * @param h1 The maximum height, exclusive, of the flower, in standard
 * coordinates. Must be greater than h0 after truncation to flower height
 * units.
 * @return Whether successful; failure means that too many elements have been
 * added, or that the constraints on h0 and h1 were not met.
 */
unsigned wod_add_flower(flower_type type, coord h0, coord h1);
/**
 * Distributes up to max_instances of the current element set throughout the
 * vmap.
 *
 * Attempts where the current distribution is less than threshold are rejected,
 * as are attempts which violate altitude restrictions or start from a
 * forbitdden terrain type.
 *
 * Exactly max_instances attempts will be made.
 *
 * @return Some non-zero cost of evaluation on success, 0 on failure. Element
 * failure is silently ignored.
 */
unsigned wod_distribute(unsigned max_instances, unsigned threshold);

#endif /* WORLD_WORLD_OBJECT_DISTRIBUTOR_H_ */
