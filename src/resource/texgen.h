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
#ifndef RESOURCE_TEXGEN_H_
#define RESOURCE_TEXGEN_H_

#define TG_TEXDIM 64
#define TG_TEXSIZE (TG_TEXDIM*TG_TEXDIM)

/**
 * @file
 *
 * Utilities for generating textures. Callable from Llua.
 *
 * All functions return statically-allocated memory, which therefore must be
 * immediately copied somewhere that the caller controls.
 *
 * All values are of dimensions TG_TEXDIM by TG_TEXDIM unless noted otherwise.
 */

/* For clarity */
typedef const unsigned char* tg_tex8;
typedef const unsigned char* tg_texrgb;

/**
 * Returns an 8-bit texture entirely filled with the given value.
 */
tg_tex8 tg_fill(unsigned char value);

/**
 * Produces an 8-bit texture whose values are randomly chosen from src using
 * the given seed value.
 *
 * src can be of any size, and is terminated by the first zero byte. A
 * zero-length or NULL src results in generating from values 1..254 uniformly.
 */
tg_tex8 tg_uniform_noise(const unsigned char* src, unsigned seed);

/**
 * Generates a "similarity" texture.
 *
 * The value of each pixel in the output is 255, minus the distance in pixels
 * between its own coordinate and (x,y) (which may be outside the texture),
 * minus the absolute difference between base and the pixel in control at the
 * same coordinates.
 *
 * This can be used to generate Worley noise, among other possibilities.
 */
tg_tex8 tg_similarity(signed x, signed y, tg_tex8 control, signed base);

/**
 * Returns an 8-bit texture whose values are the pairwise maxima of those in
 * the input textures.
 */
tg_tex8 tg_max(tg_tex8, tg_tex8);

/**
 * Returns an 8-bit texture whose values are the pairwise minima of those in
 * the input textures.
 */
tg_tex8 tg_min(tg_tex8, tg_tex8);

/**
 * Adjusts the given 8-bit texture so that all values range from min,
 * inclusive, to max, inclusive.
 */
tg_tex8 tg_normalise(tg_tex8, unsigned char min, unsigned char max);

/**
 * Returns an 8-bit texture which replaces selected pixels of bottom with those
 * of top.
 *
 * Pixels are selected if their values within control are between the same
 * pixels of min and max, both inclusive.
 */
tg_tex8 tg_stencil(tg_tex8 bottom, tg_tex8 top, tg_tex8 control,
                   tg_tex8 min, tg_tex8 max);

/**
 * Zips the given 8-bit textures, each representing one colour channel,
 * together into one RGB texture.
 */
tg_texrgb tg_zip(tg_tex8 r, tg_tex8 g, tg_tex8 b);

/**
 * Mipmaps the given texture, whose dimension is given in dim (and may not
 * exceed TG_TEXDIM or have odd dimensions). The resulting texture has
 * dimensions of (dim/2).
 *
 * Output pixels are obtained by choosing the input pixel (of the 4
 * possibilities) with the greatest R (ie, palette-index) value.
 */
tg_texrgb tg_mipmap_maximum(unsigned dim, tg_texrgb);

#endif /* RESOURCE_TEXGEN_H_ */
