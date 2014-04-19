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

#include "../graphics/perspective.h"
#include "../world/basic-world.h"
#include "../world/props.h"
#include "context.h"
#include "props.h"

void render_world_props(const world_prop* props,
                        unsigned num_props,
                        const basic_world* world,
                        coord xmin, coord xmax,
                        coord zmin, coord zmax,
                        unsigned char distsq_shift,
                        const prop_renderer* renderers,
                        const rendering_context*restrict context) {
  unsigned lower_bound, upper_bound, i;
  coord tmp, cx, cz;
  signed long long dx, dz;
  unsigned long long raw_distsq, distsq, inner_distsq, outer_distsq;
  unsigned dist;
  fraction progression;
  int inverted_x_test;
  const perspective*restrict proj =
    ((const rendering_context_invariant*)context)->proj;

  cx = proj->camera[0];
  cz = proj->camera[2];

  lower_bound = props_bsearch_z(props, zmin, 0, num_props);
  upper_bound = props_bsearch_z(props, zmax, 0, num_props);
  if (num_props == lower_bound)
    lower_bound = 0;
  if (num_props == upper_bound)
    upper_bound = 0;

  if (xmin <= xmax) {
    /* Normal case where min < max. Any point between min and max passes the
     * test.
     */
    inverted_x_test = 0;
  } else {
    /* min > max: the X axis wraps around the torus. Swap the two so that min <
     * max; any point between min and max now *fails* the test.
     */
    inverted_x_test = 1;
    tmp = xmin;
    xmin = xmax;
    xmax = tmp;
  }

  /* upper_bound might be less than lower_bound if we'll be wrapping around the
   * Z axis, hence the odd update statement. A highly predictible branch is
   * about twice as fast as a modulo based on emperical testing.
   */
  for (i = lower_bound; i != upper_bound; i = (i+1 == num_props? 0 : i+1)) {
    if (!props[i].type)
      /* Absent */
      continue;

    /* Test X coordinates */
    if (inverted_x_test ^ (props[i].x < xmin || props[i].x >= xmax))
      /* Failed X test */
      continue;

    /* Calculate distance squared */
    dx = torus_dist(cx - props[i].x, world->xmax * TILE_SZ);
    dz = torus_dist(cz - props[i].z, world->zmax * TILE_SZ);
    raw_distsq = dx*dx + dz*dz;
    distsq = raw_distsq >> distsq_shift;
    /* Skip if square root is >= 64 (maximum LOD / input to fisqrt()) */
    if (distsq >= 64*64) continue;

    /* Reduce to actual distance */
    dist = fisqrt(distsq);

    /* Deterimen progression through level */
    inner_distsq = dist*dist;
    inner_distsq <<= distsq_shift;
    outer_distsq = (dist+1)*(dist+1);
    outer_distsq <<= distsq_shift;
    raw_distsq /= METRE;
    raw_distsq /= METRE;
    inner_distsq /= METRE;
    inner_distsq /= METRE;
    outer_distsq /= METRE;
    outer_distsq /= METRE;
    if (outer_distsq > inner_distsq)
      progression = (raw_distsq - inner_distsq) *
        fraction_of(outer_distsq - inner_distsq);
    else
      progression = 0;

    (*renderers[props[i].type])(props+i, world,
                                64 - dist,
                                fraction_of(1) - progression, context);
  }
}
