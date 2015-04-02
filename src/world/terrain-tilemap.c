/*-
 * Copyright (c) 2013, 2014, 2015 Jason Lingle
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
#include "terrain-tilemap.h"
#include "terrain.h"

terrain_tilemap* terrain_tilemap_new(coord xmax, coord zmax,
                                     coord xmin, coord zmin) {
  size_t memory_reqd = 0, bw_mem = sizeof(terrain_tilemap);
  size_t tile_sz = sizeof(terrain_tile_type) + sizeof(terrain_tile_altitude);
  coord xs, zs;
  terrain_tilemap* world, * prev = NULL;
  char* base, * curr;
  /* Dummy head so that we can clear the next field correctly */
  SLIST_HEAD(, terrain_tilemap_s) dummy_head;

  /* Below, we allocate all worlds in one allocation. We don't manually handle
   * alignment, as xs*zs is always a large power of two (ie, a multiple of 8),
   * so any xs*zs*tile_size results in proper alignment.
   */

  /* Count required memory */
  for (xs = xmax, zs = zmax; xs >= xmin && zs >= zmin; xs /= 2, zs /= 2) {
    memory_reqd += bw_mem + xs*zs*tile_sz;
  }

  base = curr = xmalloc(memory_reqd);
  /* Initialise values */
  for (xs = xmax, zs = zmax; xs >= xmin && zs >= zmin; xs /= 2, zs /= 2) {
    world = (terrain_tilemap*)curr;
    world->type = (terrain_tile_type*)(curr + bw_mem);
    world->alt = (terrain_tile_altitude*)(world->type + xs*zs);
    curr += bw_mem + xs*zs*tile_sz;

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

  return (terrain_tilemap*)base;
}

void terrain_tilemap_delete(terrain_tilemap* this) {
  free(this);
}

static void terrain_tilemap_patch_shallow(
  terrain_tilemap* small, const terrain_tilemap* large, coord x, coord z
) {
  coord sx, sz;
  unsigned ox, oz, alt, strongest, loff, soff;

  /* Shift to base offset for reduced tile */
  x &= ~1u;
  z &= ~1u;

  sx = x/2;
  sz = z/2;
  soff = terrain_tilemap_offset(small, sx, sz);

  /* Select strongest element at each index for the combined value */
  strongest = 256;
  for (oz = 0; oz < 2; ++oz) {
    for (ox = 0; ox < 2; ++ox) {
      loff = terrain_tilemap_offset(large,
                                    (x+ox) & (large->xmax-1),
                                    (z+oz) & (large->zmax-1));
      if (large->type[loff] < strongest) {
        strongest = large->type[loff];
        small->type[soff] = large->type[loff];
        small->alt[soff] = large->alt[loff];
      }
    }
  }

  /* Terrain altitude is the maximum of the four.
   * It's less mathematically correct, but generally has better effects given
   * the way terrain is drawn.
   */
  alt = 0;
  for (oz = 0; oz < 2; ++oz) {
    for (ox = 0; ox < 2; ++ox) {
      loff = terrain_tilemap_offset(large,
                                    (x+ox) & (large->xmax-1),
                                    (z+oz) & (large->zmax-1));
      if (large->alt[loff] > alt)
        alt = large->alt[loff];
    }
  }
  small->alt[soff] = alt;
}

void terrain_tilemap_patch_next(terrain_tilemap* large, coord x, coord z) {
  terrain_tilemap* small;

  while ((small = SLIST_NEXT(large, next))) {
    terrain_tilemap_patch_shallow(small, large, x, z);

    /* Move to next level */
    large = small;
    x /= 2;
    z /= 2;
  }
}

void terrain_tilemap_calc_next(terrain_tilemap* large) {
  terrain_tilemap* small;
  coord x, z;

  while ((small = SLIST_NEXT(large, next))) {
    for (z = 0; z < large->zmax; z += 2)
      for (x = 0; x < large->xmax; x += 2)
        terrain_tilemap_patch_shallow(small, large, x, z);

    large = small;
  }
}

