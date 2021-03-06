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
#ifndef RENDER_COLOUR_PALETTES_H_
#define RENDER_COLOUR_PALETTES_H_

#include "../math/sse.h"
#include "../graphics/canvas.h"
#include "../world/terrain.h"
#include "context.h"

#define NUM_GRASS_COLOUR_VARIANTS 8

typedef struct {
  ssepi terrain[4*7];
  canvas_pixel oak_leaf[8];
  canvas_pixel oak_trunk[10];
  canvas_pixel cherry_leaf[8];
  canvas_pixel cherry_trunk[10];
  float grass[NUM_GRASS_COLOUR_VARIANTS][1 << TERRAIN_SHADOW_BITS][3];
} colour_palettes;

const colour_palettes* get_colour_palettes(const rendering_context*restrict);

size_t colour_palettes_put_context_offset(size_t);
void colour_palettes_context_set(rendering_context*restrict);

#endif /* RENDER_COLOUR_PALETTES_H_ */
