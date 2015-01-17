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
#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include <string.h>
#include <stdlib.h>

#include "../alloc.h"
#include "rand.h"
#include "coords.h"
#include "frac.h"
#include "poisson-disc.h"

#define FP POISSON_DISC_FP

void poisson_disc_distribution(
  poisson_disc_result* dst,
  unsigned w, unsigned h,
  unsigned desired_points_per_w,
  unsigned max_point_size_fp,
  unsigned lcg
) {
  unsigned point_size_fp = umin(
    max_point_size_fp, w * FP / desired_points_per_w);
  unsigned radius_fp = umax(point_size_fp/2, 2);
  unsigned natural_grid_sz_fp =
    fraction_umul(radius_fp, 0x5A827999 /* 1/sqrt(2) */) - 1;
  unsigned grid_sz_fp = umax(natural_grid_sz_fp, 1);
  unsigned gridw = w*FP / grid_sz_fp + 1;
  unsigned gridh = h*FP / grid_sz_fp + 1;

  poisson_disc_point* points;
  unsigned* active_points;
  unsigned (*indices)[gridw];
  unsigned num_points = 0, num_active_points = 0;

  angle ang;
  unsigned r_fp, x_fp, y_fp, gx, gy;
  signed gox, goy, dx_fp, dy_fp;
  unsigned i, p;

  points = xmalloc(gridw*gridh * (2*sizeof(unsigned) +
                                  sizeof(points[0])));
  active_points = (void*)(points + gridw*gridh);
  indices = (void*)(active_points + gridw*gridh);

  memset(points, 0, sizeof(points[0]) * gridw*gridh);
  memset(indices, 0, sizeof(unsigned) * gridw*gridh);

  points[0].x_fp = w * FP / 2;
  points[0].y_fp = h * FP / 2;
  active_points[0] = 0;
  num_active_points = num_points = 1;

  while (num_active_points) {
    place_next_point:

    /* Ensure the most recent point is in the grid */
    indices[points[num_points-1].y_fp / grid_sz_fp]
      [points[num_points-1].x_fp / grid_sz_fp] = num_points - 1;

    p = active_points[lcgrand(&lcg) % num_active_points];
    for (i = 0; i < 8; ++i) {
      ang = lcgrand(&lcg);
      r_fp = radius_fp + lcgrand(&lcg) % radius_fp;
      x_fp = points[p].x_fp + zo_cosms(ang, r_fp);
      y_fp = points[p].y_fp + zo_sinms(ang, r_fp);
      if (x_fp >= w*FP || y_fp >= h*FP) goto reject_point;

      gx = x_fp / grid_sz_fp;
      gy = y_fp / grid_sz_fp;
      if (gx >= gridw || gy >= gridh ||
          gx == points[indices[gy][gx]].x_fp / grid_sz_fp ||
          gy == points[indices[gy][gx]].y_fp / grid_sz_fp)
        goto reject_point;

      for (goy = -5; goy <= +5; ++goy) {
        if ((unsigned)(gy+goy) < gridh) {
          for (gox = -5; gox <= +5; ++gox) {
            if ((unsigned)(gx+gox) < gridw) {
              dx_fp = x_fp - points[indices[gy+goy][gx+gox]].x_fp;
              dy_fp = y_fp - points[indices[gy+goy][gx+gox]].y_fp;
              if (dx_fp*dx_fp + dy_fp*dy_fp < (signed)(radius_fp*radius_fp))
                goto reject_point;
            }
          }
        }
      }

      /* Acceptable distance from all neighbours */
      points[num_points].x_fp = x_fp;
      points[num_points].y_fp = y_fp;
      active_points[num_active_points] = num_points;
      ++num_points;
      ++num_active_points;
      goto place_next_point;

      reject_point:;
    }

    /* Failed to place any new points near this one, mark inactive */
    for (i = 0; active_points[i] != p; ++i);
    memmove(active_points + i, active_points + i + 1,
            (num_active_points - i - 1) * sizeof(unsigned));
    --num_active_points;
  }

  /* In most cases, the caller will immediately convert points into some other
   * format and then destroy the result, so there's no point separating the
   * points allocation and the other temporary stuff that isn't strictly part
   * of the result.
   */
  dst->points = points;
  dst->num_points = num_points;
  dst->point_size_fp = point_size_fp;
}

void poisson_disc_result_minify(poisson_disc_result* result) {
  poisson_disc_point* new_points;

  new_points = realloc(result->points,
                       sizeof(poisson_disc_point) * result->num_points);
  if (new_points)
    result->points = new_points;
}

void poisson_disc_result_destroy(poisson_disc_result* result) {
  free(result->points);
}
