/*-
 * Copyright (c) 2014, 2015 Jason Lingle
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

#include <glew.h>

#include "../world/env-vmap.h"
#include "../render/env-vmap-renderer.h"

env_voxel_context_map res_voxel_context_map;
static unsigned res_num_voxel_types = 1;

env_voxel_graphic* res_voxel_graphics[NUM_ENV_VOXEL_CONTEXTUAL_TYPES];
static env_voxel_graphic res_voxel_graphics_array[
  NUM_ENV_VOXEL_CONTEXTUAL_TYPES];
static unsigned res_num_voxel_graphics;

static env_voxel_graphic_plane res_graphic_planes[
  NUM_ENV_VOXEL_CONTEXTUAL_TYPES*3];
static unsigned res_num_graphic_planes;

#define MAX_TEXTURES 1024
static GLuint res_textures[MAX_TEXTURES];
unsigned res_num_textures;

static GLuint res_palettes[MAX_TEXTURES];
unsigned res_num_palettes;

static GLuint res_default_texture;

static int res_is_frozen = 0;

static void res_gen_default_texture(GLuint tex) {
  unsigned char black[4] = { 0, 0, 0, 255 };

  glBindTexture(GL_TEXTURE_2D, tex);
  glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA,
               1, 1, 0, GL_RGBA, GL_UNSIGNED_BYTE, black);
}

void rl_clear(void) {
  static int has_textures = 0;

  res_num_voxel_types = 1;
  res_num_voxel_graphics = 1;
  res_num_graphic_planes = 1;
  res_num_textures = 1;
  res_num_palettes = 1;
  memset(res_voxel_graphics, 0, sizeof(res_voxel_graphics));
  memset(&res_voxel_context_map, 0, sizeof(res_voxel_context_map));
  memset(res_voxel_graphics_array, 0, sizeof(res_voxel_graphics_array));
  memset(res_graphic_planes, 0, sizeof(res_graphic_planes));

  if (!has_textures) {
    glGenTextures(1, &res_default_texture);
    glGenTextures(MAX_TEXTURES-1, res_textures + 1);
    glGenTextures(MAX_TEXTURES-1, res_palettes + 1);
    res_gen_default_texture(res_default_texture);
    has_textures = 1;
  }
}

void rl_set_frozen(int is_frozen) {
  res_is_frozen = is_frozen;
}

#define CKNF() do { if (res_is_frozen) return 0; } while (0)
#define CKIX(ix, max) do { if (!(ix) || (ix) >= (max)) return 0; } while (0)

unsigned rl_voxel_type_new(void) {
  CKNF();
  CKIX(res_num_voxel_types, NUM_ENV_VOXEL_TYPES);
  return res_num_voxel_types++;
}

unsigned rl_voxel_set_sensitivity(unsigned voxel, unsigned char sensitivity) {
  CKNF();
  CKIX(voxel, res_num_voxel_types);
  res_voxel_context_map.defs[voxel].sensitivity = sensitivity;
  return 1;
}

unsigned rl_voxel_set_in_family(unsigned voxel, unsigned other, int in_family) {
  CKNF();
  CKIX(voxel, res_num_voxel_types);
  CKIX(other, res_num_voxel_types);
  env_voxel_context_def_set_in_family(
    res_voxel_context_map.defs + voxel, other, in_family);
  return 1;
}

unsigned rl_voxel_set_voxel_graphic(unsigned cvoxel, unsigned graphic) {
  CKNF();
  CKIX(cvoxel, res_num_voxel_types << ENV_VOXEL_CONTEXT_BITS);
  CKIX(graphic, res_num_voxel_graphics);
  res_voxel_graphics[cvoxel] = res_voxel_graphics_array + graphic;
  return 1;
}

unsigned rl_voxel_graphic_new(void) {
  CKNF();
  CKIX(res_num_voxel_graphics, NUM_ENV_VOXEL_CONTEXTUAL_TYPES);
  return res_num_voxel_graphics++;
}

unsigned rl_voxel_graphic_set_plane(unsigned graphic, unsigned axis,
                                    unsigned plane) {
  CKNF();
  CKIX(graphic, res_num_voxel_graphics);
  if (axis >= 3) return 0;
  CKIX(plane, res_num_graphic_planes);
  res_voxel_graphics_array[graphic].planes[axis] =
    res_graphic_planes + plane;
  return 1;
}

unsigned rl_graphic_plane_new(void) {
  CKNF();
  CKIX(res_num_graphic_planes, NUM_ENV_VOXEL_CONTEXTUAL_TYPES*3);

  res_graphic_planes[res_num_graphic_planes].texture = res_default_texture;
  res_graphic_planes[res_num_graphic_planes].palette = res_default_texture;
  res_graphic_planes[res_num_graphic_planes].texture_scale[0][0] = 65536;
  res_graphic_planes[res_num_graphic_planes].texture_scale[0][1] = 0;
  res_graphic_planes[res_num_graphic_planes].texture_scale[1][0] = 0;
  res_graphic_planes[res_num_graphic_planes].texture_scale[1][1] = 65536;
  return res_num_graphic_planes++;
}

unsigned rl_graphic_plane_set_texture(unsigned plane, unsigned texture) {
  CKNF();
  CKIX(plane, res_num_graphic_planes);
  CKIX(texture, res_num_textures);

  res_graphic_planes[plane].texture = res_textures[texture];
  return 1;
}

unsigned rl_graphic_plane_set_palette(unsigned plane, unsigned palette) {
  CKNF();
  CKIX(plane, res_num_graphic_planes);
  CKIX(palette, res_num_palettes);

  res_graphic_planes[plane].palette = res_palettes[palette];
  return 1;
}

unsigned rl_graphic_plane_set_scale(unsigned plane, signed sx, signed sy,
                                    signed tx, signed ty) {
  CKNF();
  CKIX(plane, res_num_graphic_planes);

  res_graphic_planes[plane].texture_scale[0][0] = sx;
  res_graphic_planes[plane].texture_scale[0][1] = sy;
  res_graphic_planes[plane].texture_scale[1][0] = tx;
  res_graphic_planes[plane].texture_scale[1][1] = ty;
  return 1;
}

unsigned rl_texture_new(void) {
  CKNF();
  CKIX(res_num_textures, MAX_TEXTURES);

  res_gen_default_texture(res_textures[res_num_textures]);
  return res_num_textures++;
}

unsigned rl_texture_load64x64rgb(unsigned texture,
                                 const void* d64,
                                 const void* d32,
                                 const void* d16,
                                 const void* d8,
                                 const void* d4,
                                 const void* d2,
                                 const void* d1) {
  CKNF();
  CKIX(texture, res_num_textures);

  glBindTexture(GL_TEXTURE_2D, res_textures[texture]);
  glPixelStorei(GL_UNPACK_ALIGNMENT, 1);
#define L(l,s)                                          \
  glTexImage2D(GL_TEXTURE_2D, l, GL_RGB, s, s, 0,       \
               GL_RGB, GL_UNSIGNED_BYTE, d##s)
  L(0, 64);
  L(1, 32);
  L(2, 16);
  L(3,  8);
  L(4,  4);
  L(5,  2);
  L(6,  1);
#undef L

  return 1;
}

unsigned rl_palette_new(void) {
  CKNF();
  CKIX(res_num_palettes, MAX_TEXTURES);

  res_gen_default_texture(res_palettes[res_num_palettes]);
  return res_num_palettes++;
}

unsigned rl_palette_loadNxMrgba(unsigned palette,
                                unsigned ncolours, unsigned ntimes,
                                const void* data) {
  CKNF();
  CKIX(palette, res_num_palettes);
  if (!ncolours || ncolours > 256) return 0;
  if (ntimes < 10 || ntimes > 256) return 0;

  glBindTexture(GL_TEXTURE_2D, res_palettes[palette]);
  glPixelStorei(GL_UNPACK_ALIGNMENT, 1);
  glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, ncolours, ntimes, 0,
               GL_RGBA, GL_UNSIGNED_BYTE, data);
  return 1;
}