#define BMP_HEADER_SIZE 68
void terrain_tilemap_bmp_dump(FILE* out, const terrain_tilemap* this) {
  /* Save image as BMP
   * 0000+02  Signature = "BM"
   * 0002+04  File size, = 0x44+width*height*4
   * 0006+04  Zero
   * 000A+04  Offset, = 0x44
   * 000E+04  Header size, =0x28 (BITMAPINFOHEADER)
   * 0012+04  Width
   * 0016+04  Height
   * 001A+02  Planes = 1
   * 001C+02  BPP, = 0x20
   * 001E+04  Compression, = 3
   * 0022+04  Image size, = 4*width*height
   * 0026+04  Horiz resolution, = 0x1000
   * 002A+04  Vert resolution, = 0x1000
   * 002E+04  Palette size, = 0
   * 0032+04  "Important colours", = 0
   * 0036+04  Red mask, = 0x000000FF
   * 003A+04  Grn mask, = 0x0000FF00
   * 003E+04  Blu mask, = 0x00FF0000
   * 0042+02  Gap
   * 0044+end Image
   */
  unsigned num_px = this->xmax*this->zmax;
  unsigned image_size = 4 * num_px;
  unsigned file_size = BMP_HEADER_SIZE + image_size;
  unsigned char* buffer = xmalloc(file_size);
  unsigned char header[BMP_HEADER_SIZE] = {
    'B', 'M',
    (unsigned char)(file_size >>  0),
    (unsigned char)(file_size >>  8),
    (unsigned char)(file_size >> 16),
    (unsigned char)(file_size >> 24), /* File size */
    0, 0, 0, 0, /* Zero */
    0x44, 0, 0, 0, /* Offset */
    0x28, 0, 0, 0, /* Header size / BITMAPINFOHEADER */
    (unsigned char)(this->xmax >>  0),
    (unsigned char)(this->xmax >>  8),
    (unsigned char)(this->xmax >> 16),
    (unsigned char)(this->xmax >> 24), /* Width */
    (unsigned char)(this->zmax >>  0),
    (unsigned char)(this->zmax >>  8),
    (unsigned char)(this->zmax >> 16),
    (unsigned char)(this->zmax >> 24), /* height */
    0x01, 0x00, 0x20, 0x00, /* Planes, BPP */
    3, 0, 0, 0, /* Compression */
    (unsigned char)(image_size >>  0),
    (unsigned char)(image_size >>  8),
    (unsigned char)(image_size >> 16),
    (unsigned char)(image_size >> 24), /* Image size */
    0x00, 0x10, 0x00, 0x00, /* Horiz res */
    0x00, 0x10, 0x00, 0x00, /* Vert res */
    0, 0, 0, 0, /* Palette size */
    0, 0, 0, 0, /* Important colours */
    0xFF, 0x00, 0x00, 0x00, /* Red mask */
    0x00, 0xFF, 0x00, 0x00, /* Grn mask */
    0x00, 0x00, 0xFF, 0x00, /* Blu mask */
    0x00, 0x00, /* Gap */
  };
  unsigned i, alt, r, g, b;

  memcpy(buffer, header, sizeof(header));

  for (i = 0; i < num_px; ++i) {
    alt = this->alt[i] & 0x7F;

    switch (this->type[i] >> TERRAIN_SHADOW_BITS) {
    case terrain_type_snow:
      r = g = b = 0x80 + alt;
      break;

    case terrain_type_road:
      r = 0xFF;
      g = b = alt;
      break;

    case terrain_type_stone:
      r = g = b = alt;
      break;

    case terrain_type_grass:
      r = alt * 3 / 2;
      g = 0xA0;
      b = alt;
      break;

    case terrain_type_bare_grass:
      r = alt * 3 / 2;
      g = 0xA0;
      b = 0;
      break;

    case terrain_type_gravel:
      r = g = 0xC0;
      b = alt;
      break;

    case terrain_type_water:
      r = g = alt;
      b = 0xA0;
      break;

    default: abort();
    }

    buffer[BMP_HEADER_SIZE + i*4 + 0] = r;
    buffer[BMP_HEADER_SIZE + i*4 + 1] = g;
    buffer[BMP_HEADER_SIZE + i*4 + 2] = b;
    buffer[BMP_HEADER_SIZE + i*4 + 3] = 0xFF;
  }

  fwrite(buffer, file_size, 1, out);
  free(buffer);
}

terrain_tilemap* terrain_tilemap_deserialise(FILE* in) {
  struct {
    coord xmax, zmax, xmin, zmin;
  } size_info;
  terrain_tilemap* this;

  if (1 != fread(&size_info, sizeof(size_info), 1, in))
    err(EX_OSERR, "Reading terrain tilemap");

  this = terrain_tilemap_new(size_info.xmax, size_info.zmax,
                         size_info.xmin, size_info.zmin);
  if (1 != fread(this->type,
                 sizeof(terrain_tile_type)*this->xmax*this->zmax, 1, in))
    err(EX_OSERR, "Reading terrain tilemap");
  if (1 != fread(this->alt,
                 sizeof(terrain_tile_altitude)*this->xmax*this->zmax, 1, in))
    err(EX_OSERR, "Reading terrain tilemap");
  terrain_tilemap_calc_next(this);
  return this;
}

void terrain_tilemap_serialise(FILE* out, const terrain_tilemap* this) {
  const terrain_tilemap* world;
  struct {
    coord xmax, zmax, xmin, zmin;
  } size_info;

  size_info.xmax = this->xmax;
  size_info.zmax = this->zmax;
  for (world = this; world; world = SLIST_NEXT(world, next)) {
    size_info.xmin = world->xmax;
    size_info.zmin = world->zmax;
  }

  if (1 != fwrite(&size_info, sizeof(size_info), 1, out))
    goto error;

  if (1 != fwrite(this->type,
                  sizeof(terrain_tile_type)*this->xmax*this->zmax,
                  1, out))
    goto error;

  if (1 != fwrite(this->alt,
                  sizeof(terrain_tile_altitude)*this->xmax*this->zmax,
                  1, out))
    goto error;

  return;

  error:
  warnx("Serialising terrain tilemap");
}
