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
#ifndef WORLD_PROPS_H_
#define WORLD_PROPS_H_

#include "../math/coords.h"

typedef unsigned char prop_type;
typedef unsigned char prop_variant;

/**
 * Defines the property of one "prop" within the world. Props are stationary,
 * but possibly destroyable, objects placed within the world. Props with the
 * same visibility distance are generally stored in contiguous arrays sorted by
 * Z coordinate.
 */
typedef struct {
  /**
   * The X and Z world coordinates of this prop.
   */
  coord x, z;
  /**
   * The type of this prop. This value entirely identifies the behaviour of the
   * prop, such as its dimensions, collidability, and drawing method.
   *
   * A type of zero indicates an absent element (destroyed or otherwise
   * removed).
   */
  prop_type type;
  /**
   * The "variant" of this prop. Variants are used as random seeds when
   * drawing, so that different props of the same type look different.
   */
  prop_variant variant;
  /**
   * The rotation of this prop about the y axis. This only affects the way
   * props are drawn.
   */
  angle yrot;
} world_prop;

/**
 * Sorts the first count elements of the given array of props in-place
 * according to ascending Z coordinates.
 */
void props_sort_z(world_prop* props, unsigned count);
/**
 * Determines the index of the first element in props between lower_bound
 * (inclusive) and upper_bound (exclusive) whose Z coordinate is greater than
 * or equal to the input Z coordinate. It may return upper_bound, indicating
 * that all props in the selected range have an Z coordinate less than the
 * input coordinate.
 */
unsigned props_bsearch_z(const world_prop* props,
                         coord z,
                         unsigned lower_bound,
                         unsigned upper_bound);

#endif /* WORLD_PROPS_H_ */
