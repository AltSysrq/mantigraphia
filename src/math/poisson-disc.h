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
#ifndef MATH_POISSON_DISC_H_
#define MATH_POISSON_DISC_H_

/**
 * @file
 *
 * Provides facilities for generating Poisson-Disc Distributions of points.
 *
 * See http://bost.ocks.org/mike/algorithms/
 *
 * Items ending in _fp are in fixed-point. Divide by POISSON_DISC_FP to convert
 * to normal integers.
 */

#define POISSON_DISC_FP 16

/**
 * A single point in a PD distribution.
 */
typedef struct {
  /**
   * The coordinates of this point, fixed-point.
   */
  unsigned x_fp, y_fp;
} poisson_disc_point;

/**
 * Structure of result from poisson_disc_distribution().
 *
 * Initialised values of this structure must be destroyed with a call to
 * poisson_disc_result_destroy(). A result produced by
 * poisson_disc_distribution() often holds much more memory than it actually
 * needs to. If a result is to be held for an extended period of time, call
 * poisson_disc_result_minify() to compact the memory to what is actually
 * required to hold the result's data.
 */
typedef struct {
  /**
   * The points generated for the distribution, in no particular order.
   */
  poisson_disc_point* points;
  /**
   * The number of elements in the points array.
   */
  unsigned num_points;
  /**
   * The actual point size used when generating the distribution, fixed-point.
   */
  unsigned point_size_fp;
} poisson_disc_result;

/**
 * Generates a Poisson Disc distribution of points into the given result
 * object, which is assumed to be uninitialised.
 *
 * @param w The maximum X coordinate for any point centre, exclusive.
 * @param h The maximum Y coordinate for any point centre, exclusive.
 * @param desired_points_per_w The number of points that the caller would like
 * to span the distance from 0 to w, other constraints permitting.
 * @param max_point_size_fp The maximum permissable point size, fixed-point.
 * @param seed The random seed to use.
 */
void poisson_disc_distribution(
  poisson_disc_result* dst,
  unsigned w, unsigned h,
  unsigned desired_points_per_w,
  unsigned max_point_size_fp,
  unsigned seed);

/**
 * Reallocates the given disc result so that it uses no more memory than
 * absolutely required.
 */
void poisson_disc_result_minify(poisson_disc_result*);
/**
 * Frees any memory held by the given disc result. Does not free the disc
 * result itself.
 */
void poisson_disc_result_destroy(poisson_disc_result*);

#endif /* MATH_POISSON_DISC_H_ */
