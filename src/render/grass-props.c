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

#include "../graphics/pencil.h"
#include "../graphics/canvas.h"
#include "../graphics/perspective.h"
#include "../world/basic-world.h"
#include "../world/terrain.h"
#include "draw-queue.h"
#include "props.h"
#include "grass-props.h"

static void render_grass_prop_simple(drawing_queue*, const world_prop*,
                                     const basic_world*, unsigned,
                                     const rendering_context*restrict);

static const prop_renderer grass_prop_renderers_[] = {
  0,
  render_grass_prop_simple,
};
const prop_renderer*const grass_prop_renderers = grass_prop_renderers_;

static void render_grass_prop_simple(drawing_queue* dst,
                                     const world_prop* this,
                                     const basic_world* world, unsigned level,
                                     const rendering_context*restrict context) {
  const perspective*restrict proj =
    ((const rendering_context_invariant*)context)->proj;
  unsigned screen_width =
    ((const rendering_context_invariant*)context)->screen_width;
  pencil_spec pencil;
  vc3 base, tip;
  vo3 pbase, ptip;
  drawing_queue_burst burst;

  pencil_init(&pencil);
  pencil.colour = argb(255,8,96,12);

  base[0] = tip[0] = this->x;
  base[2] = tip[2] = this->z;
  base[1] = terrain_base_y(world, this->x, this->z);
  tip[1] = base[1] + METRE * level / 64;

  if (!perspective_proj(pbase, base, proj) ||
      !perspective_proj(ptip, tip, proj))
    return;

  drawing_queue_start_burst(&burst, dst);
  DQMETH(burst, pencil);
  DQCANV(burst);

  drawing_queue_put_points(&burst, pbase, ZO_SCALING_FACTOR_MAX,
                           ptip, ZO_SCALING_FACTOR_MAX);
  drawing_queue_draw_line(&burst, zo_scale(screen_width, pencil.thickness));
  drawing_queue_flush(&burst);
  drawing_queue_end_burst(dst, &burst);
}
