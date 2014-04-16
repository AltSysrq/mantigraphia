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
#ifndef WORLD_TERRAIN_H_
#define WORLD_TERRAIN_H_

#include "basic-world.h"
#include "../coords.h"
#include "../simd.h"

/**
 * The base terrain types, used for bits 7..2 of the tile type byte. The lower
 * two bits indicate shadow level (3 = high shadow, 0 = no shadow). Shift left
 * by TERRAIN_SHADOW_BITS to convert to an actual type value.
 */
enum terrain_type {
  terrain_type_snow = 0,
  terrain_type_road,
  terrain_type_stone,
  terrain_type_grass,
  terrain_type_bare_grass,
  terrain_type_gravel,
  terrain_type_water,
};

#define TERRAIN_SHADOW_BITS 2

/**
 * Returns the base Y coordinate of the terrain at the given world X and Z
 * coordinates. While every terrain tile element nominally has a single exact
 * altitude, the altitude used for display and physics is linearly interpolated
 * across adjacent tiles so that the terrain looks smooth. This function
 * provides the smoothed value.
 */
coord terrain_base_y(const basic_world*, coord x, coord z);

/**
 * Returns the base Y coordinate of the terrain at the given world X and Z
 * coordinate, as it is to be rendered. This is often the same as the true base
 * Y, but varies for things such as water.
 */
coord terrain_graphical_y(const basic_world*, coord x, coord z,
                          chronon t);

/**
 * Calculates the normal of the terrain at the *centre* of the tile at (tx,tz)
 * (tile coordinates, not world coordinates). The normal vector will have no
 * particular magnitude.
 */
void terrain_basic_normal(vo3 normal, const basic_world*, coord tx, coord tz);

/**
 * Determines the colour for the terrain at the given world coordinates. This
 * is linearly interpolated across tiles, taking on the tile's exact value at
 * each tile's origin.
 *
 * The return value contains four 32-bit integers representing red, green, and
 * blue, respectively, ranging between 0 and 255, inclusive. The fourth integer
 * has no particular value.
 *
 * The given palette determines the base colours for each tile type, and must
 * be compatible with the `terrain` field of the colour_palettes struct.
 */
simd4 terrain_colour(const basic_world*, coord x, coord z,
                     const simd4* palette);

#endif /* WORLD_TERRAIN_H_ */
