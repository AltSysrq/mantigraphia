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
#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include <string.h>
#include <stdlib.h>

#include "../coords.h"
#include "canvas.h"
#include "tscan.h"
#include "abstract.h"
#include "perspective.h"
#include "hull.h"

/**
 * Set in the scratch buffer to indicate that the triangle was
 * non-front-facing.
 */
#define SCRATCH_BACK_FACING 0
/**
 * Set in the scratch buffer to indicate that the triangle was front-facing and
 * that it has not yet been processed for outlining.
 */
#define SCRATCH_FRONT_FACING 1
/**
 * Set in the scratch buffer to indicate that the triangle has already been
 * processed for outlining.
 */
#define SCRATCH_OUTLINED 2

static void get_vertex(vc3 dst, const coord_offset*restrict vertices,
                       unsigned ix, unsigned stride,
                       coord_offset ox, coord_offset oy, coord_offset oz,
                       zo_scaling_factor ycos, zo_scaling_factor ysin,
                       const perspective*restrict proj) {
  coord_offset rx, rz;
  vertices += ix*stride;

  rx = zo_scale(vertices[0], ycos) - zo_scale(vertices[2], ysin);
  rz = zo_scale(vertices[2], ycos) + zo_scale(vertices[0], ysin);

  dst[0] = rx + ox;
  dst[1] = vertices[1] + oy;
  dst[2] = rz + oz;
}

void hull_render(canvas*restrict dst,
                 hull_render_scratch*restrict scratch,
                 const hull_triangle*restrict triangles,
                 unsigned num_triangles,
                 const coord_offset*restrict vertices,
                 unsigned stride,
                 coord_offset ox, coord_offset oy, coord_offset oz,
                 angle yrot,
                 triangle_shader shader,
                 void* userdata,
                 const perspective*restrict proj) {
  unsigned t, i;
  zo_scaling_factor ycos = zo_cos(yrot), ysin = zo_sin(yrot);
  vc3 ca, cb, cc;
  vo3 pa, pb, pc;
  coord_offset interp[3][stride-2];

  /* Assume non-front-facing until we learn otherwise */
  memset(scratch, SCRATCH_BACK_FACING,
         sizeof(hull_render_scratch)*num_triangles);
  for (t = 0; t < num_triangles; ++t) {
    /* Read and project vertices */
    get_vertex(ca, vertices, triangles[t].vert[0], stride,
               ox, oy, oz, ycos, ysin, proj);
    if (!perspective_proj(pa, ca, proj)) continue;
    get_vertex(cb, vertices, triangles[t].vert[1], stride,
               ox, oy, oz, ycos, ysin, proj);
    if (!perspective_proj(pb, cb, proj)) continue;
    get_vertex(cc, vertices, triangles[t].vert[2], stride,
               ox, oy, oz, ycos, ysin, proj);
    if (!perspective_proj(pc, cc, proj)) continue;

    /* Check if front-facing. Now in screen space, so front-facing will have a
     * negative Z in the cross product between pa->pb and pa->pc.
     *
     * If the cross product is zero, consider it non-front-facing since it will
     * be a degenerate triangle anyway.
     */
    if ((pb[0]-pa[0])*(pc[1]-pa[1]) - (pc[0]-pa[0])*(pb[1]-pa[1]) >= 0)
      continue;

    /* OK, front-facing */
    scratch[t] = SCRATCH_FRONT_FACING;
    /* Copy interp data */
    for (i = 0; i < 3; ++i)
      memcpy(interp[i], vertices + triangles[t].vert[i]*stride + 3, stride - 3);
    interp[0][stride-3] = pa[2];
    interp[1][stride-3] = pb[2];
    interp[2][stride-3] = pc[2];

    (*shader)(dst, pa, interp[0], pb, interp[1], pc, interp[2], userdata);
  }
}

void hull_outline(canvas*restrict dst,
                  hull_render_scratch*restrict scratch,
                  const hull_triangle*restrict triangles,
                  unsigned num_triangles,
                  const coord_offset*restrict vertices,
                  unsigned stride,
                  coord_offset ox, coord_offset oy, coord_offset oz,
                  angle yrot,
                  const drawing_method* method,
                  void* dm_accum,
                  const perspective* proj) {
  /* TODO: This doesn't actually draw continuous lines yet, but just draws
   * segments as it finds them.
   */
  unsigned t, i;
  zo_scaling_factor ycos = zo_cos(yrot), ysin = zo_sin(yrot);
  vc3 ca, cb;
  vo3 pa, pb;

  for (t = 0; t < num_triangles; ++t) {
    if (SCRATCH_FRONT_FACING == scratch[t]) {
      for (i = 0; i < 3; ++i) {
        if ((0xFFFF ^ triangles[t].adj[i]) &&
            SCRATCH_BACK_FACING == scratch[triangles[t].adj[i]]) {
          /* adj i is an edge to draw */
          get_vertex(ca, vertices, triangles[t].vert[i], stride,
                     ox, oy, oz, ycos, ysin, proj);
          get_vertex(cb, vertices, triangles[t].vert[(i+1)%3], stride,
                     ox, oy, oz, ycos, ysin, proj);
          perspective_proj(pa, ca, proj);
          perspective_proj(pb, cb, proj);
          /* Bump Z forward */
          ++pa[2];
          ++pb[2];

          /* Draw */
          dm_draw_line(dm_accum, method,
                       pa, ZO_SCALING_FACTOR_MAX,
                       pb, ZO_SCALING_FACTOR_MAX);
        }
      }
    }
  }

  dm_flush(dm_accum, method);
}
