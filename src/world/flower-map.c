/*-
 * Copyright (c) 2015 Jason Llngle
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

#include <stddef.h>

#include "../alloc.h"
#include "../math/coords.h"
#include "terrain-tilemap.h"
#include "flower-map.h"

static void flower_fhive_init(flower_fhive*);
static void flower_fhive_destroy(flower_fhive*);
static void flower_fhive_put(flower_fhive*, flower_type, flower_height,
                             flower_coord, flower_coord);

flower_map* flower_map_new(unsigned tiles_w, unsigned tiles_h) {
  flower_map* this;
  unsigned fhives_w, fhives_h, i;

  fhives_w = tiles_w / FLOWER_FHIVE_SIZE;
  fhives_h = tiles_h / FLOWER_FHIVE_SIZE;

  this = xmalloc(offsetof(flower_map, hives) +
                 sizeof(flower_fhive) * fhives_w * fhives_h);
  this->fhives_w = fhives_w;
  this->fhives_h = fhives_h;

  for (i = 0; i < fhives_w * fhives_h; ++i)
    flower_fhive_init(this->hives + i);

  return this;
}

static void flower_fhive_init(flower_fhive* this) {
  this->size = 0;
  this->cap = 32;
  this->flowers = xmalloc(this->cap * sizeof(flower_desc));
}

void flower_map_delete(flower_map* this) {
  unsigned i;

  for (i = 0; i < this->fhives_w * this->fhives_h; ++i)
    flower_fhive_destroy(this->hives + i);

  free(this);
}

static void flower_fhive_destroy(flower_fhive* this) {
  free(this->flowers);
}

void flower_map_put(flower_map* this, flower_type type, flower_height height,
                    coord wx, coord wz) {
  unsigned fhx, fhz;
  flower_fhive* fhive;

  fhx = wx / TILE_SZ / FLOWER_FHIVE_SIZE;
  fhz = wz / TILE_SZ / FLOWER_FHIVE_SIZE;
  fhive = this->hives + flower_map_fhive_offset(this, fhx, fhz);
  flower_fhive_put(fhive, type, height,
                   wx / FLOWER_COORD_UNIT, wz / FLOWER_COORD_UNIT);
}

static void flower_fhive_put(flower_fhive* this, flower_type type,
                             flower_height height,
                             flower_coord x, flower_coord z) {
  if (this->size == this->cap) {
    this->cap *= 2;
    this->flowers = xrealloc(this->flowers,
                             this->cap * sizeof(flower_desc));
  }

  this->flowers[this->size].type = type;
  this->flowers[this->size].y = height;
  this->flowers[this->size].x = x;
  this->flowers[this->size].z = z;
  ++this->size;
}

unsigned flower_map_fhive_offset(const flower_map* this,
                                 unsigned x,
                                 unsigned z) {
  return z * this->fhives_w + x;
}
