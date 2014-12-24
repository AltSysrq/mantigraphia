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

#include <string.h>
#include <stdlib.h>

#include "../alloc.h"
#include "../micromp.h"
#include "../math/coords.h"
#include "env-vmap.h"

env_vmap* env_vmap_new(coord xmax, coord zmax, int is_toroidal,
                       const env_voxel_context_map* context_map) {
  size_t voxels_sz = sizeof(env_voxel_type) * xmax * zmax;
  char* raw;
  env_vmap* this;

  raw = xmalloc(sizeof(env_vmap) + UMP_CACHE_LINE_SZ + voxels_sz);
  this = (env_vmap*)raw;
  this->voxels = align_to_cache_line(raw + sizeof(*this));
  this->xmax = xmax;
  this->zmax = zmax;
  this->is_toroidal = is_toroidal;
  this->context_map = context_map;
  memset(this->voxels, 0, voxels_sz);

  return this;
}

void env_vmap_delete(env_vmap* this) {
  free(this);
}

env_voxel_contextual_type env_vmap_expand_type(
  const env_vmap* this, coord x, coord y, coord z
) {
  env_voxel_type base_type;
  env_voxel_contextual_type context_type;

  base_type = this->voxels[env_vmap_offset(this, x, y, z)];
  context_type =
    ((env_voxel_contextual_type)base_type) << ENV_VOXEL_CONTEXT_BITS;

#define CHECK(bitfield, axis, offset, max, toroidal)                    \
  do {                                                                  \
    if ((this->context_map->defs[base_type].sensitivity & (bitfield)) && \
        (((coord)((axis)+(offset))) < (coord)(max) || (toroidal))) {    \
      axis += (offset);                                                 \
      axis &= ((max)-1);                                                \
      if (env_voxel_context_def_is_in_family(                           \
            this->context_map->defs + base_type,                        \
            this->voxels[env_vmap_offset(this, x, y, z)])) {            \
        context_type |= (bitfield);                                     \
      }                                                                 \
      axis -= (offset);                                                 \
      axis &= ((max)-1);                                                \
    }                                                                   \
  } while (0)

  CHECK(ENV_VOXEL_CONTEXT_YP, y, +1, ENV_VMAP_H, 0);
  CHECK(ENV_VOXEL_CONTEXT_YN, y, -1, ENV_VMAP_H, 0);
  CHECK(ENV_VOXEL_CONTEXT_XP, x, +1, this->xmax, this->is_toroidal);
  CHECK(ENV_VOXEL_CONTEXT_XN, x, -1, this->xmax, this->is_toroidal);
  CHECK(ENV_VOXEL_CONTEXT_ZP, z, +1, this->zmax, this->is_toroidal);
  CHECK(ENV_VOXEL_CONTEXT_ZN, z, -1, this->zmax, this->is_toroidal);
#undef CHECK

  return context_type;
}
