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
#include <stddef.h>

#include "../alloc.h"
#include "../bsd.h"
#include "world.h"

basic_world* basic_world_new(coord xmax, coord zmax, coord xmin, coord zmin) {
  size_t memory_reqd = 0, bw_mem = offsetof(basic_world, tiles);
  coord xs, zs;
  basic_world* world, * prev = NULL;
  char* base, * curr;
  /* Dummy head so that we can clear the next field correctly */
  SLIST_HEAD(, basic_world_s) dummy_head;

  /* Below, we allocate all worlds in one allocation. We don't manually handle
   * alignment, as xs*zs is always an even number when there will be a next
   * world, and even*sizeof(tile_info) is a multiple of eight, so any alignment
   * requirements will always be met.
   */

  /* Count required memory */
  for (xs = xmax, zs = zmax; xs >= xmin && zs >= zmin; xs /= 2, zs /= 2) {
    memory_reqd += bw_mem + xs*zs*sizeof(tile_info);
  }

  base = curr = xmalloc(memory_reqd);
  /* Initialise values */
  for (xs = xmax, zs = zmax; xs >= xmin && zs >= zmin; xs /= 2, zs /= 2) {
    world = (basic_world*)curr;
    curr += bw_mem + xs*zs*sizeof(tile_info);

    /* Set bounds */
    world->xmax = xs;
    world->zmax = zs;
    /* Add to list */
    if (prev) {
      SLIST_INSERT_AFTER(prev, world, next);
    } else {
      SLIST_INIT(&dummy_head);
      SLIST_INSERT_HEAD(&dummy_head, world, next);
    }
    prev = world;
  }

  return (basic_world*)base;
}

void basic_world_delete(basic_world* this) {
  free(this);
}

void basic_world_patch_next(basic_world* large, coord x, coord z) {
  basic_world* small;
  coord sx, sz;
  unsigned ox, oz, elt, alt, strongest, loff, soff;

  /* Shift to base offset for reduced tile */
  x &= ~1u;
  z &= ~1u;

  while ((small = SLIST_NEXT(large, next))) {
    sx = x/2;
    sz = z/2;
    soff = basic_world_offset(small, sx, sz);

    /* Select strongest element at each index for the combined value */
    for (elt = 0; elt < TILE_NELT; ++elt) {
      strongest = 256;
      for (oz = 0; oz < 2; ++oz) {
        for (ox = 0; ox < 2; ++ox) {
          loff = basic_world_offset(large,
                                    (x+ox) & (large->xmax-1),
                                    (z+oz) & (large->zmax-1));
          if (large->tiles[loff].elts[elt].type < strongest) {
            strongest = large->tiles[loff].elts[elt].type;
            memcpy(&small->tiles[soff].elts[elt],
                   &large->tiles[loff].elts[elt],
                   sizeof(tile_element));
          }
        }
      }
    }

    /* Terrain altitude is the average of the four */
    alt = 0;
    for (oz = 0; oz < 2; ++oz) {
      for (ox = 0; ox < 2; ++ox) {
        loff = basic_world_offset(large,
                                  (x+ox) & (large->xmax-1),
                                  (z+oz) & (large->zmax-1));
        alt += large->tiles[loff].elts[0].altitude;
      }
    }
    small->tiles[soff].elts[0].altitude = alt/4;

    /* Move to next level */
    large = small;
    x = sx;
    z = sz;
  }
}

void basic_world_calc_next(basic_world* this) {
  coord x, z;

  for (z = 0; z < this->zmax; z += 2) {
    for (x = 0; x < this->xmax; x += 2) {
      basic_world_patch_next(this, x, z);
    }
  }
}
