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

#include "../frac.h"
#include "../defs.h"
#include "../simd.h"
#include "../graphics/canvas.h"
#include "../graphics/dm-proj.h"
#include "../graphics/glbrush.h"
#include "../world/basic-world.h"
#include "../world/terrain.h"
#include "draw-queue.h"
#include "turtle.h"
#include "context.h"
#include "lsystem.h"
#include "colour-palettes.h"
#include "tree-props.h"

RENDERING_CONTEXT_STRUCT(tree_props_trunk, glbrush_handle*)
RENDERING_CONTEXT_STRUCT(tree_props_leaves, glbrush_handle*)

size_t tree_props_put_context_offset(size_t sz) {
  return tree_props_trunk_put_context_offset(
    tree_props_leaves_put_context_offset(sz));
}

void tree_props_context_ctor(rendering_context*restrict context) {
  *tree_props_trunk_getm(context) = NULL;
  *tree_props_leaves_getm(context) = NULL;
}

void tree_props_context_dtor(rendering_context*restrict context) {
  if (*tree_props_trunk_get(context)) {
    glbrush_hdelete(*tree_props_trunk_get(context));
    glbrush_hdelete(*tree_props_leaves_get(context));
  }
}

void tree_props_context_set(rendering_context*restrict context) {
  glbrush_handle_info info;
  const colour_palettes* palettes = get_colour_palettes(context);

  info.decay = 0.2f;
  info.noise = 0.75f;
  info.pallet = palettes->oak_trunk;
  info.pallet_size = lenof(palettes->oak_trunk);
  glbrush_hset(tree_props_trunk_getm(context), &info);

  info.decay = 0;
  info.noise = 0.75f;
  info.pallet = palettes->oak_leaf;
  info.pallet_size = lenof(palettes->oak_leaf);
  glbrush_hset(tree_props_leaves_getm(context), &info);
}

static lsystem oak_tree_system;

void tree_props_init(void) {
  /* 9..6: Branch lengths (movement)
   * -   : Draw branch (full size)
   * _   : Draw branch (scale by progression)
   * A   : Leaf splotch (full size)
   * B   : Leaf splotch (scale by progression)
   * [  ]: Push/pop
   * (  ): Push/pop with size reduction
   * a..h: Rotations (10 degree increments, a = 30 deg)
   * q..v: Forks (15/30/45/60/75/90 degrees)
   * .   : Potential branching point
   * z   : Random rotation on next iteration
   */
  lsystem_compile(
    &oak_tree_system,
    "9 8[z.]8",
    "8 7[z.]7",
    "7 6[z.]6",
    "B A",
    "_ -",
    ". . [(q_9B)z.] [(r_9B)z.] [(s_9B)z.] [(t_9B)z.] [(u_9B)z.] [(v_9B)z.]",
    "z a b c d e f g h",
    NULL);
}

static void render_tree_prop_oak(drawing_queue*, const world_prop*,
                                  const basic_world*,
                                  unsigned, fraction,
                                  const rendering_context*restrict);
static const prop_renderer tree_prop_renderers_[] = {
  0,
  render_tree_prop_oak,
};
const prop_renderer*const tree_prop_renderers = tree_prop_renderers_;

static void render_tree_prop_oak(drawing_queue* queue, const world_prop* this,
                                 const basic_world* world,
                                 unsigned base_level, fraction progression,
                                 const rendering_context*restrict context) {
  turtle_state turtle[17];
  unsigned level, depth = 0, size_shift = 0, i;
  unsigned screen_width = CTXTINV(context)->screen_width;
  vc3 root;
  glbrush_spec trunk_brush, leaf_brush;
  glbrush_accum accum = { 1.5f / 0.2f, this->x ^ this->z };
  lsystem_state sys;
  coord_offset base_size, trunk_size;

  root[0] = this->x;
  root[1] = terrain_base_y(world, this->x, this->z);
  root[2] = this->z;

  if (!turtle_init(&turtle[0], CTXTINV(context)->proj, root, TURTLE_UNIT))
    return;

  /* Don't do anything of too far behind the camera */
  if (simd_vs(turtle[0].pos.curr, 2) > 4*METRE) return;

  base_size = 8*METRE + this->variant * (METRE / 64);
  if (base_level < 32)
    base_size = base_size * (base_level + 32) / 64;
  trunk_size = METRE + this->variant * (METRE / 256);

  glbrush_init(&trunk_brush, *tree_props_trunk_get(context));
  trunk_brush.xscale = fraction_of(12);
  trunk_brush.yscale = fraction_of(2);
  trunk_brush.screen_width = screen_width;
  trunk_brush.base_distance = accum.distance;
  trunk_brush.random_seed = accum.rand;
  glbrush_init(&leaf_brush, *tree_props_leaves_get(context));
  leaf_brush.xscale = fraction_of(4);
  leaf_brush.yscale = fraction_of(4);
  leaf_brush.screen_width = screen_width;
  leaf_brush.base_distance = 0;
  leaf_brush.random_seed = accum.rand;

  /* Move level to a less linear scale. */
  if      (base_level < 32) level = 0, progression = fraction_of(1);
  else if (base_level== 32) level = 1;
  else if (base_level < 40) level = 1, progression = fraction_of(1);
  else if (base_level== 40) level = 2;
  else if (base_level < 48) level = 2, progression = fraction_of(1);
  else if (base_level== 48) level = 3;
  else if (base_level < 52) level = 3, progression = fraction_of(1);
  else if (base_level== 52) level = 4;
  else if (base_level < 55) level = 4, progression = fraction_of(1);
  else if (base_level== 55) level = 5;
  else if (base_level < 58) level = 5, progression = fraction_of(1);
  else if (base_level== 58) level = 6;
  else                      level = 6, progression = fraction_of(1);

  lsystem_execute(&sys, &oak_tree_system, "_9B", level, this->x^this->z);

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

    case '_':
      turtle[depth+1] = turtle[depth];
      turtle_move(turtle+depth+1, 0,
                  (base_size >> size_shift)
                  / TURTLE_UNIT,
                  0);
      turtle_draw_line(&accum, &trunk_brush, turtle+depth+1,
                       fraction_umul(trunk_size >> size_shift,
                                     progression),
                       fraction_umul(trunk_size >> size_shift,
                                     progression),
                       screen_width);
      glbrush_flush(&accum, &trunk_brush);
      break;

    case '-':
      turtle[depth+1] = turtle[depth];
      turtle_move(turtle+depth+1, 0,
                  (base_size >> size_shift)
                  / TURTLE_UNIT,
                  0);
      turtle_draw_line(&accum, &trunk_brush, turtle+depth+1,
                       trunk_size >> size_shift, trunk_size >> size_shift,
                       screen_width);
      glbrush_flush(&accum, &trunk_brush);
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
      turtle_draw_point(&accum, &leaf_brush, turtle+depth,
                        4*METRE +
                        (base_level > 48? 0 :
                         base_level > 16? (48 - base_level)*16*METRE / 32 :
                         16*METRE),
                        screen_width);
      break;

    case 'B':
      turtle_draw_point(&accum, &leaf_brush, turtle+depth,
                        fraction_umul(
                          4*METRE +
                          (base_level > 48? 0 :
                           base_level > 16? (48 - base_level)*16*METRE / 32 :
                           16*METRE), progression),
                        screen_width);
      break;
    }
  }
}
