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
#ifndef RENDER_FLOWER_MAP_RENDERER_H_
#define RENDER_FLOWER_MAP_RENDERER_H_

#include "../world/flower-map.h"
#include "../graphics/canvas.h"
#include "context.h"

/**
 * Describes how a single flower type is drawn.
 */
typedef struct {
  /**
   * The colour of this flower type, unshadowed.
   */
  canvas_pixel bright_colour;
  /**
   * The colour of this flower type in full shadow.
   */
  canvas_pixel shadow_colour;
  /**
   * The dates (as 16.16 fixed-point months) at which this flower type appears
   * and disappears, respectively.
   *
   * Negative values are permitted so that it is possible to have flowers at
   * full size at date zero.
   */
  signed date_appear, date_disappear;
  /**
   * The size, in standard units, of the flower, when no conditions (such as
   * distance or time) are applied to shrink it.
   */
  coord size;
} flower_graphic;

typedef struct flower_map_renderer_s flower_map_renderer;

/**
 * Allocates a new flower map renderer for the given flower map.
 *
 * @param flowers The flower map to render.
 * @param graphics The graphics for each type of flower. This array must remain
 * valid for the lifetime of the renderer.
 * @param base_object The object to pass to get_y_offset.
 * @param get_y_offset Function to return the base Y coordinate for any (X,Z)
 * coordinate in the world.
 * @return The new renderer.
 */
flower_map_renderer* flower_map_renderer_new(
  const flower_map* flowers,
  const flower_graphic graphics[NUM_FLOWER_TYPES],
  const void* base_object,
  coord (*get_y_offset)(const void* base_object, coord x, coord z));

/**
 * Frees all resources held by the given renderer.
 */
void flower_map_renderer_delete(flower_map_renderer*);

/**
 * Renderes the flower map associated with the given renderer.
 */
void render_flower_map(canvas* dst, flower_map_renderer*restrict,
                       const rendering_context*restrict context);

#endif /* RENDER_FLOWER_MAP_RENDERER_H_ */
