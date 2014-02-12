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
#ifndef GRAPHICS_CANVAS_H_
#define GRAPHICS_CANVAS_H_

#include <SDL.h>
#include <glew.h>

/**
 * The SDL pixel format used by the screen texture.
 */
extern SDL_PixelFormat* screen_pixel_format;

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
   * The number of pixels that must be advanced to progress down one row of
   * pixels. This is distinct from w, which merely indicates the number of
   * pixels in each row.
   */
  unsigned pitch;
  /**
   * The physical width of the ultimate backing of this canvas. Operations
   * relative to the "width" of a canvas typically use this value instead of
   * the actual width, as they may be drawing to a slice of a larger canvas.
   */
  unsigned logical_width;
  /**
   * The logical screen coordinates of the origin of this canvas. This is only
   * to be used for adjusting screen-texture coordinates.
   */
  unsigned ox, oy;
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
 *
 * Canvases to be the targets of slicing (the slice argument of canvas_slice())
 * should not be allocated with this call, since their backing store would then
 * be wasted.
 */
canvas* canvas_new(unsigned width, unsigned height);
/**
 * Frees the memory associated with the given canvas.
 */
void canvas_delete(canvas*);

/**
 * Initialises slice to point to a rectangle within backing, such that the top
 * left of slice corresponds to the point (x,y) in backing, and the dimensions
 * of slice are (w,h).
 *
 * All operations upon either slice or backing are reflected in the other,
 * provided that it involves part of the selected region (ie, they share
 * memory). It is the callers responsibility to ensure that slice is not used
 * after backing is freed.
 */
void canvas_slice(canvas* slice, const canvas* backing,
                  unsigned x, unsigned y,
                  unsigned w, unsigned h);

/**
 * Clears the given canvas. This resets all depth information to ~0. It leaves
 * the pixels unchanged, as typically another background will replace them
 * anyway.
 */
void canvas_clear(canvas*);
/**
 * Blits the given canvas onto the given SDL_Texture.
 */
void canvas_blit(SDL_Texture*, const canvas*);

/**
 * Copies the source canvas into the destination texture, scaling with linear
 * interpolation / sampling, so that the source canvas exactly fills the
 * destination. The depth buffer is *not* copied.
 */
void canvas_scale_onto(canvas*restrict, const canvas*restrict);
/**
 * Converts the given canvas into an OpenGL texture. The canvas is adapted as
 * necessary to meet the limitations of the underlying OpenGL implementation
 * (eg, maximum dimensions or power-of-two restrictions). The program is
 * aborted if the texture cannot be constructed. The depth buffer of the source
 * canvas has no effect.
 *
 * @param mipmap If true, the texture will be fully mipmapped.
 * @return An OpenGL texture unit matching the canvas. The texture and the
 * canvas are wholely independent after this call.
 */
GLuint canvas_to_texture(const canvas*, int mipmap);

/**
 * Calculates the array offset within the given canvas for the given pixel
 * coordinates.
 */
static inline unsigned canvas_offset(const canvas* c, unsigned x, unsigned y) {
  return y * c->pitch + x;
}

#ifdef PROFILE
static void canvas_write(canvas*,unsigned,unsigned,canvas_pixel,canvas_depth)
__attribute__((noinline));
static void canvas_write_c(canvas*,unsigned,unsigned,canvas_pixel,canvas_depth)
__attribute__((noinline));
#endif

static
#ifndef PROFILE
inline
#endif
int canvas_depth_test(canvas* dst, unsigned x, unsigned y,
                      canvas_depth depth) {
  return depth < dst->depth[canvas_offset(dst, x, y)];
}

/**
 * Writes the given pixel and depth information into the canvas at the given
 * coordinate, if the new depth is nearer than the old depth.
 */
static
#ifndef PROFILE
inline
#endif
void canvas_write(canvas* dst, unsigned x, unsigned y,
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
 * Like canvas_write(), but is safe to call with out-of-bounds values, even
 * negative ones (other than depth).
 */
static
#ifndef PROFILE
inline
#endif
void canvas_write_c(canvas* dst, unsigned x, unsigned y,
                    canvas_pixel px, canvas_depth depth) {
  if (x < dst->w && y < dst->h)
    canvas_write(dst, x, y, px, depth);
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

/* This is a marco so it can be used in constexprs */
#define argb(a,r,g,b)                           \
  ((((canvas_pixel)(a)) << ASHFT) |             \
   (((canvas_pixel)(r)) << RSHFT) |             \
   (((canvas_pixel)(g)) << GSHFT) |             \
   (((canvas_pixel)(b)) << BSHFT))

#endif /* GRAPHICS_CANVAS_H_ */
