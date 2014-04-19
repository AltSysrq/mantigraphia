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

#include "../alloc.h"
#include "../micromp.h"
#include "../gl/marshal.h"
#include "../graphics/canvas.h"
#include "../graphics/perspective.h"
#include "../world/propped-world.h"
#include "basic-world.h"
#include "context.h"
#include "props.h"
#include "grass-props.h"
#include "tree-props.h"
#include "propped-world.h"

#define GRASS_DIST (128*METRE)
#define GRASS_DISTSQ_SHIFT (2*(7+16-6))
#define TREES0_DIST (1024*METRE)
#define TREES1_DIST (512*METRE)
#define TREES_DISTSQ_SHIFT (2*(10+16-6))

static const propped_world*restrict render_propped_world_this;
static const rendering_context*restrict render_propped_world_context;
static canvas*restrict render_propped_world_dst;

static void render_propped_world_enqueue(unsigned, unsigned);
static ump_task render_propped_world_enqueue_task = {
  render_propped_world_enqueue,
  0, /* Determined dynamically (= num processors) */
  0, /* Unused (synchronous) */
};

void render_propped_world(canvas* dst,
                          const propped_world*restrict this,
                          const rendering_context*restrict context) {
  render_propped_world_this = this;
  render_propped_world_context = context;
  render_propped_world_dst = dst;

  render_basic_world(dst, this->terrain, context);

  render_propped_world_enqueue_task.num_divisions = ump_num_workers()+1;
  ump_run_sync(&render_propped_world_enqueue_task);
}

static void render_propped_world_enqueue(unsigned ix, unsigned count) {
  const rendering_context*restrict context = render_propped_world_context;
  const propped_world*restrict this = render_propped_world_this;

  const perspective*restrict proj =
    ((const rendering_context_invariant*)context)->proj;
  coord cx = proj->camera[0], cz = proj->camera[2];
  coord_offset zoff_low  = 2 * GRASS_DIST * ix / count - GRASS_DIST;
  coord_offset zoff_high = 2 * GRASS_DIST * (ix+1) / count - GRASS_DIST;

  render_world_props(this->grass.props, this->grass.size,
                     this->terrain,
                     (cx - GRASS_DIST) & (this->terrain->xmax * TILE_SZ - 1),
                     (cx + GRASS_DIST) & (this->terrain->xmax * TILE_SZ - 1),
                     (cz + zoff_low  ) & (this->terrain->zmax * TILE_SZ - 1),
                     (cz + zoff_high ) & (this->terrain->zmax * TILE_SZ - 1),
                     GRASS_DISTSQ_SHIFT,
                     grass_prop_renderers,
                     context);
  zoff_low  = 2 * TREES0_DIST * ix / count - TREES0_DIST;
  zoff_high = 2 * TREES0_DIST * (ix+1) / count - TREES0_DIST;
  render_world_props(this->trees[0].props, this->trees[0].size,
                     this->terrain,
                     (cx - TREES0_DIST) & (this->terrain->xmax * TILE_SZ - 1),
                     (cx + TREES0_DIST) & (this->terrain->xmax * TILE_SZ - 1),
                     (cz + zoff_low  )  & (this->terrain->zmax * TILE_SZ - 1),
                     (cz + zoff_high )  & (this->terrain->zmax * TILE_SZ - 1),
                     TREES_DISTSQ_SHIFT,
                     tree_prop_renderers,
                     context);
  zoff_low  = 2 * TREES1_DIST * ix / count - TREES1_DIST;
  zoff_high = 2 * TREES1_DIST * (ix+1) / count - TREES1_DIST;
  render_world_props(this->trees[1].props, this->trees[1].size,
                     this->terrain,
                     (cx - TREES1_DIST) & (this->terrain->xmax * TILE_SZ - 1),
                     (cx + TREES1_DIST) & (this->terrain->xmax * TILE_SZ - 1),
                     (cz + zoff_low  )  & (this->terrain->zmax * TILE_SZ - 1),
                     (cz + zoff_high )  & (this->terrain->zmax * TILE_SZ - 1),
                     TREES_DISTSQ_SHIFT,
                     tree_prop_renderers,
                     context);
  /* Since we enqueued stuff for GL, we need to ensure this thread's queues get
   * flushed.
   */
  glm_finish_thread();
}
