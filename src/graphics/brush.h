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
#ifndef GRAPHICS_BRUSH_H_
#define GRAPHICS_BRUSH_H_

#include "../coords.h"
#include "canvas.h"
#include "abstract.h"

#define MAX_BRUSH_BRISTLES 64

/**
 * The brush drawing method simulates a brush applying paint or sumi (ink) to
 * the canvas. Weight on drawing affects the thickness of the stroke, but not
 * the visual fineness of the brush itself. "Paint" on the brush depletes as it
 * moves through space, and is replenished when the brush is flushed.
 */
typedef struct {
  drawing_method meth;

  /**
   * An array of colours which are applied by this brush, whose size is (at
   * lest) num_colours. As the brush moves over the canvas, bristles move
   * (generally) up through this array. Once they move past it, they cease to
   * apply pigment.
   *
   * Typically, the first few colours will be similar, darker colours, and
   * become progressively lighter as the array progresses. Distinct colours can
   * be used to give the effect of partially mixed paint colours.
   *
   * Colours at the beginning are said to be "strong", whilst those at the end
   * are "weak".
   */
  const canvas_pixel*restrict colours;
  unsigned num_colours;

  /**
   * The number of "bristles" in this brush, which must be between 1 and
   * MAX_BRUSH_BRISTLES, inclusive. The width of each bristle is determined by
   * size, and unaffected by weight.
   */
  unsigned bristles;
  /**
   * Each step, every bristle has an x/65536 chance of decreasing
   * (strengthening) and y/65536 chance of increasing ("weakening"), where x
   * and y are controlled by the below variables.
   *
   * The actual value of x and y is based upon the bristle's location. The
   * centre-most bristle receives the inner_ value; bristles further out have a
   * linearly interpolated value until the outermost bristle, which gets the
   * outer_ value.
   *
   * It does not make sense for x+y to exceed 65536 in any circumstance.
   */
  unsigned inner_strengthening_chance;
  unsigned outer_strengthening_chance;
  unsigned inner_weakening_chance;
  unsigned outer_weakening_chance;
  /**
   * The "noise mask" for the brush. For every pixel, a random value is chosen
   * and ANDed with this value, which is treated as an unsigned char and added
   * to the colour index that would be drawn.
y   */
  unsigned char noise;
  /**
   * The basic size of the brush, relative to the width of the screen. This
   * also controls the visual width of each bristle.
   */
  zo_scaling_factor size;
  /**
   * The size of a "step" in relation to the screen width. Characteristics of
   * the bristles only change on step boundaries.
   */
  zo_scaling_factor step;

  /**
   * The initial state for each bristle in the brush, as an index into
   * colours. The middle bristle is always at (MAX_BRUSH_BRISTLES/2),
   * regardless of the actual number of bristles.
   */
  unsigned char init_bristles[MAX_BRUSH_BRISTLES];
} brush_spec;

/**
 * Accumulator for the brush drawing method. Clients should treat it as
 * opaque. It is declared here so it can be used as an immediate.
 */
typedef struct {
  /* The current index of each bristle */
  unsigned char bristles[MAX_BRUSH_BRISTLES];
  /* The target canvas */
  canvas* dst;
  /* The current state of the LCG */
  unsigned random_state;
  /* The unweighted size, in pixels diameter */
  unsigned basic_size;
  /* The steps (less than one) per pixel. */
  zo_scaling_factor step_size;
} brush_accum;

/**
 * Configures the initial values for the brush drawing method in the given
 * spec. Note that colours and num_colours is not modified.
 */
void brush_init(brush_spec*);
/**
 * Preps the given accumulator to be used by the given brush.
 *
 * @param canvas The canvas to draw to
 * @param random_seed The random seed to use to produce chaotic effects. This
 * should be consistent for usages of the same object on the screen, so that
 * static objects get static paint.
 */
void brush_prep(brush_accum*, const brush_spec*, canvas*,
                unsigned random_seed);

void brush_draw_point(
  brush_accum*restrict,
  const brush_spec*restrict,
  const vo3, zo_scaling_factor);

void brush_draw_line(
  brush_accum*restrict,
  const brush_spec*restrict,
  const vo3, zo_scaling_factor,
  const vo3, zo_scaling_factor);

void brush_flush(brush_accum*, const brush_spec*);

#endif /* GRAPHICS_BRUSH_H_ */
