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

#include <stdio.h>

#include "../bsd.h"

#include "../alloc.h"
#include "terrain-tilemap.h"
#include "propped-world.h"

propped_world* propped_world_new(terrain_tilemap* terrain,
                                 unsigned num_grass,
                                 unsigned num_trees) {
  propped_world* this = xmalloc(
    sizeof(propped_world) +
    num_grass*sizeof(world_prop) +
    num_trees*sizeof(world_prop));
  this->terrain = terrain;
  this->grass.size = num_grass;
  this->grass.props = (world_prop*)(this+1);
  this->trees[0].size = num_trees/2;
  this->trees[0].props = this->grass.props + num_grass;
  this->trees[1].size = num_trees/2;
  this->trees[1].props = this->trees[0].props + num_trees/2;
  return this;
}

void propped_world_delete(propped_world* this) {
  terrain_tilemap_delete(this->terrain);
  free(this);
}

propped_world* propped_world_deserialise(FILE* in) {
  struct {
    unsigned num_grass;
    unsigned num_trees;
  } size_info;
  propped_world* this;

  if (1 != fread(&size_info, sizeof(size_info), 1, in))
    err(EX_OSERR, "Reading propped world");

  this = propped_world_new(terrain_tilemap_deserialise(in),
                           size_info.num_grass, size_info.num_trees);
  if (1 != fread(this->grass.props, sizeof(world_prop)*this->grass.size, 1, in))
    err(EX_OSERR, "Reading propped world");
  if (1 != fread(this->trees[0].props, sizeof(world_prop)*this->trees[0].size, 1, in))
    err(EX_OSERR, "Reading propped world");
  if (1 != fread(this->trees[1].props, sizeof(world_prop)*this->trees[1].size, 1, in))
    err(EX_OSERR, "Reading propped world");

  return this;
}

void propped_world_serialise(FILE* out, const propped_world* this) {
  struct {
    unsigned num_grass;
    unsigned num_trees;
  } size_info = { this->grass.size, this->trees[0].size + this->trees[1].size };

  if (1 != fwrite(&size_info, sizeof(size_info), 1, out))
    goto error;
  terrain_tilemap_serialise(out, this->terrain);
  if (1 != fwrite(this->grass.props, sizeof(world_prop)*this->grass.size, 1, out))
    goto error;
  if (1 != fwrite(this->trees[0].props, sizeof(world_prop)*this->trees[0].size, 1, out))
    goto error;
  if (1 != fwrite(this->trees[1].props, sizeof(world_prop)*this->trees[1].size, 1, out))
    goto error;

  return;

  error:
  warn("Serialising propped world");
}
