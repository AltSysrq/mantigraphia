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

#include "../frac.h"
#include "../graphics/glpencil.h"
#include "../graphics/perspective.h"
#include "../world/basic-world.h"
#include "../world/terrain.h"
#include "draw-queue.h"
#include "props.h"
#include "context.h"
#include "grass-props.h"

typedef struct {
  glpencil_handle* pencil;
} grass_props_context_info;
RENDERING_CONTEXT_STRUCT(grass_props, grass_props_context_info)

void grass_props_context_ctor(rendering_context*restrict ctxt) {
  grass_props_getm(ctxt)->pencil = NULL;
}

void grass_props_context_set(rendering_context*restrict ctxt) {
  grass_props_context_info* info = grass_props_getm(ctxt);
  glpencil_handle_info hinfo = {
    CTXTINV(ctxt)->screen_width / 480
  };

  if (!info->pencil)
    info->pencil = glpencil_hnew(&hinfo);
}

void grass_props_context_dtor(rendering_context*restrict ctxt) {
  grass_props_context_info* info = grass_props_getm(ctxt);
  if (info->pencil)
    glpencil_hdelete(info->pencil);
}

static void render_grass_prop_simple(drawing_queue*, const world_prop*,
                                     const basic_world*,
                                     unsigned, fraction,
                                     const rendering_context*restrict);

static const prop_renderer grass_prop_renderers_[] = {
  0,
  render_grass_prop_simple,
};
const prop_renderer*const grass_prop_renderers = grass_prop_renderers_;

static const float grass_colour[3] = { 8.0f / 255.0f, 96.0f / 255.0f,
                                       12.0f / 255.0f };

static void render_grass_prop_simple(drawing_queue* dst,
                                     const world_prop* this,
                                     const basic_world* world,
                                     unsigned level, fraction progression,
                                     const rendering_context*restrict context) {
  const perspective*restrict proj = CTXTINV(context)->proj;
  glpencil_spec pencil;
  vc3 base, tip;
  vo3 pbase, ptip;

  glpencil_init(&pencil, grass_props_get(context)->pencil);
  pencil.colour = grass_colour;

  base[0] = tip[0] = this->x;
  base[2] = tip[2] = this->z;
  base[1] = terrain_base_y(world, this->x, this->z);
  tip[1] = base[1] + METRE * level / 64;

  if (!perspective_proj(pbase, base, proj) ||
      !perspective_proj(ptip, tip, proj))
    return;

  glpencil_draw_line(NULL, &pencil, pbase, 0, ptip, 0);
  glpencil_flush(NULL, &pencil);
}
