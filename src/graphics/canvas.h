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
#ifndef GRAPHICS_CANVAS_H_
#define GRAPHICS_CANVAS_H_

#include <SDL.h>

/**
 * Type for storing pixel data for rendering. It can be broken into
 * colour_components with the get_*() functions defined below.
 */
typedef Uint32 canvas_pixel;
/**
 * Type for storing pixel depth information in a canvas. Lower values are
 * closer to the camera. ~0 is "infinity".
 */
typedef unsigned canvas_depth;
/**
 * Type for representing one colour component of a pixel.
 */
typedef unsigned char colour_component;

/**
 * A canvas is the lowest level of graphical rendering. It contains an array of
 * pixel data (which can be blitted into an SDL_Texture) as well as depth
 * information for each pixel.
 */
typedef struct {
  /**
   * The width and height of the canvas, in pixels, respectively.
   */
  unsigned w, h;
  /**
   * The colour information on the canvas. Offsets can be calculated with
   * canvas_offset(). Alpha components in these pixels have no meaning.
   */
  canvas_pixel*restrict px;
  /**
   * Depth information for each pixel in the canvas. Offsets correspond to the
   * px array.
   */
  canvas_depth*restrict depth;
} canvas;

/**
 * Allocates a new canvas of the given size. The pixel and depth information in
 * the canvas is undefined.
 */
canvas* canvas_new(unsigned width, unsigned height);
/**
 * Frees the memory associated with the given canvas.
 */
void canvas_delete(canvas*);

/**
 * Clears the given canvas. This resets all pixels to black and depth
 * information to ~0.
 */
void canvas_clear(canvas*);
/**
 * Blits the given canvas onto the given SDL_Texture.
 */
void canvas_blit(SDL_Texture*, const canvas*);
/**
 * Merges src into dst, respecting depth information. Both canvases must be the
 * same size.
 */
void canvas_merge_into(canvas*restrict dst, const canvas*restrict src);

/**
 * Calculates the array offset within the given canvas for the given pixel
 * coordinates.
 */
static inline unsigned canvas_offset(const canvas* c, unsigned x, unsigned y) {
  return y * c->w + x;
}

/**
 * Writes the given pixel and depth information into the canvas at the given
 * coordinate, if the new depth is nearer than the old depth.
 */
static inline void canvas_write(canvas* dst, unsigned x, unsigned y,
                                canvas_pixel px, canvas_depth depth) {
  unsigned off = canvas_offset(dst, x, y);
  /* This method involves one poorly-predictable branch and three possible memory
   * accesses. It is possible to do this without a branch, but experimental
   * testing shows that the extra memory accesses required (always four) incur
   * a greater penalty than branch misses even in the worst case for branch
   * prediction.
   */
  if (depth < dst->depth[off]) {
    dst->px[off] = px;
    dst->depth[off] = depth;
  }
}

/**
 * Masks and shifts for colour components within canvas_pixels.
 */
#define AMASK 0xFF000000
#define RMASK 0x00FF0000
#define GMASK 0x0000FF00
#define BMASK 0x000000FF
#define ASHFT 24
#define RSHFT 16
#define GSHFT 8
#define BSHFT 0

static inline colour_component get_alpha(canvas_pixel px) {
  return px >> ASHFT;
}

static inline colour_component get_red(canvas_pixel px) {
  return px >> RSHFT;
}

static inline colour_component get_green(canvas_pixel px) {
  return px >> GSHFT;
}

static inline colour_component get_blue(canvas_pixel px) {
  return px >> BSHFT;
}

static inline canvas_pixel argb(colour_component a,
                                colour_component r,
                                colour_component g,
                                colour_component b) {
  return
    (((canvas_pixel)a) << ASHFT) |
    (((canvas_pixel)r) << RSHFT) |
    (((canvas_pixel)g) << GSHFT) |
    (((canvas_pixel)b) << BSHFT);
}

#endif /* GRAPHICS_CANVAS_H_ */
