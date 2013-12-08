/*
  Copyright (c) 2013 Jason Lingle
  All rights reserved.

  Redistribution and use in source and binary forms, with or without
  modification, are permitted provided that the following conditions
  are met:
  1. Redistributions of source code must retain the above copyright
     notice, this list of conditions and the following disclaimer.
  2. Redistributions in binary form must reproduce the above copyright
     notice, this list of conditions and the following disclaimer in the
     documentation and/or other materials provided with the distribution.
  3. Neither the name of the author nor the names of its contributors
     may be used to endorse or promote products derived from this software
     without specific prior written permission.

     THIS SOFTWARE IS PROVIDED BY THE AUTHORS AND CONTRIBUTORS ``AS IS'' AND
     ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
     IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
     ARE DISCLAIMED.  IN NO EVENT SHALL THE AUTHORS OR CONTRIBUTORS BE LIABLE
     FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
     DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
     OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
     HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
     LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
     OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
     SUCH DAMAGE.
*/
#ifndef WORLD_TERRAIN_H_
#define WORLD_TERRAIN_H_

#include "world.h"
#include "../coords.h"

/**
 * Returns the base Y coordinate of the terrain at the given world X and Z
 * coordinates. While every terrain tile element nominally has a single exact
 * altitude, the altitude used for display and physics is linearly interpolated
 * across adjacent tiles so that the terrain looks smooth. This function
 * provides the smoothed value.
 */
coord terrain_base_y(const basic_world*, coord x, coord z);
/**
 * Calculates the normal of the terrain at the *centre* of the tile at (tx,tz)
 * (tile coordinates, not world coordinates). The normal vector will have no
 * particular magnitude.
 */
void terrain_basic_normal(vo3 normal, const basic_world*, coord tx, coord tz);

#endif /* WORLD_TERRAIN_H_ */
