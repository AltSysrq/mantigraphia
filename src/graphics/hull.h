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
#ifndef GRAPHICS_HULL_H_
#define GRAPHICS_HULL_H_

#include "../coords.h"
#include "canvas.h"
#include "tscan.h"
#include "abstract.h"
#include "perspective.h"

/**
 * A hull is defined by some number of triangles. Each triangle references
 * exactly three vertices by index, and up to three other adjacent triangles.
 */
typedef struct {
  /**
   * The three vertices which make up this triangle.
   */
  unsigned short vert[3];
  /**
   * The triangles adjacent to each edge of this triangle. Adj 0 is the edge
   * defined by vertices 0 and 1; 1 by 1 and 2; 2 by 2 and 0. A value of ~0
   * indicates there is no adjacent triangle.
   */
  unsigned short adj[3];
} hull_triangle;

/**
 * Type for scratch data shared between hull_render() and hull_outline(). Its
 * contents are unspecified.
 */
typedef unsigned char hull_render_scratch;

/**
 * Renders onto the given canvas the specified set of triangles. The scratch
 * space must be an array of length at least num_triangles. Its initial state
 * has no effect. One vertex is defined every stride values in the vertices
 * array. Each vertex begins with the three spatial coordinates, followed by
 * zero or more interpolation parameters.
 *
 * Only triangles which are front-facing (counter-clockwise) are
 * drawn. triangle_shader is called to shade each triangle. The interpolation
 * parameters explicit in vertices (stride-3) are passed in first, followed by
 * the interpolated Z coordinate in screen space.
 */
void hull_render(canvas*restrict,
                 hull_render_scratch*restrict,
                 const hull_triangle*restrict,
                 unsigned num_triangles,
                 const coord_offset*restrict vertices,
                 unsigned stride,
                 coord_offset x, coord_offset y, coord_offset z,
                 angle yrot,
                 triangle_shader,
                 void* userdata,
                 const perspective*restrict);

/**
 * Draws an outline around the given hull using the given drawing
 * method. triangles and vertices are treated as with hull_render(). The
 * scratch space must have been initialised with a corresponding call to
 * hull_render(). This call destroys the scratch space, so another outline
 * operation requires another call to hull_render().
 *
 * A line segment is produced along every edge at which one triangle is
 * front-facing and the other is back-facing. Screen Z coordinates are shifted
 * slightly forward so that the outline is visible. The drawing method will be
 * called in runs across continuous lines, only flushing when a vertex has no
 * outgoing edges and at the end.
 */
void hull_outline(canvas*restrict,
                  hull_render_scratch*restrict,
                  const hull_triangle*restrict,
                  unsigned num_triangles,
                  const coord_offset*restrict vertices,
                  unsigned stride,
                  coord_offset x, coord_offset y, coord_offset z,
                  angle yrot,
                  const drawing_method* method,
                  void* dm_accum,
                  const perspective*);

#endif /* GRAPHICS_HULL_H_ */
