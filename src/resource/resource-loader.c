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

#include "../alloc.h"
#include "../world/env-vmap.h"
#include "../render/env-voxel-graphic.h"

static unsigned res_num_voxel_types = 1;

env_voxel_graphic* res_voxel_graphics[NUM_ENV_VOXEL_TYPES];
static env_voxel_graphic res_voxel_graphics_array[NUM_ENV_VOXEL_TYPES];
static unsigned res_num_voxel_graphics;

static env_voxel_graphic_blob res_graphic_blobs[256];
static unsigned res_num_graphic_blobs;

#define MAX_PALETTES 256
#define MAX_VALTEXES 256

static GLuint res_palettes[MAX_PALETTES];
static unsigned res_num_palettes;
static GLuint res_valtexes[MAX_VALTEXES];
static unsigned res_num_valtexes;

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
  res_num_graphic_blobs = 1;
  res_num_palettes = 1;
  res_num_valtexes = 1;
  memset(res_voxel_graphics, 0, sizeof(res_voxel_graphics));
  memset(res_voxel_graphics_array, 0, sizeof(res_voxel_graphics_array));
  memset(res_graphic_blobs, 0, sizeof(res_graphic_blobs));

  if (!has_textures) {
    glGenTextures(MAX_PALETTES-1, res_palettes + 1);
    glGenTextures(MAX_VALTEXES-1, res_valtexes + 1);

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

unsigned rl_voxel_set_voxel_graphic(unsigned voxel, unsigned graphic) {
  CKNF();
  CKIX(voxel, res_num_voxel_types);
  CKIX(graphic, res_num_voxel_graphics);
  res_voxel_graphics[voxel] = res_voxel_graphics_array + graphic;
  return 1;
}

unsigned rl_voxel_graphic_new(void) {
  CKNF();
  CKIX(res_num_voxel_graphics, NUM_ENV_VOXEL_TYPES);
  return res_num_voxel_graphics++;
}

unsigned rl_voxel_graphic_set_blob(unsigned graphic, unsigned blob) {
  CKNF();
  CKIX(graphic, res_num_voxel_graphics);
  CKIX(blob, res_num_graphic_blobs);
  res_voxel_graphics_array[graphic].blob = res_graphic_blobs + blob;
  return 1;
}

unsigned rl_graphic_blob_new(void) {
  CKNF();
  CKIX(res_num_graphic_blobs, 256);

  res_graphic_blobs[res_num_graphic_blobs].ordinal = res_num_graphic_blobs-1;
  res_graphic_blobs[res_num_graphic_blobs].palette = res_default_texture;
  res_graphic_blobs[res_num_graphic_blobs].noise = res_default_texture;
  res_graphic_blobs[res_num_graphic_blobs].noise_bias = 0;
  res_graphic_blobs[res_num_graphic_blobs].noise_amplitude = 65536;
  res_graphic_blobs[res_num_graphic_blobs].noise_xfreq = 65536;
  res_graphic_blobs[res_num_graphic_blobs].noise_yfreq = 65536;
  res_graphic_blobs[res_num_graphic_blobs].perturbation = 0;
  return res_num_graphic_blobs++;
}

unsigned rl_graphic_blob_set_valtex(unsigned blob, unsigned valtex) {
  CKNF();
  CKIX(blob, res_num_graphic_blobs);
  CKIX(valtex, res_num_valtexes);

  res_graphic_blobs[blob].noise = res_valtexes[valtex];
  return 1;
}

unsigned rl_graphic_blob_set_palette(unsigned blob, unsigned palette) {
  CKNF();
  CKIX(blob, res_num_graphic_blobs);
  CKIX(palette, res_num_palettes);

  res_graphic_blobs[blob].palette = res_palettes[palette];
  return 1;
}

unsigned rl_graphic_blob_set_noise(unsigned blob, unsigned bias, unsigned amp,
                                   unsigned xfreq, unsigned yfreq) {
  CKNF();
  CKIX(blob, res_num_graphic_blobs);

  res_graphic_blobs[blob].noise_bias = bias;
  res_graphic_blobs[blob].noise_amplitude = amp;
  res_graphic_blobs[blob].noise_xfreq = xfreq;
  res_graphic_blobs[blob].noise_yfreq = yfreq;
  return 1;
}

unsigned rl_graphic_blob_set_perturbation(unsigned blob,
                                          unsigned perturbation) {
  CKNF();
  CKIX(blob, res_num_graphic_blobs);

  res_graphic_blobs[blob].perturbation = perturbation;
  return 1;
}

unsigned rl_palette_new(void) {
  CKNF();
  CKIX(res_num_palettes, MAX_PALETTES);

  res_gen_default_texture(res_palettes[res_num_palettes]);
  return res_num_palettes++;
}

unsigned rl_palette_loadMxNrgba(unsigned palette,
                                unsigned ncolours, unsigned ntimes,
                                const void* data) {
  CKNF();
  CKIX(palette, res_num_palettes);

  glBindTexture(GL_TEXTURE_2D, res_palettes[palette]);
  glPixelStorei(GL_UNPACK_ALIGNMENT, 1);
  glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, ncolours, ntimes, 0,
               GL_RGBA, GL_UNSIGNED_BYTE, data);
  return 1;
}

unsigned rl_valtex_new(void) {
  CKNF();
  CKIX(res_num_valtexes, MAX_VALTEXES);

  res_gen_default_texture(res_valtexes[res_num_valtexes]);
  return res_num_valtexes++;
}

unsigned rl_valtex_load64x64r(unsigned valtex, const void* data) {
  CKNF();
  CKIX(valtex, res_num_valtexes);

  glBindTexture(GL_TEXTURE_2D, res_valtexes[valtex]);
  glPixelStorei(GL_UNPACK_ALIGNMENT, 1);
  glTexImage2D(GL_TEXTURE_2D, 0, GL_RED, 64, 64, 0,
               GL_RED, GL_UNSIGNED_BYTE, data);
  return 1;
}
