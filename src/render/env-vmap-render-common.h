/*-
 * Copyright (c) 2015 Jason Lingle
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
#ifndef RENDER_ENV_VMAP_RENDER_COMMON_H_
#define RENDER_ENV_VMAP_RENDER_COMMON_H_

/**
 * @file
 *
 * Common routines used by the vmap renderers.
 */

#include "../bsd.h"
#include "../world/env-vmap.h"
#include "env-voxel-graphic.h"

static unsigned env_vmap_renderer_ll_majority_component(
  unsigned long long) __unused;
static const env_voxel_graphic* env_vmap_renderer_get_graphic(
  const env_voxel_graphic*const*, const env_vmap*,
  coord, coord, coord, unsigned char) __unused;

/* This uses an O(n) algorithm to find the majority component as an
 * approximation of the statistical mode of a cell (read in as a single qword).
 *
 * Note that processors of different endianness will produce slightly different
 * results in cases where there is no majority component. Since this is only
 * used for graphics, this isn't really an issue.
 */
static unsigned env_vmap_renderer_ll_majority_component(
  unsigned long long bytes
) {
  unsigned m, n, b;

  if (!bytes) return 0;

  m = bytes & 0xFF;
  n = 1;
#define BYTE()                                  \
  bytes >>= 8;                                  \
  b = bytes & 0xFF;                             \
  if (m == b) ++n;                              \
  else if (!--n) m = b, n = 1
  BYTE(); /* 1 */
  BYTE(); /* 2 */
  BYTE(); /* 3 */
  BYTE(); /* 4 */
  BYTE(); /* 5 */
  BYTE(); /* 6 */
  BYTE(); /* 7 */
#undef BYTE
  return m;
}

static const env_voxel_graphic* env_vmap_renderer_get_graphic(
  const env_voxel_graphic*const* graphics,
  const env_vmap* vmap,
  coord x, coord y, coord z,
  unsigned char lod
) {
  const unsigned long long* cell;

  switch (lod) {
  case 0:
    return graphics[env_vmap_expand_type(vmap, x, y, z)];

  case 1:
  case 2:
  case 3:
    cell = (unsigned long long*)
      (vmap->voxels + env_vmap_offset(vmap, x, y, z));
    return graphics[
      env_vmap_renderer_ll_majority_component(*cell) <<
      ENV_VOXEL_CONTEXT_BITS];
  }

  /* unreachable */
  abort();
}

#endif /* RENDER_ENV_VMAP_RENDER_COMMON_H_ */
