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

#include <stddef.h>
#include <stdlib.h>
#include <string.h>

#include "../alloc.h"
#include "../coords.h"
#include "../frac.h"
#include "sybmap.h"

typedef struct {
  coord_offset min, max;
} sybmap_entry;

struct sybmap_s {
  unsigned w, h;
  sybmap_entry entries[1];
};

sybmap* sybmap_new(unsigned w, unsigned h) {
  sybmap* this = xmalloc(offsetof(sybmap, entries) +
                         w*sizeof(sybmap_entry));
  this->w = w;
  this->h = h;
  return this;
}

void sybmap_delete(sybmap* this) {
  free(this);
}

void sybmap_clear(sybmap* this) {
  unsigned x;

  for (x = 0; x < this->w; ++x) {
    this->entries[x].min = this->h;
    this->entries[x].max = 0;
  }
}

int sybmap_test(const sybmap* this,
                coord_offset xl, coord_offset xh,
                coord_offset yl, coord_offset yh) {
  coord_offset x;

  if (xl < 0) xl = 0;
  if (xh >= (signed)this->w) xh = this->w - 1;

  for (x = xl; x <= xh; ++x)
    if (yh > this->entries[x].max ||
        yl < this->entries[x].min)
      return 1;

  return 0;
}

void sybmap_put(sybmap* this, const vo3 vl, const vo3 vm, const vo3 vr) {
  const coord_offset* tmp;
  coord_offset vmmin, vmmax, x0, x1, x, ox, ymin, ymax;
  vo3 midpoint;
  coord_offset dx;
  fraction idx;

  /* Sort the vertices in ascending order of X coordinate */
  if (vm[0] < vl[0] && vm[0] < vr[0]) {
    /* input vm is left-most */
    tmp = vl;
    vl = vm;
    vm = tmp;
  } else if (vr[0] < vl[0]) {
    /* input vr is left-most */
    tmp = vl;
    vl = vr;
    vr = tmp;
  } /* else, vl is left-most */

  if (vr[0] < vm[0]) {
    /* vr is actually middle */
    tmp = vm;
    vm = vr;
    vr = tmp;
  }

  dx = vr[0] - vl[0];
  if (!dx) return; /* nothing to do */
  idx = fraction_of(dx);

  /* Calculate point between vl and vr with the same X as vm */
  midpoint[0] = vm[0];
  midpoint[1] = fraction_smul((vm[0]-vl[0])*vr[1] + (dx-(vm[0]-vl[0]))*vl[1],
                              idx);

  if (midpoint[1] < vm[1]) {
    vmmin = midpoint[1];
    vmmax = vm[1];
  } else {
    vmmin = vm[1];
    vmmax = midpoint[1];
  }

  /* Fill area between vl and vm */
  x0 = (vl[0] >= 0? vl[0] : 0);
  x1 = (vm[0] < (signed)this->w? vm[0] : (signed)this->w-1);
  for (x = x0; x <= x1; ++x) {
    ox = x - vl[0];
    ymin = fraction_smul(ox*vmmin + (dx-ox)*vl[1], idx);
    ymax = fraction_smul(ox*vmmax + (dx-ox)*vl[1], idx);
    if (ymin < this->entries[x].min)
      this->entries[x].min = ymin;
    if (ymax > this->entries[x].max)
      this->entries[x].max = ymax;
  }

  /* Fill area between vm and vr */
  x0 = (vm[0] >= 0? vm[0] : 0) + 1; /* offset since above loop included */
  x1 = (vr[0] < (signed)this->w? vr[0] : (signed)this->w-1);
  for (x = x0; x <= x1; ++x) {
    ox = x - vm[0];
    ymin = fraction_smul(ox*vr[1] + (dx-ox)*vmmin, idx);
    ymax = fraction_smul(ox*vr[1] + (dx-ox)*vmmax, idx);
    if (ymin < this->entries[x].min)
      this->entries[x].min = ymin;
    if (ymax > this->entries[x].max)
      this->entries[x].max = ymax;
  }
}
