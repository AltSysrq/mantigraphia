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
#ifndef GRAPHICS_TILED_TEXTURE_H_
#define GRAPHICS_TILED_TEXTURE_H_

#include "canvas.h"
#include "../coords.h"

/**
 * References and describes the data to be painted when an area is to be filled
 * with a tiled texture.
 */
typedef struct {
  /**
   * The raw texture data.
   */
  const canvas_pixel*restrict texture;
  /**
   * Bitmasks which will modulo any given integer to the bounds of the actual
   * texture.
   */
  unsigned w_mask, h_mask;
  /**
   * The width of one row in the texture. This is usually w_mask+1, but need
   * not be.
   */
  unsigned pitch;
  /**
   * Coordinate offsets to apply before texture mapping and rotation. They
   * essentially specify the negative "origin" of the texture within what is
   * being drawn, so that the texture moves with the object as its position on
   * the screen changes.
   */
  coord_offset x_off, y_off;
  /**
   * The sine and cosine of the angle at which the texture is to be rotated
   * about the origin.
   */
  zo_scaling_factor rot_cos, rot_sin;
  /**
   * The resolution (screen width) this texture was designed for. The texture
   * will be scaled to account for differences.
   */
  unsigned nominal_resolution;
} tiled_texture;

/**
 * Fills a triangle on the given canvas with a tiled texture. Z coordinates are
 * interpolated linearly across the triangle.
 *
 * Note that no depth correction is performed --- textures are mapped to the
 * *screen* rather than the polygon. This is consistent with the desired
 * appearance of physical pigment applied to a canvas, rather than an actual
 * three dimensional object.
 */
void tiled_texture_fill(
  canvas*restrict,
  const tiled_texture*restrict,
  const vo3, const vo3, const vo3);

/**
 * Like tiled_texture_fill(), but is compatible with triangle_shader. It's
 * interpolation arguments are just the Z coordinates. Userdata is the texture
 * to tile.
 */
void tiled_texture_fill_a(
  canvas*restrict,
  const coord_offset*restrict,
  const coord_offset*restrict,
  const coord_offset*restrict,
  const coord_offset*restrict,
  const coord_offset*restrict,
  const coord_offset*restrict,
  void*);

#endif /* GRAPHICS_TILED_TEXTURE_H_ */
