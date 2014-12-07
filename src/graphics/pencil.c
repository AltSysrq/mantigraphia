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

#include <stdlib.h>

#include "../math/coords.h"
#include "../math/frac.h"
#include "canvas.h"
#include "abstract.h"
#include "pencil.h"

void pencil_init(pencil_spec* spec) {
  spec->meth.draw_line = (drawing_method_draw_line_t)pencil_draw_line;
  spec->meth.draw_point = (drawing_method_draw_point_t)pencil_draw_point;
  spec->meth.flush = (drawing_method_flush_t)pencil_flush;
  spec->colour = 0;
  spec->thickness = ZO_SCALING_FACTOR_MAX / 1024;
}

void pencil_flush(canvas* c, const pencil_spec* spec) { }

static unsigned thickness(const canvas* c,
                          const pencil_spec* spec,
                          zo_scaling_factor weight) {
  signed nominal = zo_scale(zo_scale(c->logical_width, spec->thickness),
                            weight);
  return nominal > 0? nominal : 1;
}

void pencil_draw_point(
  canvas* dst,
  const pencil_spec* spec,
  const vo3 where,
  zo_scaling_factor weight
) {
  unsigned diam = thickness(dst, spec, weight), rad, rad2;
  coord_offset x0, x1, y0, y1, x, y, dx, dy;

  if (1 == diam) {
    if (where[0] >= 0 && where[0] < (signed)dst->w &&
        where[1] >= 0 && where[1] < (signed)dst->h) {
      canvas_write(dst, where[0], where[1], spec->colour, where[2]);
    }
  } else {
    rad = diam/2;
    rad2 = diam*diam/4;

    x0 = where[0] - rad;
    x1 = x0 + diam + 1;
    y0 = where[1] - rad;
    y1 = y0 + diam + 1;

    x0 = (x0 >= 0? x0 : 0);
    x1 = (x1 < (signed)dst->w? x1 : (signed)dst->w);
    y0 = (y0 >= 0? y0 : 0);
    y1 = (y1 < (signed)dst->h? y1 : (signed)dst->h);

    for (x = x0; x < x1; ++x) {
      dx = abs(x-where[0]);
      for (y = y0; y < y1; ++y) {
        dy = abs(y-where[1]);
        if (dx*dx + dy*dy <= (coord_offset)rad2)
          canvas_write(dst, x, y, spec->colour, where[2]);
      }
    }
  }
}

void pencil_draw_line(
  canvas* dst,
  const pencil_spec* spec,
  const vo3 from,
  zo_scaling_factor from_weight,
  const vo3 to,
  zo_scaling_factor to_weight
) {
  coord_offset x, y, lx, ly, xp, yp, il, i, imin, imax, t, z, dist;
  coord_offset low, high;
  unsigned thickf = thickness(dst, spec, from_weight);
  unsigned thickt = thickness(dst, spec, to_weight);
  signed thick;
  fraction iil, idist;

  if ((from[0] < 0 && to[0] < 0) ||
      (from[1] < 0 && to[1] < 0) ||
      (from[0] >= (signed)dst->w && to[0] >= (signed)dst->w) ||
      (from[1] >= (signed)dst->h && to[1] >= (signed)dst->h))
    return;

  /* Round off with two endpoints */
  pencil_draw_point(dst, spec, from, from_weight);
  pencil_draw_point(dst, spec, to, to_weight);

  lx = from[0] - to[0];
  ly = from[1] - to[1];

  if (abs(lx) >= abs(ly)) {
    il = abs(lx);
    xp = 0;
    yp = 1;

    if (from[0] < to[0]) {
      low = from[0];
      high = to[0];
    } else {
      low = to[0];
      high = from[0];
    }

    imin = low - clamps(0, low, dst->w);
    imax = clamps(0, high, dst->w) - low;
  } else {
    il = abs(ly);
    xp = 1;
    yp = 0;

    if (from[1] < to[1]) {
      low = from[1];
      high = to[1];
    } else {
      low = to[1];
      high = from[1];
    }

    imin = low - clamps(0, low, dst->h);
    imax = clamps(0, high, dst->h) - low;
  }

  if (il > 0) {
    iil = fraction_of(il);
    dist = isqrt(lx*lx + ly*ly);
    idist = fraction_of(dist);

    for (i = imin; i <= imax; ++i) {
      z = fraction_smul(i*from[2] + (il-i)*to[2], iil);
      thick = fraction_smul(i*thickf + (il-i)*thickt, iil);
      if (0 == thick) thick=1;

      for (t = 0; t < thick; ++t) {
        x = fraction_smul(i*from[0] + (il-i)*to[0], iil) -
            fraction_smul((t-thick/2)*ly, idist);
        y = fraction_smul(i*from[1] + (il-i)*to[1], iil) +
            fraction_smul((t-thick/2)*lx, idist);

        canvas_write_c(dst, x, y, spec->colour, z);
        canvas_write_c(dst, x+xp, y+yp, spec->colour, z);
      }
    }
  } else {
    /* One point, nothing to do */
  }
}
