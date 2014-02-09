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
#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include "../defs.h"
#include "../simd.h"
#include "../graphics/fast-brush.h"
#include "../graphics/canvas.h"
#include "../graphics/dm-proj.h"
#include "../world/basic-world.h"
#include "../world/terrain.h"
#include "draw-queue.h"
#include "turtle.h"
#include "shared-fast-brush.h"
#include "context.h"
#include "tree-props.h"

static void render_tree_prop_temp(drawing_queue*, const world_prop*,
                                  const basic_world*, unsigned,
                                  const rendering_context*restrict);
static const prop_renderer tree_prop_renderers_[] = {
  0,
  render_tree_prop_temp,
};
const prop_renderer*const tree_prop_renderers = tree_prop_renderers_;

static const canvas_pixel temp_trunk_pallet[] = {
  argb(255, 0, 0, 0),
  argb(255, 32, 28, 0),
  argb(255, 48, 32, 0),
  argb(255, 48, 40, 0),
};

static void render_tree_prop_temp(drawing_queue* queue, const world_prop* this,
                                  const basic_world* world, unsigned level,
                                  const rendering_context*restrict context) {
  drawing_queue_burst burst;
  turtle_state turtle;
  zo_scaling_factor scale;
  unsigned width;
  vc3 root;
  fast_brush_accum accum;

  root[0] = this->x;
  root[1] = terrain_base_y(world, this->x, this->z);
  root[2] = this->z;

  if (!turtle_init(&turtle, CTXTINV(context)->proj, root, TURTLE_UNIT))
    return;

  /* XXX This should be factored into turtle */
  if (simd_vs(turtle.pos.curr, 2) >=
      CTXTINV(context)->proj->effective_near_clipping_plane)
    return;

  scale = dm_proj_calc_weight(CTXTINV(context)->screen_width,
                              CTXTINV(context)->proj,
                              -simd_vs(turtle.pos.curr, 2),
                              METRE);
  width = zo_scale(CTXTINV(context)->screen_width, scale);
  if (width > 1024) return;

  drawing_queue_start_burst(&burst, queue);
  dq_shared_fast_brush(&burst, context);
  accum.colours = temp_trunk_pallet;
  accum.num_colours = lenof(temp_trunk_pallet);
  accum.random = accum.random_seed = this->x ^ this->z;
  accum.distance = 0;
  DQACC(burst, accum);

  turtle_move(&turtle, 0, 4 * METRE / TURTLE_UNIT, 0);
  if (turtle_put_points(&burst, &turtle, scale, scale)) {
    drawing_queue_draw_line(&burst, width);
    drawing_queue_flush(&burst);
  }

  drawing_queue_end_burst(queue, &burst);
}
