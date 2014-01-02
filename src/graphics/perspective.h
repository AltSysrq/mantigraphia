/*-
 * Copyright (c) 2013, 2014 Jason Lingle
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
#ifndef GRAPHICS_PERSPECTIVE_H_
#define GRAPHICS_PERSPECTIVE_H_

#include "../coords.h"
#include "canvas.h"

/**
 * Provides parameters describing how to project world coordinates into
 * orthogonal screen coordinates. This includes both camera information and
 * projection/screen information.
 *
 * Note that perspective translation and projection is by design *not* correct
 * according to normal cartesian space.
 */
typedef struct {
  /**
   * The absolute (world) coordinates of the camera.
   */
  vc3 camera;
  /**
   * The maximum (exclusive) X and Z coordinates of the world torus,
   * respectively.
   */
  coord torus_w, torus_h;
  /**
   * Angles from which other values are derived. These are not directly used by
   * the perspective code, but are used by other code which must reason about
   * the raw angles in question.
   *
   * fov is automatically set by perspective_init().
   */
  angle yrot, rxrot, fov;
  /**
   * The cosine and sine of the rotation of the camera about the Z axis.
   */
  zo_scaling_factor yrot_cos, yrot_sin;
  /**
   * The cosine and sine of the rotation of the camera about its *relative* X
   * axis (ie, after Y rotation is taken into account).
   */
  zo_scaling_factor rxrot_cos, rxrot_sin;
  /**
   * The near clipping plane, in scaled coordinates. Vertices in front of it (Z
   * <= plane) are discarded. This must be at least 0. The actual clipping
   * plane is dependent on fov and screen size.
   */
  coord_offset near_clipping_plane;
  /**
   * The "effective" near clipping plane, in relative coordinates. This is set
   * by perspective_init(), and compensates the absolute near clipping plane
   * for the Z scaling factor. This value is always negative.
   */
  coord_offset effective_near_clipping_plane;
  /**
   * Perspective Z scaling factor. Should be set by perspective_init().
   */
  zo_scaling_factor zscale;
  /**
   * Screen coordinate translation offsets. Should be set by
   * perspective_init().
   */
  coord_offset sxo, syo;
} perspective;

/**
 * Initialises the so noted fields of the given perspective to project onto the
 * centre of the given canvas with the given field of view.
 */
void perspective_init(perspective*,
                      const canvas*,
                      angle fov);


/**
 * Translates the given vector according to the perspective, without projecting
 * it into screen space. Thus, dst is set to the coordinates *relative* to the
 * camera. This always succeeds.
 */
void perspective_xlate(vo3 dst, const vc3 src, const perspective*);

/**
 * Projects src (world coordinates) into dst (orthogonal screen coordinates)
 * according to the given perspective.
 *
 * Returns 0 if src cannot be projected into screen coordinates (eg, resulting
 * in a negative screen Z value). In this case, the contents of dst are
 * destroyed and undefined.
 */
int perspective_proj(vo3 dst,
                     const vc3 src,
                     const perspective*);

/**
 * Projects src (relative coordinates, as per perspective_xlate()) into dst
 * (orthogonal screen coordinates) according to the given perspective.
 *
 * Returns 0 if src cannot be projected into screen coordinates (eg, resulting
 * in a negative screen Z value). In this case, the contents of dst are
 * destroyed and undefined.
 */
int perspective_proj_rel(vo3 dst, const vo3 src, const perspective*);

#endif /* GRAPHICS_PERSPECTIVE_H_ */
