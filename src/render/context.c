/*-
 * Copyright (c) 2013 Jason Lingle
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
#include <string.h>
#include <assert.h>

#include "../alloc.h"
#include "world.h"
#include "context.h"

static size_t space_for_invariant(size_t);

static size_t (*const context_put[])(size_t) = {
  space_for_invariant,
  render_basic_world_put_context_offset,
  NULL
};

static void (*const context_set[])(rendering_context*restrict) = {
  NULL
};

static void (*const context_ctor[])(rendering_context*restrict) = {
  render_basic_world_context_ctor,
  NULL
};

static void (*const context_dtor[])(rendering_context*restrict) = {
  render_basic_world_context_dtor,
  NULL
};

static size_t space_for_invariant(size_t zero) {
  assert(!zero);
  return sizeof(rendering_context_invariant);
}

rendering_context* rendering_context_new(void) {
  rendering_context* this;
  size_t off = 0;
  unsigned i;

  for (i = 0; context_put[i]; ++i) {
    off = (*context_put[i])(off);
    /* Align to pointer */
    off += sizeof(void*) - 1;
    off &= ~(sizeof(void*) - 1);
  }

  this = xmalloc(off);
  for (i = 0; context_ctor[i]; ++i)
    (*context_ctor[i])(this);

  return this;
}

void rendering_context_delete(rendering_context* this) {
  unsigned i;

  for (i = 0; context_dtor[i]; ++i)
    (*context_dtor[i])(this);

  free(this);
}

void rendering_context_set(rendering_context*restrict this,
                           const rendering_context_invariant*restrict inv) {
  unsigned i;
  memcpy(this, inv, sizeof(*inv));
  for (i = 0; context_set[i]; ++i)
    (*context_set[i])(this);
}
