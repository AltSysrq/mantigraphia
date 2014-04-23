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
#ifndef WORLD_HSPACE_H_
#define WORLD_HSPACE_H_

#include "../coords.h"

#define MAX_HSPACES 16

/**
 * An H-Space (H=Hammer, because I couldn't come up with any other short name
 * for this concept) is a region of compressed 3-space. For the sake of
 * simplicity, it is restricted to an axis-aligned rectangular prisim which
 * does not wrap around the coordinate system. Two H-Spaces may not intersect;
 * additionally, no two H-Spaces may overlap on the X axis, even if doing so
 * would not make the actual volumes intersect.
 *
 * The coordinate system within an H-Space is perfectly continuous with the
 * conventional space surrounding it; as far as physics are concerned, the only
 * effect of being within an H-Space is the reduction of velocity and size. The
 * transition between conventional and H-Space typically requires abrupt
 * repositioning for graphical correctness.
 */
typedef struct {
  /**
   * The lower coordinates of this H-Space.
   */
  vc3 lower;
  /**
   * The upper coordinates of this H-Space.
   */
  vc3 upper;
  /**
   * The amount by which space within this H-Space is compressed. Velocity of
   * objects must be right-shifted by this amount before being added to
   * position, and as must the size of bounding boxes.
   */
  unsigned char compression;
} hspace;

/**
 * Stores up to MAX_HSPACES H-Space values, allowing for efficient retrieval of
 * H-Spaces based on coordinates.
 */
typedef struct {
  /**
   * The extant H-Spaces, sorted ascending by x0. Only the first num_spaces
   * values are meaningful.
   */
  hspace spaces[MAX_HSPACES];
  /**
   * The number of values in the spaces array.
   */
  unsigned num_spaces;
} hspace_map;

/**
 * Adds an H-Space to the given hspace_map.
 */
void add_hspace(hspace_map*, const hspace*);
/**
 * Retrieves a pointer to the H-Space within the given hspace_map which
 * contains the given coordinate, or NULL if there is no such H-Space.
 */
const hspace* get_hspace(const hspace_map*, const vc3);

#endif /* WORLD_HSPACE_H_ */
