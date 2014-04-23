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

#include <assert.h>
#include <stdlib.h>
#include <string.h>

#include "hspace.h"

static int hspace_cmp(const hspace* a, const hspace* b) {
  if (a->lower[0] < b->lower[0]) return -1;
  if (a->lower[0] > b->lower[0]) return +1;
  return 0;
}

void add_hspace(hspace_map* map, const hspace* space) {
  assert(map->num_spaces < MAX_HSPACES);
  memcpy(map->spaces + map->num_spaces, space, sizeof(hspace));
  ++map->num_spaces;

  /* While quicksort is not terribly good at already-sorted arrays, the small
   * size here makes the O(n^2) runtime negligible.
   */
  qsort(map->spaces, map->num_spaces, sizeof(hspace),
        (int(*)(const void*,const void*))hspace_cmp);
}

/* Since we guarantee that the X ranges of no two hspaces overlap, we can
 * consider a vc3 "equal" to an hspace if it is contained within. If a point
 * lies outside the X range, return -1 or +1 as appropriate. If it is within
 * that range but is not contained in the hspace, return non-zero arbitrarily
 * so that the bsearch is unsuccessful.
 */
static int hspace_test(const vc3 point, const hspace* hs) {
  if (point[0] < hs->lower[0]) return -1;
  if (point[0] >= hs->upper[0]) return +1;

  /* We already know that hs->x0 <= point[0] < hs->x1, so that check can be
   * elided in this bounds check.
   */
  if (point[1] >= hs->lower[1] && point[1] < hs->upper[1] &&
      point[2] >= hs->lower[2] && point[2] < hs->upper[2])
    return 0;
  else
    return -1; /* arbitrary */
}

const hspace* get_hspace(const hspace_map* map, const vc3 where) {
  return bsearch(where, map->spaces, map->num_spaces, sizeof(hspace),
                 (int(*)(const void*,const void*))hspace_test);
}
