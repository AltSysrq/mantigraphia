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
#ifndef GRAPHICS_ABSTRACT_H_
#define GRAPHICS_ABSTRACT_H_

#include "../math/coords.h"

/**
 * A drawing_method abstractly defines how a number of linear graphical
 * primitives can be drawn (eg, brush or pen type).
 *
 * Any drawing operation is performed by some constant state consisting of at
 * least this type (it is usually the first member of another struct) which
 * describes the *constant* parameters about what is being drawn, and some
 * variable but opaque state, called the accumulator, which generally at least
 * includes the target canvas.
 *
 * The two primitive oparations are line drawing and point drawing. Consecutive
 * operations may have effects on each other unless separated by a call to
 * flush. Flushing the state resets it to pristine status, and ensures that all
 * changes have reached the underlying canvas. When all drawing is complete,
 * flush must be called to ensure all changes have actually been made.
 *
 * All drawing operations operate in screen space, rather than world space. The
 * origin is the top-left of the screen; X goes right, Y down, and Z away from
 * the player. The projection is orthogonal (ie, the Z coordinate is simply
 * discarded to get canvas coordinates). Negative coordinates are legal on the
 * X and Y axes, but not the Z axis.
 */
typedef struct drawing_method_s drawing_method;

/**
 * Function type for line drawing. Lines travel between two points in 3D screen
 * space, with a "weight" interpolated linearly with respect to (X,Y) between
 * the points. Interpretation of weight is method-defined.
 */
typedef void (*drawing_method_draw_line_t)(
  void* accum,
  const drawing_method* parms,
  const vo3 from,
  zo_scaling_factor from_weight,
  const vo3 to,
  zo_scaling_factor to_weight);

/**
 * Function type for point drawing. A point is placed in 3D screen space, with
 * a single "weight". Interpretation of weight is method-defined.
 */
typedef void (*drawing_method_draw_point_t)(
  void* accum,
  const drawing_method* parms,
  const vo3 where,
  zo_scaling_factor weight);

/**
 * Function type for drawing flushing. The method must flush all changes to the
 * underlying canvas and reset accum to its pristine state, ready for new,
 * independent drawing operations.
 */
typedef void (*drawing_method_flush_t)(
  void* accum,
  const drawing_method* parms);

struct drawing_method_s {
  drawing_method_draw_line_t draw_line;
  drawing_method_draw_point_t draw_point;
  drawing_method_flush_t flush;
};

/**
 * Convenience function for calling the draw_line abstract primitive.
 */
static inline void dm_draw_line(
  void* accum,
  const drawing_method* parms,
  const vo3 from,
  zo_scaling_factor from_weight,
  const vo3 to,
  zo_scaling_factor to_weight
) {
  (*parms->draw_line)(accum, parms, from, from_weight, to, to_weight);
}

/**
 * Convenience function for calling the draw_point abstract primitive.
 */
static inline void dm_draw_point(
  void* accum,
  const drawing_method* parms,
  const vo3 where,
  zo_scaling_factor weight
) {
  (*parms->draw_point)(accum, parms, where, weight);
}

/**
 * Convenience function for calling the flush abstract primitive.
 */
static inline void dm_flush(
  void* accum,
  const drawing_method* parms
) {
  (*parms->flush)(accum, parms);
}

#endif /* GRAPHICS_ABSTRACT_H_ */
