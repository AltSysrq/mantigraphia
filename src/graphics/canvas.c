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

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include <SDL.h>

#include <stdlib.h>
#include <string.h>

#include "../alloc.h"
#include "../micromp.h"
#include "canvas.h"

SDL_PixelFormat* screen_pixel_format;

canvas* canvas_new(unsigned width, unsigned height) {
  canvas* this = xmalloc(
    sizeof(canvas) + UMP_CACHE_LINE_SZ +
    sizeof(canvas_pixel)*width*height + UMP_CACHE_LINE_SZ +
    sizeof(canvas_depth)*width*height +
    height);

  this->w = width;
  this->h = height;
  this->pitch = width;
  this->logical_width = width;
  this->ox = this->oy = 0;
  this->px = align_to_cache_line(this + 1);
  this->depth = align_to_cache_line(this->px + width*height);
  this->interlacing = (unsigned char*)(this->depth + width*height);
  memset(this->interlacing, 1, height);
  return this;
}

void canvas_delete(canvas* this) {
  free(this);
}

void canvas_clear(canvas* this) {
  /* Set each depth element to 0x7F7F7F7F. The sign bit can't be set, as
   * SIMD-based drawing functions use signed depth tests. Nothing will ever be
   * drawing 32km away anyway, so the loss of this bit is insignificant.
   */
  memset(this->depth, 0x7F, sizeof(canvas_depth) * this->w * this->h);
}

void canvas_blit(SDL_Texture* dst, const canvas* this) {
  SDL_UpdateTexture(dst, NULL, this->px, this->w * sizeof(canvas_pixel));
}

void canvas_slice(canvas* slice, const canvas* backing,
                  unsigned x, unsigned y,
                  unsigned w, unsigned h) {
  slice->w = w;
  slice->h = h;
  slice->pitch = backing->pitch;
  slice->logical_width = backing->logical_width;
  slice->ox = x + backing->ox;
  slice->oy = y + backing->oy;
  slice->px = backing->px + canvas_offset(backing, x, y);
  slice->depth = backing->depth + canvas_offset(backing, x, y);
  slice->interlacing = backing->interlacing + y;
}
