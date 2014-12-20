/*-
 * Copyright (c) 2014 Jason Lingle
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

#include <string.h>
#include <glew.h>

#include "../bsd.h"
#include "../alloc.h"
#include "../math/rand.h"
#include "../math/frac.h"
#include "../math/coords.h"
#include "../gl/shaders.h"
#include "../gl/glinfo.h"
#include "../gl/marshal.h"
#include "canvas.h"
#include "paint-overlay.h"

#define DESIRED_POINTS_PER_SCREENW 128

struct paint_overlay_s {
  GLuint vbo;
  unsigned num_points;
  unsigned point_size;
};

paint_overlay* paint_overlay_new(const canvas* canv) {
  /* Poisson-disc sampling.
   *
   * See http://bost.ocks.org/mike/algorithms/
   */
  paint_overlay* this = xmalloc(sizeof(paint_overlay));

  unsigned point_size = canv->w / DESIRED_POINTS_PER_SCREENW < max_point_size?
    canv->w / DESIRED_POINTS_PER_SCREENW : max_point_size;
  unsigned radius = point_size/2 + 1;
  unsigned natural_grid_sz =
    fraction_umul(radius, 0x5A827999 /*1/sqrt(2)*/) - 1;
  unsigned grid_sz = natural_grid_sz? natural_grid_sz : 1;
  unsigned gridw = canv->w / grid_sz, gridh = canv->h / grid_sz;

  unsigned* active_points;
  struct { unsigned x, y; }* points;
  unsigned (*indices)[gridw];
  unsigned num_points = 0, num_active_points = 0;

  unsigned lcg = 0x5A827999 /* arbitrary seed */;
  angle ang;
  unsigned r, x, y, gx, gy;
  signed gox, goy, dx, dy;
  unsigned i, p;

  shader_solid_vertex* vertices;

  active_points = xmalloc(gridw*gridh * (2*sizeof(unsigned) +
                                         sizeof(points[0])));
  points = (void*)(active_points + gridw*gridh);
  indices = (void*)(points + gridw*gridh);

  memset(indices, 0, sizeof(unsigned)*gridh*gridw);
  memset(points, 0, sizeof(points[0])*gridh*gridw);

  points[0].x = canv->w / 2;
  points[0].y = canv->h / 2;
  active_points[0] = 0;
  num_active_points = num_points = 1;

  while (num_active_points) {
    place_next_point:

    /* Ensure the most recent point is in the grid */
    indices[points[num_points-1].y / grid_sz]
      [points[num_points-1].x / grid_sz] = num_points - 1;

    p = active_points[lcgrand(&lcg) % num_active_points];
    for (i = 0; i < 8; ++i) {
      ang = lcgrand(&lcg);
      r = radius + lcgrand(&lcg) % radius;
      x = points[p].x + zo_cosms(ang, r);
      y = points[p].y + zo_sinms(ang, r);
      if (x >= canv->w || y >= canv->h) goto reject_point;

      gx = x / grid_sz;
      gy = y / grid_sz;
      if (gx == points[indices[gy][gx]].x / grid_sz ||
          gy == points[indices[gy][gx]].y / grid_sz)
        goto reject_point;

      for (goy = -5; goy <= +5; ++goy) {
        if ((unsigned)(gy+goy) < gridh) {
          for (gox = -5; gox <= +5; ++gox) {
            if ((unsigned)(gx+gox) < gridw) {
              dx = x - points[indices[gy+goy][gx+gox]].x;
              dy = y - points[indices[gy+goy][gx+gox]].y;
              if (dx*dx + dy*dy < (signed)(radius*radius))
                goto reject_point;
            }
          }
        }
      }

      /* Acceptable distance from all neighbours */
      points[num_points].x = x;
      points[num_points].y = y;
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

  vertices = xmalloc(num_points * sizeof(shader_solid_vertex));
  for (i = 0; i < num_points; ++i) {
    vertices[i].v[0] = points[i].x;
    vertices[i].v[1] = points[i].y;
    vertices[i].v[2] = 0;
    canvas_pixel_to_gl4fv(
      vertices[i].colour,
      (0xFF << ASHFT) |
      (lcgrand(&lcg) ^ (lcgrand(&lcg) << 16)));
  }

  glGenBuffers(1, &this->vbo);
  glBindBuffer(GL_ARRAY_BUFFER, this->vbo);
  glBufferData(GL_ARRAY_BUFFER, num_points * sizeof(shader_solid_vertex),
               vertices, GL_STATIC_DRAW);
  free(vertices);
  free(active_points);

  this->num_points = num_points;
  this->point_size = point_size;
  return this;
}

void paint_overlay_delete(paint_overlay* this) {
  glDeleteBuffers(1, &this->vbo);
  free(this);
}

static void paint_overlay_postprocess_impl(paint_overlay* this) {
  shader_solid_activate(NULL);
  glBindBuffer(GL_ARRAY_BUFFER, this->vbo);
  glPointSize(this->point_size);
  shader_solid_configure_vbo();
  glDisable(GL_TEXTURE_2D);
  glDrawArrays(GL_POINTS, 0, this->num_points);
}

void paint_overlay_postprocess(paint_overlay* this) {
  glm_do((void(*)(void*))paint_overlay_postprocess_impl, this);
}
