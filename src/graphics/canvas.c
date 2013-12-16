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

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include <SDL.h>

#include <stdlib.h>
#include <string.h>

#include "../alloc.h"
#include "canvas.h"

SDL_PixelFormat* screen_pixel_format;

canvas* canvas_new(unsigned width, unsigned height) {
  canvas* this = xmalloc(
    sizeof(canvas) +
    sizeof(canvas_pixel)*width*height +
    sizeof(canvas_depth)*width*height);

  this->w = width;
  this->h = height;
  this->px = (canvas_pixel*)(this + 1);
  this->depth = (canvas_depth*)(this->px + width*height);
  return this;
}

void canvas_delete(canvas* this) {
  free(this);
}

void canvas_clear(canvas* this) {
  memset(this->depth, ~0, sizeof(canvas_depth) * this->w * this->h);
}

void canvas_blit(SDL_Texture* dst, const canvas* this) {
  SDL_UpdateTexture(dst, NULL, this->px, this->w * sizeof(canvas_pixel));
}

void canvas_merge_into(canvas*restrict dst, const canvas*restrict src) {
  unsigned max = dst->w * dst->h;
  unsigned i;

  /* If needed, this could be made more efficient on AMD64 by reading depth
   * information in 64-bit chunks. That probably won't be necessary though.
   */
  for (i = 0; i < max; ++i) {
    if (src->depth[i] < dst->depth[i]) {
      dst->depth[i] = src->depth[i];
      dst->px[i] = src->px[i];
    }
  }
}
