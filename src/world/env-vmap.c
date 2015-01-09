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

/* These return dbit indices, offset from the start of visiblity.
 *
 * byte index = offset/4
 * bit shift = offset%4
 */
static inline unsigned env_vmap_visibility2_offset(
  const env_vmap* this, coord x, coord y, coord z
) {
  return z/2 * (this->xmax/2) * (ENV_VMAP_H/2) +
    x/2 * (ENV_VMAP_H/2) + y/2;
}

static inline unsigned env_vmap_visibility4_offset(
  const env_vmap* this, coord x, coord y, coord z
) {
  return (this->xmax/2) * (this->zmax/2) * ENV_VMAP_H/2 +
    z/4 * (this->xmax/4) * (ENV_VMAP_H/4) +
    x/4 * (ENV_VMAP_H/4) + y/4;
}

env_vmap* env_vmap_new(coord xmax, coord zmax, int is_toroidal,
                       const env_voxel_context_map* context_map) {
  size_t voxels_sz = sizeof(env_voxel_type) * xmax * zmax * ENV_VMAP_H;
  size_t visibility2_sz = (xmax/2) * (zmax/2) * (ENV_VMAP_H/2) / 4;
  size_t visibility4_sz = (xmax/4) * (zmax/4) * (ENV_VMAP_H/4) / 4;
  char* raw;
  env_vmap* this;

  raw = xmalloc(sizeof(env_vmap) + UMP_CACHE_LINE_SZ + voxels_sz +
                visibility2_sz + visibility4_sz);
  this = (env_vmap*)raw;
  this->voxels = align_to_cache_line(raw + sizeof(*this));
  this->visibility = (void*)(this->voxels + voxels_sz/sizeof(env_voxel_type));
  this->xmax = xmax;
  this->zmax = zmax;
  this->is_toroidal = is_toroidal;
  this->context_map = context_map;
  memset(this->voxels, 0, voxels_sz);
  memset(this->visibility, 0, visibility2_sz + visibility4_sz);

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

static void set_max_level(env_vmap* this,
                          unsigned offset,
                          unsigned char level) {
  unsigned byte = offset/4;
  unsigned field = offset%4 * 2;
  unsigned char mask, packed;

  level <<= field;
  mask = 3 << field;

  packed = this->visibility[byte];
  if (level > (packed & mask)) {
    packed &= ~mask;
    packed |= level;
    this->visibility[byte] = packed;
  }
}

void env_vmap_make_visible(env_vmap* this,
                           coord x, coord y, coord z,
                           unsigned char level) {
  set_max_level(this, env_vmap_visibility2_offset(this, x, y, z), level);
  set_max_level(this, env_vmap_visibility4_offset(this, x, y, z), level);
}

int env_vmap_is_visible(const env_vmap* this,
                        coord x, coord y, coord z,
                        unsigned char level) {
  unsigned offset, byte, field;
  unsigned char mask, packed;

  if (level <= 1)
    offset = env_vmap_visibility2_offset(this, x, y, z);
  else
    offset = env_vmap_visibility4_offset(this, x, y, z);

  byte = offset/4;
  field = offset%4 * 2;

  mask = 3 << field;
  level <<= field;

  packed = this->visibility[byte];
  return (packed & mask) >= level;
}
