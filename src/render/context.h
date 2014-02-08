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
#ifndef RENDER_CONTEXT_H_
#define RENDER_CONTEXT_H_

#include "../coords.h"
#include "../frac.h"
#include "../graphics/canvas.h"
#include "../graphics/perspective.h"

/**
 * Defines the invariant data which define a "rendering context" from the
 * perspective of code outside of the rendering system.
 */
typedef struct {
  /**
   * The projection being used to render the scene.
   */
  const perspective*restrict proj;

  /**
   * The integer value (0..11) of the current scenic month. 0,1,2 are spring;
   * 3,4,5 are summer; 6,7,8 are autumn; 9,10,11 are winter.
   */
  unsigned month_integral;
  /**
   * The fraction of which the current scenic month has elapsed.
   */
  fraction month_fraction;

  /**
   * The current chronon of the game being rendered.
   */
  chronon now;

  /**
   * The "long" rotation of the camera; in other words, the total Y rotation
   * that the camera has undergone, often being outside the normal angle
   * range. This is maintained for texture X coordinate correction, since 256
   * is not a multiple of pi, and thus textures would "jump" at the wrap point
   * if only an angle were used.
   */
  signed long_yrot;

  /**
   * The total width of the screen, in pixels. This is used by operations which
   * need to determine pixel sizes but do not render directly to a canvas.
   */
  unsigned screen_width;
} rendering_context_invariant;

/**
 * A rendering_context holds precalculated data needed to render a single frame
 * of the scene. It is assumed to alias with nothing, and must not be altered
 * outside calls to rendering_context_set(). The structure begins with a
 * rendering_context_invariant, but other data calculated therefrom follow. The
 * format of these data is calculated at run-time.
 */
typedef struct rendering_context rendering_context;

/**
 * Allocates space for a rendering_context, including all auxilliary data. This
 * is a potentially expensive call.
 */
rendering_context* rendering_context_new(void);
/**
 * Frees the resources held by the given rendering_context.
 */
void rendering_context_delete(rendering_context*restrict);
/**
 * Copies the given rendering_context_invariant into the given context, and
 * updates auxilliary data accordingly.
 */
void rendering_context_set(rendering_context*restrict,
                           const rendering_context_invariant*restrict);

#define CTXTINV(context) ((const rendering_context_invariant*restrict)context)

/**
 * Provides the scaffolding necessary for adding a member to the rendering
 * context. prefix specifies the prefix for all global names produced; typename
 * is a full type-name which identifies the member to add to the rendering
 * context.
 *
 * The following items are generated:
 *
 * static size_t prefix_context_offset
 *   The byte offset within a rendering context where the member will be
 *   located.
 *
 * size_t prefix_put_context_offset(size_t)
 *   Sets prefix_context_offset to the parameter, then returns parameter plus
 *   the size of the member. This is to be called by rendering_context_new().
 *
 * static inline const typename* prefix_get(const void*)
 *   Returns the member embedded within the given rendering context.
 *
 * static inline typename* prefix_getm(void*)
 *   Returns the member embedded within the given rendering context, but not
 *   const.
 */
#define RENDERING_CONTEXT_STRUCT(prefix,typename)               \
  static size_t prefix##_context_offset;                        \
  size_t prefix##_put_context_offset(size_t sz) {               \
    prefix##_context_offset = sz;                               \
    return sz + sizeof(typename);                               \
  }                                                             \
  static inline typename const* prefix##_get(                   \
    const void*restrict context                                 \
  ) {                                                           \
    return (typename const*restrict)                            \
      (((const char*restrict)context)+prefix##_context_offset); \
  }                                                             \
  static inline typename* prefix##_getm(                        \
    void*restrict context                                       \
  ) {                                                           \
    return (typename*restrict)                                  \
      (((char*restrict)context)+prefix##_context_offset);       \
  }

#endif /* RENDER_CONTEXT_H_ */
