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
#include "../render/env-vmap-voxel-renderer.h"
#include "../render/voxel-tex-interp.h"

env_voxel_context_map res_voxel_context_map;
static unsigned res_num_voxel_types = 1;

env_voxel_graphic* res_voxel_graphics[NUM_ENV_VOXEL_CONTEXTUAL_TYPES];
static env_voxel_graphic res_voxel_graphics_array[
  NUM_ENV_VOXEL_CONTEXTUAL_TYPES];
static unsigned res_num_voxel_graphics;

static env_voxel_graphic_plane res_graphic_planes[
  NUM_ENV_VOXEL_CONTEXTUAL_TYPES*3];
static unsigned res_num_graphic_planes;

static env_voxel_graphic_blob res_graphic_blobs[256];
static unsigned res_num_graphic_blobs;

#define MAX_TEXTURES 1024
#define MAX_PALETTES 256
#define TEXTURE_MM_NPX (64*64+32*32+16*16+8*8+4*4+2*2+1*1)

static struct {
  GLuint texture[2];
  unsigned char selector[TEXTURE_MM_NPX];
  unsigned char* palette;
  unsigned palette_w, palette_h;
} res_textures[MAX_TEXTURES];
static unsigned res_num_textures;

static GLuint res_palettes[MAX_PALETTES];
static unsigned res_num_palettes;

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
  unsigned i;

  res_num_voxel_types = 1;
  res_num_voxel_graphics = 1;
  res_num_graphic_planes = 1;
  res_num_graphic_blobs = 1;
  res_num_textures = 1;
  res_num_palettes = 1;
  memset(res_voxel_graphics, 0, sizeof(res_voxel_graphics));
  memset(&res_voxel_context_map, 0, sizeof(res_voxel_context_map));
  memset(res_voxel_graphics_array, 0, sizeof(res_voxel_graphics_array));
  memset(res_graphic_planes, 0, sizeof(res_graphic_planes));
  memset(res_graphic_blobs, 0, sizeof(res_graphic_blobs));

  if (!has_textures) {
    for (i = 1; i < MAX_TEXTURES; ++i)
      glGenTextures(2, res_textures[i].texture);

    glGenTextures(MAX_PALETTES-1, res_palettes + 1);

    has_textures = 1;
  }

  for (i = 1; i < MAX_TEXTURES; ++i)
    if (res_textures[i].palette)
      free(res_textures[i].palette);
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

unsigned rl_voxel_graphic_set_blob(unsigned graphic, unsigned blob) {
  CKNF();
  CKIX(graphic, res_num_voxel_graphics);
  CKIX(blob, res_num_graphic_blobs);
  res_voxel_graphics_array[graphic].blob = res_graphic_blobs + blob;
  return 1;
}

unsigned rl_graphic_plane_new(void) {
  CKNF();
  CKIX(res_num_graphic_planes, NUM_ENV_VOXEL_CONTEXTUAL_TYPES*3);

  res_graphic_planes[res_num_graphic_planes].texture = res_default_texture;
  res_graphic_planes[res_num_graphic_planes].control = res_default_texture;
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

  res_graphic_planes[plane].texture = res_textures[texture].texture[0];
  res_graphic_planes[plane].control = res_textures[texture].texture[1];
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

unsigned rl_graphic_blob_new(void) {
  CKNF();
  CKIX(res_num_graphic_blobs, 256);

  res_graphic_blobs[res_num_graphic_blobs].ordinal = res_num_graphic_blobs-1;
  res_graphic_blobs[res_num_graphic_blobs].palette = res_default_texture;
  res_graphic_blobs[res_num_graphic_blobs].noise_bias = 0;
  res_graphic_blobs[res_num_graphic_blobs].noise_amplitude = 65536;
  res_graphic_blobs[res_num_graphic_blobs].noise_xfreq = 65536;
  res_graphic_blobs[res_num_graphic_blobs].noise_yfreq = 65536;
  return res_num_graphic_blobs++;
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

unsigned rl_texture_new(void) {
  CKNF();
  CKIX(res_num_textures, MAX_TEXTURES);

  res_gen_default_texture(res_textures[res_num_textures].texture[0]);
  res_gen_default_texture(res_textures[res_num_textures].texture[1]);
  res_textures[res_num_textures].palette = NULL;
  return res_num_textures++;
}

unsigned rl_texture_load64x64rgbmm_NxMrgba(
  unsigned texture,
  const void* d64, const void* d32,
  const void* d16, const void* d8,
  const void* d4,  const void* d2,
  const void* d1,
  unsigned ncolours, unsigned ntimes,
  const void* palette
) {
  unsigned char rgb_data[TEXTURE_MM_NPX*3];
  unsigned char control_data[TEXTURE_MM_NPX*2];
  unsigned char* levelptr;
  unsigned level, ln;

  CKNF();
  CKIX(texture, res_num_textures);

  levelptr = rgb_data;
#define CPY(l) memcpy(levelptr, d##l, l*l*3); levelptr += l*l*3
  CPY(64);
  CPY(32);
  CPY(16);
  CPY(8);
  CPY(4);
  CPY(2);
  CPY(1);
#undef CPY

  if (res_textures[texture].palette)
    free(res_textures[texture].palette);

  res_textures[texture].palette = xmalloc(ncolours * ntimes * 4);
  memcpy(res_textures[texture].palette, palette, ncolours * ntimes * 4);
  res_textures[texture].palette_w = ncolours;
  res_textures[texture].palette_h = ntimes;

  voxel_tex_demux(control_data, res_textures[texture].selector, rgb_data,
                  TEXTURE_MM_NPX);

  glBindTexture(GL_TEXTURE_2D, res_textures[texture].texture[1]);
  glPixelStorei(GL_UNPACK_ALIGNMENT, 1);
  for (level = 64, ln = 0, levelptr = control_data;
       level;
       levelptr += level*level*2, level /= 2, ++ln)
    glTexImage2D(GL_TEXTURE_2D, ln, GL_RG, level, level, 0,
                 GL_RG, GL_UNSIGNED_BYTE, levelptr);

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

void rl_update_textures(unsigned offset, unsigned stride,
                        fraction t) {
  unsigned char rgba[TEXTURE_MM_NPX*4];
  unsigned char* levelptr;
  unsigned tex, level, ln;

  for (tex = offset + 1; tex < res_num_textures; tex += stride) {
    if (!res_textures[tex].palette) continue;

    voxel_tex_apply_palette(rgba, res_textures[tex].selector, TEXTURE_MM_NPX,
                            res_textures[tex].palette,
                            res_textures[tex].palette_w,
                            res_textures[tex].palette_h,
                            t);

    glBindTexture(GL_TEXTURE_2D, res_textures[tex].texture[0]);
    for (levelptr = rgba, level = 64, ln = 0;
         level;
         levelptr += level*level*4, level /= 2, ++ln)
      glTexImage2D(GL_TEXTURE_2D, ln, GL_RGBA, level, level, 0,
                   GL_RGBA, GL_UNSIGNED_BYTE, levelptr);
  }
}
