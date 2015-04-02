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
#ifndef GRAPHICS_LINEAR_PAINT_TILE_H_
#define GRAPHICS_LINEAR_PAINT_TILE_H_

#include "canvas.h"

/**
 * Renders into dst a tileable texture that looks like an area filled solid
 * with a paint brush (provided an appropriate palette is given). The texture
 * needs to be small enough to reasonably fit on the stack, as proportional
 * stack space is required.
 *
 * This operation is not reproduceable.
 *
 * @param w The width of the texture to generate
 * @param h The height of the texture to generate
 * @param streak The width of the moving average that generates "streaks"
 * @param bleed The height of the moving average that bleeds rows into each
 * other
 * @param palette An array of colours to use to populate the initial texture,
 * which will then be averaged together in various ways.
 * @param palette_size The number of entries in the palette
 */
void linear_paint_tile_render(
  canvas_pixel* dst,
  unsigned w, unsigned h,
  unsigned streak, unsigned bleed,
  const canvas_pixel* palette,
  unsigned palette_size);

#endif /* GRAPHICS_LINEAR_PAINT_TILE_H_ */
