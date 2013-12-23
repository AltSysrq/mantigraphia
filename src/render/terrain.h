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
#ifndef RENDER_TERRAIN_H_
#define RENDER_TERRAIN_H_

#include "../coords.h"
#include "../graphics/canvas.h"
#include "../graphics/sybmap.h"
#include "../graphics/perspective.h"
#include "../world/world.h"

/**
 * Initialises the resources needed to render terrain.
 */
void render_terrain_init(void);

/**
 * Renders a terrain tile to the given canvas and sybmap, which is at the given
 * tile coordinates in the given world.
 *
 * Terrain tiles MUST be rendered front-to-back in order for usage of the
 * sybmap to make sense.
 */
void render_terrain_tile(canvas*, sybmap*,
                         const basic_world*,
                         const perspective*,
                         coord tx, coord tz,
                         coord_offset logical_tx,
                         coord_offset logical_ty,
                         unsigned char size_shift);

#endif /* RENDER_TERRAIN_H_ */
