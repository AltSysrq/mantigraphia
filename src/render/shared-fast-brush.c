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

#include "../graphics/abstract.h"
#include "../graphics/brush.h"
#include "../graphics/fast-brush.h"
#include "draw-queue.h"
#include "context.h"
#include "shared-fast-brush.h"

RENDERING_CONTEXT_STRUCT(shared_fast_brush, drawing_method*)

/* The ctor just initialises the value to NULL. We can't actually create it
 * until we know the screen width, which we find out in the first call to
 * _set().
 */
void shared_fast_brush_context_ctor(rendering_context*restrict context) {
  *shared_fast_brush_getm(context) = NULL;
}

void shared_fast_brush_context_set(rendering_context*restrict context) {
  unsigned sw =
    ((const rendering_context_invariant*restrict)context)->screen_width;
  brush_spec brush;
  unsigned i;

  /* Do nothing if already initialised */
  if (*shared_fast_brush_get(context)) return;

  brush_init(&brush);
  for (i = 0; i < MAX_BRUSH_BRISTLES; ++i)
    brush.init_bristles[i] = 1;

  brush.inner_strengthening_chance = 3680;
  brush.outer_strengthening_chance = 128;
  brush.inner_weakening_chance = 128;
  brush.outer_weakening_chance = 1000;

  *shared_fast_brush_getm(context) = fast_brush_new(&brush, sw/16, sw*2, 0);
}

void shared_fast_brush_context_dtor(rendering_context*restrict context) {
  fast_brush_delete(*shared_fast_brush_get(context));
}

void dq_shared_fast_brush(drawing_queue_burst* burst,
                          const rendering_context*restrict context) {
  DQMETHPTR(*burst, *shared_fast_brush_get(context));
}
