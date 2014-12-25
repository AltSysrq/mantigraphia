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

#include <stdlib.h>
#include <string.h>

#include "linear-paint-tile.h"

void linear_paint_tile_render(canvas_pixel* dst,
                              unsigned w, unsigned h,
                              unsigned xs, unsigned ys,
                              const canvas_pixel* palette,
                              unsigned palette_size) {
  canvas_pixel px, noise[w*h];
  unsigned aa, ar, ag, ab;
  unsigned x, y, xa, ya, i;

  /* Generate noise from the palette */
  for (i = 0; i < w*h; ++i)
    noise[i] = palette[rand() % palette_size];

  /* Average pixels according to streak and bleed */
  for (y = 0; y < h; ++y) {
    for (x = 0; x < w; ++x) {
      aa = ar = ag = ab = 0;
      for (ya = 0; ya < ys; ++ya) {
        for (xa = 0; xa < xs; ++xa) {
          px = noise[((x+xa)%w) + w*((y+ya)%h)];
          aa += get_alpha(px);
          ar += get_red(px);
          ag += get_green(px);
          ab += get_blue(px);
        }
      }

      dst[x + y*w] =
        argb(aa / xs / ys,
             ar / xs / ys,
             ag / xs / ys,
             ab / xs / ys);
    }
  }
}
