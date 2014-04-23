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
#include "../defs.h"
#include "../graphics/glpencil.h"
#include "../graphics/glbrush.h"
#include "../graphics/perspective.h"
#include "../graphics/dm-proj.h"
#include "../world/basic-world.h"
#include "../world/grass-props.h"
#include "../world/terrain.h"
#include "props.h"
#include "context.h"
#include "colour-palettes.h"
#include "grass-props.h"

static const struct {
  /* The month in which the flower appears. It grows over this month, and fades
   * over the following.
   */
  unsigned month;
  coord size;
  fraction scale;
  float noise;
  canvas_pixel colours[8];
} flower_types[MAX_GRASS_TYPE] = {
  { 1, 300 * MILLIMETRE, fraction_of(32), 0.75f,
    { 0xffffffaa, 0xffffffbb, 0xffffffaa, 0xffffff88,
      0xffcccc66, 0xffcccc44, 0xffcccc22, 0xffeeee00 } },
  { 3, 300 * MILLIMETRE, fraction_of(32), 1.0f,
    { 0xffffffff, 0xffffffdd, 0xffffdddd, 0xffffccbb,
      0xffcc6666, 0xffcc4422, 0xffcc2222, 0xffee8888 } },
  { 5, 300 * MILLIMETRE, fraction_of(32), 0.75f,
    { 0xffaaaaff, 0xffbbbbff, 0xffccccff, 0xffddddff,
      0xffaaaacc, 0xffbbbbcc, 0xffccccdd, 0xffddddee } },
  { 1, 400 * MILLIMETRE, fraction_of(32), 1.25f,
    { 0xffff0000, 0xffff7700, 0xffff7700, 0xffff0000,
      0xffa02000, 0xffa04000, 0xffe05040, 0xffe06060 } },
  { 2, 600 * MILLIMETRE, fraction_of(32), 1.75f,
    { 0xffffaa00, 0xffff7700, 0xffff7700, 0xffffff00,
      0xffa0a000, 0xffa06000, 0xffe0e040, 0xffe0e060 } },
  { 3, 500 * MILLIMETRE, fraction_of(32), 1.50f,
    { 0xff0000ff, 0xff6020ff, 0xff2010aa, 0xff8020aa,
      0xff0020aa, 0xff8080ff, 0xff6060aa, 0xff2010aa } },
  { 4, 700 * MILLIMETRE, fraction_of(32), 1.8f,
    { 0xffffcccc, 0xffcc8888, 0xffffff00, 0xffcccc00,
      0xffcc6666, 0xffffcccc, 0xffeee000, 0xffffff00 } },
  { 5, 400 * MILLIMETRE, fraction_of(32), 0.75f,
    { 0xffff00ff, 0xffff88ff, 0xffaa00aa, 0xffaa55aa,
      0xff880088, 0xff884488, 0xff660066, 0xff663366 } },
};

typedef struct {
  glpencil_handle* pencil;
  glbrush_handle* flower_brushes[MAX_GRASS_TYPE];
  coord base_height;
  unsigned thickness;
} grass_props_context_info;
RENDERING_CONTEXT_STRUCT(grass_props, grass_props_context_info)

void grass_props_context_ctor(rendering_context*restrict ctxt) {
  grass_props_context_info* info = grass_props_getm(ctxt);
  memset(info, 0, sizeof(*info));
}

void grass_props_context_set(rendering_context*restrict ctxt) {
  unsigned i;
  glbrush_handle_info brush;
  grass_props_context_info* info = grass_props_getm(ctxt);
  glpencil_handle_info pencil = {
    CTXTINV(ctxt)->screen_width / 640
  };

  info->thickness = pencil.thickness;

  if (!info->pencil) {
    info->pencil = glpencil_hnew(&pencil);
    for (i = 0; i < MAX_GRASS_TYPE; ++i) {
      brush.decay = 0;
      brush.noise = flower_types[i].noise;
      brush.pallet = flower_types[i].colours;
      brush.pallet_size = lenof(flower_types[i].colours);
      info->flower_brushes[i] = glbrush_hnew(&brush);
    }
  }

  info->base_height = METRE / 8;
  if (0 == CTXTINV(ctxt)->month_integral)
    info->base_height = fraction_umul(info->base_height,
                                      CTXTINV(ctxt)->month_fraction);
  else if (8 == CTXTINV(ctxt)->month_integral)
    info->base_height = fraction_umul(info->base_height,
                                      fraction_of(1) -
                                      CTXTINV(ctxt)->month_fraction);
  info->base_height += METRE / 8;
}

