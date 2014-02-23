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

#include <stdlib.h>

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
#include "lsystem.h"
#include "tree-props.h"

static lsystem temp_tree_system;

void tree_props_init(void) {
  /* 9..6: Branch lengths (movement)
   * -   : Draw branch
   * A..H: Leaf splotches
   * [  ]: Push/pop
   * (  ): Push/pop with size reduction
   * a..h: Rotations (10 degree increments, a = 30 deg)
   * q..v: Forks (15/30/45/60/75/90 degrees)
   * .   : Potential branching point
   * z   : Random rotation on next iteration
   */
  lsystem_compile(
    &temp_tree_system,
    "9 8[z.]8",
    "8 7[z.]7",
    "7 6[z.]6",
    "A B",
    "B C",
    "C D",
    "D E",
    "E F",
    "F G",
    "G H",
    ". . [(q-9B)z.] [(r-9B)z.] [(s-9B)z.] [(t-9B)z.] [(u-9B)z.] [(v-9B)z.]",
    "z a b c d e f g h",
    NULL);
}

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

static const canvas_pixel temp_tree_leaf_pallet[] = {
  argb(255, 0, 32, 0),
  argb(255, 0, 48, 0),
  argb(255, 20, 64, 16),
  argb(255, 16, 52, 8),
};

static void render_tree_prop_temp(drawing_queue* queue, const world_prop* this,
                                  const basic_world* world, unsigned level,
                                  const rendering_context*restrict context) {
  drawing_queue_burst burst;
  turtle_state turtle[17];
  unsigned depth = 0, size_shift = 0, i;
  unsigned screen_width = CTXTINV(context)->screen_width;
  vc3 root;
  fast_brush_accum accum;
  lsystem_state sys;
  coord_offset base_size;

  root[0] = this->x;
  root[1] = terrain_base_y(world, this->x, this->z);
  root[2] = this->z;

  if (!turtle_init(&turtle[0], CTXTINV(context)->proj, root, TURTLE_UNIT))
    return;

  /* Don't do anything of too far behind the camera */
  if (simd_vs(turtle[0].pos.curr, 2) > 4*METRE) return;

  base_size = 8*METRE + this->variant * (METRE / 64);

  /* Move level to a less linear scale. */
  if      (level < 32) level = 0;
  else if (level < 40) level = 1;
  else if (level < 48) level = 2;
  else if (level < 52) level = 3;
  else if (level < 55) level = 4;
  else if (level < 58) level = 5;
  else if (level < 61) level = 6;
  else                 level = 6;
  level /= 2;

  lsystem_execute(&sys, &temp_tree_system, "-9A", level, this->x^this->z);

  drawing_queue_start_burst(&burst, queue);
  dq_shared_fast_brush(&burst, context);
  accum.colours = temp_trunk_pallet;
  accum.num_colours = lenof(temp_trunk_pallet);
  accum.random = accum.random_seed = this->x ^ this->z;
  accum.distance = 0;
  DQACC(burst, accum);

  for (i = 0; sys.buffer[i]; ++i) {
    switch (sys.buffer[i]) {
    default: abort();
    case 'z':
    case '.': break;

    case '(':
      ++size_shift;
      /* fall through */
    case '[':
      turtle[depth+1] = turtle[depth];
      ++depth;
      break;

    case ')':
      --size_shift;
      /* fall through */
    case ']':
      --depth;
      break;

    case '-':
      turtle[depth+1] = turtle[depth];
      turtle_move(turtle+depth+1, 0,
                  (base_size >> size_shift)
                  / TURTLE_UNIT,
                  0);
      turtle_put_draw_line(&burst, turtle+depth+1,
                           METRE >> size_shift, METRE >> size_shift,
                           screen_width);
      break;

    case '9':
    case '8':
    case '7':
    case '6':
      turtle_move(turtle+depth, 0,
                  (base_size >> ('9' - sys.buffer[i]) >> size_shift)
                  / TURTLE_UNIT,
                  0);
      break;

    case 'a':
    case 'b':
    case 'c':
    case 'd':
    case 'e':
    case 'f':
    case 'g':
    case 'h':
      turtle_rotate_y(turtle+depth,
                      65536/12 + (sys.buffer[i] - 'a') * 65536/9);
      break;

    case 'q':
    case 'r':
    case 's':
    case 't':
    case 'u':
    case 'v':
      turtle_rotate_x(turtle+depth,
                      (sys.buffer[i] - 'q' + 1) * 65536/36);
      break;

    case 'A':
    case 'B':
    case 'C':
    case 'D':
    case 'E':
    case 'F':
    case 'G':
    case 'H':
      drawing_queue_flush(&burst);
      accum.colours = temp_tree_leaf_pallet;
      accum.num_colours = lenof(temp_tree_leaf_pallet);
      DQACC(burst, accum);
      turtle_put_draw_point(&burst, turtle+depth,
                            level? 8*METRE : 16*METRE,
                            screen_width);
      drawing_queue_flush(&burst);
      accum.colours = temp_trunk_pallet;
      accum.num_colours = lenof(temp_trunk_pallet);
      DQACC(burst, accum);
      break;
    }
  }

  drawing_queue_flush(&burst);
  drawing_queue_end_burst(queue, &burst);
}
