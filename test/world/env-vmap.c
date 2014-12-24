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

#include "test.h"
#include "micromp.h"
#include "world/env-vmap.h"

defsuite(env_vmap);

static env_vmap* vmap;
static env_voxel_context_map cmap;

#define T(n) ((n) << (ENV_VOXEL_CONTEXT_BITS))
#define C(dir) |GLUE(ENV_VOXEL_CONTEXT_,dir)

defsetup {
  vmap = NULL;
  memset(&cmap, 0, sizeof(cmap));
}

defteardown {
  env_vmap_delete(vmap);
}

deftest(voxels_aligned_to_cache_line) {
  vmap = env_vmap_new(1, 1, 0, &cmap);

  ck_assert_int_eq(0, ((size_t)vmap->voxels) & (UMP_CACHE_LINE_SZ-1));
}

deftest(voxel_can_be_sensitive_to_all_directions) {
  vmap = env_vmap_new(1, 1, 1, &cmap);

  env_voxel_context_def_set_in_family(cmap.defs + 1, 1, 1);
  env_voxel_context_def_set_in_family(cmap.defs + 1, 2, 1);
  cmap.defs[1].sensitivity = 0 C(XP) C(XN) C(YP) C(YN) C(ZP) C(ZN);
  vmap->voxels[env_vmap_offset(vmap, 0, 0, 0)] = 2;
  vmap->voxels[env_vmap_offset(vmap, 0, 1, 0)] = 1;
  vmap->voxels[env_vmap_offset(vmap, 0, 2, 0)] = 1;

  ck_assert_int_eq(T(1) C(XP) C(XN) C(YP) C(YN) C(ZP) C(ZN),
                   env_vmap_expand_type(vmap, 0, 1, 0));
}

deftest(voxel_can_be_sensitive_to_no_directions) {
  vmap = env_vmap_new(1, 1, 1, &cmap);

  env_voxel_context_def_set_in_family(cmap.defs + 1, 1, 1);
  env_voxel_context_def_set_in_family(cmap.defs + 1, 2, 1);
  cmap.defs[1].sensitivity = 0;
  vmap->voxels[env_vmap_offset(vmap, 0, 0, 0)] = 2;
  vmap->voxels[env_vmap_offset(vmap, 0, 1, 0)] = 1;
  vmap->voxels[env_vmap_offset(vmap, 0, 2, 0)] = 1;

  ck_assert_int_eq(T(1), env_vmap_expand_type(vmap, 0, 1, 0));
}

deftest(non_toroidal_boundaries_are_respected) {
  vmap = env_vmap_new(1, 1, 0, &cmap);

  env_voxel_context_def_set_in_family(cmap.defs + 1, 1, 1);
  env_voxel_context_def_set_in_family(cmap.defs + 1, 2, 1);
  cmap.defs[1].sensitivity = 0 C(XP) C(XN) C(YP) C(YN) C(ZP) C(ZN);
  vmap->voxels[env_vmap_offset(vmap, 0, 0, 0)] = 2;
  vmap->voxels[env_vmap_offset(vmap, 0, 1, 0)] = 1;
  vmap->voxels[env_vmap_offset(vmap, 0, 2, 0)] = 1;

  ck_assert_int_eq(T(1) C(YP) C(YN),
                   env_vmap_expand_type(vmap, 0, 1, 0));
}

deftest(only_voxels_in_source_family_are_counted) {
  vmap = env_vmap_new(1, 1, 0, &cmap);
  /* 2 is considered family of 1, and 1 is considered family of 3. Neither
   * relation is mutual.
   *
   * The voxels are stacked 2,1,3, so 1 should only have a YN context.
   */
  env_voxel_context_def_set_in_family(cmap.defs + 1, 2, 1);
  env_voxel_context_def_set_in_family(cmap.defs + 3, 1, 1);
  cmap.defs[1].sensitivity = 0 C(XP) C(XN) C(YP) C(YN) C(ZP) C(ZN);
  vmap->voxels[env_vmap_offset(vmap, 0, 0, 0)] = 2;
  vmap->voxels[env_vmap_offset(vmap, 0, 1, 0)] = 1;
  vmap->voxels[env_vmap_offset(vmap, 0, 2, 0)] = 3;

  ck_assert_int_eq(T(1) C(YN), env_vmap_expand_type(vmap, 0, 1, 0));
}
