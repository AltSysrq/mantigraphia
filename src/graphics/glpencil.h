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
#ifndef GRAPHICS_GLPENCIL_H_
#define GRAPHICS_GLPENCIL_H_

#include "../gl/marshal.h"
#include "abstract.h"
#include "canvas.h"

/**
 * The glpencil uses OpenGL to draw simple points and lines.
 *
 * Points are drawn as circles. Lines are not antialiased and will have hard
 * edges and endpoints. Weights passed to drawing functions have no effect.
 *
 * glpencils do not have accumulators; the drawing method implementations
 * ignore the accum parameter.
 *
 * glpencil_spec values are initialised with glpencil_init, built upon a
 * glpencil_handle. glpencil_spec values have thread affinity.
 */
typedef struct {
  /**
   * Vtable. Initialised by glpencil_init().
   */
  drawing_method meth;

  /**
   * The slabs to which drawing operations will occur.
   *
   * Initialised by glpencil_init().
   */
  glm_slab* line_slab, * point_slab;
  /**
   * The colour of the next primitive(s) to draw; the first three values in the
   * array to which this points are used as R, G, and B, respectively. Client
   * code may change this at any time.
   */
  const float* colour;
} glpencil_spec;

/**
 * A glpencil_handle is a mid-weight handle on the shared slab information
 * needed to render glpencils.
 */
typedef struct glpencil_handle_s glpencil_handle;
/**
 * Information which is constant during drawing for any glpencil_handle.
 */
typedef struct {
  /**
   * The thickness of the pencil, in pixels. This is passed directly to
   * OpenGL.
   */
  float thickness;
} glpencil_handle_info;

/**
 * Creates a glpencil_handle with the given parameters.
 */
glpencil_handle* glpencil_hnew(const glpencil_handle_info*);
/**
 * Alters the configuration of the given glpencil_handle.
 */
void glpencil_hconfig(glpencil_handle*, const glpencil_handle_info*);
/**
 * Releases the resources held by the given glpencil_handle.
 */
void glpencil_hdelete(glpencil_handle*);

/**
 * Initialieses a glpencil_spec to draw using the given glpencil_handle. The
 * pencil becomes invalid if the handle is destroyed. No action is required to
 * destroy the glpencil_spec itself.
 */
void glpencil_init(glpencil_spec*, glpencil_handle*);

void glpencil_draw_point(void*, const glpencil_spec*,
                         const vo3, zo_scaling_factor);
void glpencil_draw_line(void*, const glpencil_spec*,
                        const vo3, zo_scaling_factor,
                        const vo3, zo_scaling_factor);
static inline void glpencil_flush(void* a, const glpencil_spec* t) { }

#endif /* GRAPHICS_GLPENCIL_H_ */
