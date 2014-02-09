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
#ifndef RENDER_TURTLE_H_
#define RENDER_TURTLE_H_

#include "../simd.h"
#include "../coords.h"
#include "../graphics/perspective.h"
#include "draw-queue.h"

/**
 * A turtle allows for rapid drawing of 3D objects into a drawing_queue by
 * means of relative movement. A turtle is initialised to a single point in
 * world space. This point is projected to relative space, and a vector space
 * representing relative space around the point is computed. All further
 * drawing operations are computed by translating distance vectors into the
 * turtle's vector space before projection into screen space, and by
 * manipulating the vector space.
 *
 * Since the turtle assumes that relative space is linear with respect to world
 * space, it can avoid the comparitively expensive projection translation
 * operations, but this makes it unsuitable for drawing extremely large
 * objects, ie, those where the non-linear effects of the
 * non-perspective-correct projection could become noticeable.
 *
 * Precision in turtle space is reduced by a factor (conventionally
 * TURTLE_UNIT) in order to reduce the necessity of bitshifts in internal
 * computation.
 */
typedef struct turtle_state_s turtle_state;

/**
 * Base unit for turtle space. External coordinates/distances are divided by
 * this amount before undergoing computation.
 *
 * This value reduces precision within turtle space to 4mm. This should be
 * sufficient for virtually all drawing operations, however.
 */
#define TURTLE_UNIT 256

/**
 * Stores the current and previous position of the turtle, in screen
 * coordinates.
 */
typedef struct {
  /**
   * The current position of the turtle in relative coordinates. Movement
   * operations are based off of this point.
   */
  simd4 curr;
  /**
   * The previous position of the turtle. This is only used for line drawing.
   */
  simd4 prev;
} turtle_position;

/**
 * Represents the vector space in which the turtle is currently operating. Each
 * vector herein will have a magnitude of the scale passed into turtle_init().
 */
typedef struct {
  simd4 x, y, z;
} turtle_space;

struct turtle_state_s {
  turtle_position pos;
  turtle_space space;
  const perspective* proj;
};

/**
 * Initialises a turtle to the given point. Both current and previous points
 * will be equal to the input vector's relative translation. Turtle space will
 * correspond to world space projected into screen space.
 *
 * Scale affects the magnitude of the vector space. For most uses, TURTLE_UNIT
 * is sufficient, and should be used if there are no special requirements.
 * Lower values yield greater spacial precision at low distance but cause the
 * turtle space to collapse nearer, and reduce directional precision; greater
 * values reduce spacial precision, but make longer-distance drawing possible
 * and improve precision of directional effects.
 *
 * Returns whether the turtle is non-zero. The cases where this is false are:
 *
 * - One of the vectors comprising turtle space was computed to be the zero
 *   vector.
 *
 * The input perspective is used for all further turtle operations that need to
 * project from relative into screen space.
 */
int turtle_init(turtle_state*, const perspective*, const vc3, unsigned scale);

/**
 * Moves the turtle by the given coordinate offsets relative to the current
 * point, transformed according to the current turtle space. The previous point
 * is set to the value of the current point before this call.
 *
 * The input offsets are affected by the scale of the turtle (eg, passed into
 * turtle_init()) and should be considered to be effectively implicitly
 * multiplied by that value.
 */
void turtle_move(turtle_state*, coord_offset, coord_offset, coord_offset);

/**
 * Rotates two axes of turtle space around the third axis by the given
 * angle. The turtle_rotate_?() macros should be used instead of calling this
 * function directly.
 */
void turtle_rotate_axes(simd4*restrict x, simd4*restrict y, angle);
/**
 * Rotates turtle space about the X axis.
 */
#define turtle_rotate_x(state, ang)                             \
  turtle_rotate_axes(&(state)->space.y, &(state)->space.z, ang)
/**
 * Rotates turtle space about the Y axis.
 */
#define turtle_rotate_y(state, ang)                             \
  turtle_rotate_axes(&(state)->space.z, &(state)->space.x, ang)
/**
 * Rotates turtle space about the Z axis.
 */
#define turtle_rotate_z(state, ang)                             \
  turtle_rotate_axes(&(state)->space.x, &(state)->space.y, ang)

/**
 * Reduces the scale of the turtle by shifting each element of turtle space
 * right by the given amount. This will improve spacial precision but reduce
 * directional precision.
 *
 * Returns 0 if turtle space collapsed as a result (ie, a member of the vector
 * space became zero), or 1 otherwise.
 */
int turtle_scale_down(turtle_state*, unsigned shift);

/**
 * Puts the current point into the given drawing queue as a
 * single-point-shift. No action is taken if the current point cannot be
 * projected into screen space.
 *
 * Returns whether the point was inserted into the drawing queue.
 */
int turtle_put_point(drawing_queue_burst*, const turtle_state*,
                     zo_scaling_factor weight);
/**
 * Puts the current and previous points into the drawing queue as a to-from
 * pair, respectively. No action is taken if either point cannot be projected
 * into screen space.
 *
 * Returns whether anything was inserted into the drawing queue.
 */
int turtle_put_points(drawing_queue_burst*, const turtle_state*,
                      zo_scaling_factor from_weight,
                      zo_scaling_factor to_weight);

/**
 * Puts the current point into the given drawing queue burst, then draws a
 * point there. Weight and size are determined automatically according to
 * logical size and screen width. As with dm_proj_calc_weight(), assumes that
 * the underlying drawing method draws relative to the screen width.
 *
 * @param burst Drawing queue burst into which to draw.
 * @param logical_size Size in world space of the object being drawn.
 * @param screen_width The width, in pixels, of the screen.
 * @return Whether anything was enqueued.
 */
int turtle_put_draw_point(drawing_queue_burst* burst, const turtle_state*,
                          coord logical_size, unsigned screen_width);
/**
 * Puts the current and previous points int othe drawing queue, then draws a
 * line between them. Weight and size are determined automatically according to
 * logical size and screen width. As with dm_proj_calc_weight(), assumes that t
 * he underlying drawing method draws relative to the screen width.
 *
 * @param burst Drawing queue burst into which to draw.
 * @param logical_size_from Size in world space of the object being drawn, at
 * the previous point.
 * @param logical_size_to Size in world space of the object being drawn, at the
 * current point.
 * @param screen_width The width, in pixels, of the screen.
 * @return Whether anything was enqueued.
 */
int turtle_put_draw_line(drawing_queue_burst* burst,
                         const turtle_state*,
                         coord logical_size_from,
                         coord logical_size_to,
                         unsigned screen_width);

#endif /* RENDER_TURTLE_H_ */
