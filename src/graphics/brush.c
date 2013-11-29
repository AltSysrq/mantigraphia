/*
  Copyright (c) 2013 Jason Lingle
  All rights reserved.

  Redistribution and use in source and binary forms, with or without
  modification, are permitted provided that the following conditions
  are met:
  1. Redistributions of source code must retain the above copyright
     notice, this list of conditions and the following disclaimer.
  2. Redistributions in binary form must reproduce the above copyright
     notice, this list of conditions and the following disclaimer in the
     documentation and/or other materials provided with the distribution.
  3. Neither the name of the author nor the names of its contributors
     may be used to endorse or promote products derived from this software
     without specific prior written permission.

     THIS SOFTWARE IS PROVIDED BY THE AUTHORS AND CONTRIBUTORS ``AS IS'' AND
     ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
     IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
     ARE DISCLAIMED.  IN NO EVENT SHALL THE AUTHORS OR CONTRIBUTORS BE LIABLE
     FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
     DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
     OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
     HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
     LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
     OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
     SUCH DAMAGE.
*/
#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include <string.h>
#include <stdlib.h>

#include "../coords.h"
#include "canvas.h"
#include "abstract.h"
#include "brush.h"

void brush_init(brush_spec* spec) {
  spec->meth.draw_point = (drawing_method_draw_point_t)brush_draw_point;
  spec->meth.draw_line = (drawing_method_draw_line_t)brush_draw_line;
  spec->meth.flush = (drawing_method_flush_t)brush_flush;

  spec->bristles = 16;
  spec->inner_strengthening_chance = 256;
  spec->outer_strengthening_chance = 128;
  spec->inner_weakening_chance = 368;
  spec->outer_weakening_chance = 400;
  spec->noise = 0x1;
  spec->size = ZO_SCALING_FACTOR_MAX / 512;
  spec->step = ZO_SCALING_FACTOR_MAX / 1024;
  memset(spec->init_bristles, 0, sizeof(spec->init_bristles));
}

void brush_prep(brush_accum* accum, const brush_spec* spec,
                canvas* dst, unsigned random_seed) {
  unsigned px_per_step = zo_scale(dst->w, spec->step);

  memcpy(accum->bristles, spec->init_bristles, sizeof(accum->bristles));
  accum->dst = dst;
  accum->random_state = random_seed;
  accum->basic_size = zo_scale(dst->w, spec->size);
  if (!px_per_step) px_per_step = 1;
  accum->step_size = ZO_SCALING_FACTOR_MAX / px_per_step;
}

void brush_draw_point(brush_accum* accum, const brush_spec* spec,
                      const vo3 where, zo_scaling_factor weight) {
  /* TODO */
}

void brush_draw_line(brush_accum* accum, const brush_spec* spec,
                     const vo3 from, zo_scaling_factor from_weight,
                     const vo3 to, zo_scaling_factor to_weight) {
  /* TODO */
}

void brush_flush(brush_accum* accum, const brush_spec* spec) {
  memcpy(accum->bristles, spec->init_bristles, sizeof(accum->bristles));
}