void grass_props_context_dtor(rendering_context*restrict ctxt) {
  grass_props_context_info* info = grass_props_getm(ctxt);
  unsigned i;
  if (info->pencil) {
    glpencil_hdelete(info->pencil);
    for (i = 0; i < MAX_GRASS_TYPE; ++i)
      glbrush_hdelete(info->flower_brushes[i]);
  }
}

static void render_grass_prop_simple(const world_prop*,
                                     const basic_world*,
                                     unsigned, fraction,
                                     const rendering_context*restrict);

static const prop_renderer grass_prop_renderers_[1 + MAX_GRASS_TYPE] = {
  0,
  render_grass_prop_simple,
  render_grass_prop_simple,
  render_grass_prop_simple,
  render_grass_prop_simple,
  render_grass_prop_simple,
  render_grass_prop_simple,
  render_grass_prop_simple,
  render_grass_prop_simple,
};
const prop_renderer*const grass_prop_renderers = grass_prop_renderers_;

static void render_grass_prop_simple(const world_prop* this,
                                     const basic_world* world,
                                     unsigned level, fraction progression,
                                     const rendering_context*restrict context) {
  const perspective*restrict proj = CTXTINV(context)->proj;
  glpencil_spec pencil;
  glbrush_spec brush;
  glbrush_accum baccum;
  vc3 base, tip;
  vo3 pbase, ptip;
  const colour_palettes* palettes = get_colour_palettes(context);
  const grass_props_context_info* info = grass_props_get(context);
  coord flower_size;
  fraction size_frac;
  zo_scaling_factor flower_scale;

  glpencil_init(&pencil, info->pencil);
  pencil.colour = palettes->grass[this->variant % NUM_GRASS_COLOUR_VARIANTS][
    (world->tiles[basic_world_offset(world,
                                     this->x / TILE_SZ,
                                     this->z / TILE_SZ)]
     .elts[0].type) & ((1 << TERRAIN_SHADOW_BITS) - 1)];

  base[0] = tip[0] = this->x;
  base[2] = tip[2] = this->z;
  base[1] = terrain_base_y(world, this->x, this->z);
  tip[1] = base[1] +
    (grass_props_get(context)->base_height +
     METRE/4 * this->variant / 256) * level / 64;

  if (!perspective_proj(pbase, base, proj) ||
      !perspective_proj(ptip, tip, proj))
    return;

  glpencil_draw_line(NULL, &pencil, pbase, 0, ptip, 0);
  glpencil_flush(NULL, &pencil);

  if (CTXTINV(context)->month_integral == flower_types[this->type-1].month ||
      CTXTINV(context)->month_integral == flower_types[this->type-1].month+1) {
    flower_size = flower_types[this->type-1].size;
    if (CTXTINV(context)->month_integral == flower_types[this->type-1].month)
      size_frac = CTXTINV(context)->month_fraction;
    else
      size_frac = fraction_of(1) - CTXTINV(context)->month_fraction;

    flower_size = fraction_umul(flower_size, size_frac);

    glbrush_init(&brush, info->flower_brushes[this->type-1]);
    brush.xscale = flower_types[this->type-1].scale;
    brush.yscale = flower_types[this->type-1].scale;
    brush.texoff = brush.base_distance = 0;
    baccum.rand = brush.random_seed = this->variant;
    brush.screen_width = CTXTINV(context)->screen_width;
    baccum.distance = 0;
    flower_scale = dm_proj_calc_weight(brush.screen_width, proj,
                                       ptip[2], flower_size);
    if (zo_scale(brush.screen_width, flower_scale) <
        fraction_smul(info->thickness*4, size_frac))
      flower_scale = fraction_umul(
        ZO_SCALING_FACTOR_MAX * info->thickness * 4 /
        brush.screen_width,
        size_frac);
    glbrush_draw_point(&baccum, &brush, ptip, flower_scale);
    glbrush_flush(&baccum, &brush);
  }
}
