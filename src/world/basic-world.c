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
#include "basic-world.h"
#include "terrain.h"

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

    /* Terrain altitude is the maximum of the four.
     * It's less mathematically correct, but generally has better effects given
     * the way terrain is drawn.
     */
    alt = 0;
    for (oz = 0; oz < 2; ++oz) {
      for (ox = 0; ox < 2; ++ox) {
        loff = basic_world_offset(large,
                                  (x+ox) & (large->xmax-1),
                                  (z+oz) & (large->zmax-1));
        if (large->tiles[loff].elts[0].altitude > alt)
          alt = large->tiles[loff].elts[0].altitude;
      }
    }
    small->tiles[soff].elts[0].altitude = alt;

    /* Move to next level */
    large = small;
    x = sx &~ 1;
    z = sz &~ 1;
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

#define BMP_HEADER_SIZE 68
void basic_world_bmp_dump(FILE* out, const basic_world* this) {
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
    alt = this->tiles[i].elts[0].altitude & 0x7F;

    switch (this->tiles[i].elts[0].type >> TERRAIN_SHADOW_BITS) {
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

basic_world* basic_world_deserialise(FILE* in) {
  struct {
    coord xmax, zmax, xmin, zmin;
  } size_info;
  basic_world* this;

  if (1 != fread(&size_info, sizeof(size_info), 1, in))
    err(EX_OSERR, "Reading basic world");

  this = basic_world_new(size_info.xmax, size_info.zmax,
                         size_info.xmin, size_info.zmin);
  if (1 != fread(this->tiles, sizeof(tile_info)*this->xmax*this->zmax, 1, in))
    err(EX_OSERR, "Reading basic world");
  basic_world_calc_next(this);
  return this;
}

void basic_world_serialise(FILE* out, const basic_world* this) {
  const basic_world* world;
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

  if (1 != fwrite(this->tiles, sizeof(tile_info)*this->xmax*this->zmax,
                  1, out))
    goto error;

  return;

  error:
  warnx("Serialising basic world");
}
