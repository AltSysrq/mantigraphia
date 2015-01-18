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
#ifndef RESOURCE_RESOURCE_LOADER_H_
#define RESOURCE_RESOURCE_LOADER_H_

#include "../math/frac.h"
#include "../world/env-vmap.h"
#include "../render/env-vmap-renderer.h"

/**
 * @file
 *
 * Provides a simple state-machine-based mechanism for Llua scripts to load
 * resources.
 *
 * The set of resources is a global value. This is so that Llua need not pass
 * some kind of context around; it doesn't really limit flexibility since there
 * won't ever be more than one set of resources in a process anyway.
 *
 * All Llua-callable functions return 0 on failure and non-zero on success.
 * Those that allocate resources return an index to that resource.
 */

/**
 * The voxel context map that has been loaded so far.
 */
extern env_voxel_context_map res_voxel_context_map;
/**
 * The swizzled representation of the voxel graphic table specified by calls to
 * the resource loader.
 */
extern env_voxel_graphic* res_voxel_graphics[NUM_ENV_VOXEL_CONTEXTUAL_TYPES];

/**
 * Resets the resource loader to its initial state, releasing all resources
 * held by the current resource set.
 */
void rl_clear(void);
/**
 * Sets whether the resource set is frozen. If frozen, all calls to mutate the
 * resource set fail.
 */
void rl_set_frozen(int);

/**
 * Updates every strideth texture, beginning with the offsetth texture,
 * according to their palettes and the current time.
 */
void rl_update_textures(unsigned offset, unsigned stride, fraction t);


/****** Functions below this point are llua-callable *******/

/**
 * Allocates a new (non-contextual) voxel type with an empty family, no
 * sensitivity, and no graphics.
 *
 * This is the scarcest resource exposed: A maximum of 255 can be allocated.
 *
 * @return The new voxel type.
 */
unsigned rl_voxel_type_new(void);
/**
 * Sets the sensitivity of the given voxel.
 *
 * @param voxel The voxel type (from rl_voxel_type_new)
 * @param sensitivity The sensitivity (the OR / sum of any number of the
 * ENV_VOXEL_CONTEXT_?? constants).
 *
 * @return Whether successful.
 */
unsigned rl_voxel_set_sensitivity(unsigned voxel, unsigned char sensitivity);
/**
 * Sets whether one voxel type is considered to be in the family of the other.
 *
 * @param voxel The voxel whose family is to be altered.
 * @param other The voxel which is to be added to or removed from the family.
 * @param in_family Whether other will be considered to be a member of voxel's
 * context family after the call.
 */
unsigned rl_voxel_set_in_family(unsigned voxel, unsigned other, int in_family);
/**
 * Sets the graphic associated with the given contextual voxel type.
 *
 * @param cvoxel The *contextual* voxel type (ie, a value returned from
 * rl_voxel_type_new(), multiplied by 2**ENV_VOXEL_CONTEXT_BITS, plus some
 * combination of ENV_VOXEL_CONTEXT_?? constants) to alter.
 * @param graphic The graphic to assign to this contextual voxel type (see
 * rl_voxel_graphic_new()).
 * @return Whether successful.
 */
unsigned rl_voxel_set_voxel_graphic(unsigned cvoxel, unsigned graphic);
/**
 * Allocates a new voxel graphic specification.
 *
 * @return The new voxel graphic index.
 */
unsigned rl_voxel_graphic_new(void);
/**
 * Sets the graphic plane associated with the given axis of the given voxel
 * graphic.
 *
 * @param graphic The voxel graphic to modify (see rl_voxel_graphic_new()).
 * @param axis The axis of the plane to edit (0=X, 1=Y, 2=Z)
 * @param plane The plane to assign (see rl_graphic_plane_new()).
 * @return Whether successful.
 */
unsigned rl_voxel_graphic_set_plane(unsigned graphic, unsigned axis,
                                    unsigned plane);
/**
 * Allocates a new graphic plane.
 *
 * @return The index of the new graphic plane.
 */
unsigned rl_graphic_plane_new(void);
/**
 * Sets the texture used with the given graphic plane.
 *
 * @param plane The plane to edit.
 * @param texture The texture to use with the plane (see rl_texture_new()).
 * @return Whether successful.
 */
unsigned rl_graphic_plane_set_texture(unsigned plane, unsigned texture);
/**
 * Sets the palette used with the given graphic plane.
 *
 * @param plane The plane to edit.
 * @param palette The palette to use with the plane (see rl_palette_new()).
 * @return Whether successful.
 */
unsigned rl_graphic_plane_set_palette(unsigned plane, unsigned palette);
/**
 * Sets the scale applied to the texture of the given graphic plane.
 *
 * @param plane The plane to edit.
 * @param sxscale The scale of the S axis (in 16.16 fixed-point format)
 * relative to the effective X axis.
 * @param syscale The scale of the S axis (in 16.16 fixed-point format)
 * relative to the effective Y axis.
 * @param txscale The scale of the T axis (in 16.16 fixed-point format)
 * relative to the effective X axis.
 * @param tyscale The scale of the T axis (in 16.16 fixed-point format)
 * relative to the effective Y axis.
 * @return Whether successful.
 */
unsigned rl_graphic_plane_set_scale(unsigned plane,
                                    signed sxscale, signed syscale,
                                    signed txscale, signed tyscale);
/**
 * Allocates a new texture with unspecified content.
 *
 * @return The new texture index.
 */
unsigned rl_texture_new(void);
/**
 * Edits 64x64 RGB texture by specifying both the control and palette data.
 *
 * The mipmap of the texture specifies control data as follows:
 * - R is an index into the S axis of the palette (after logical expansion).
 * - G controls the brush shape and direction (it is passed through to the
 *   alpha channel of the outpt).
 * - B controls visibility. A texel is visible if the B of the control texture
 *   is greater than the A of the palette.
 *
 * The palette specifies the actual colours to use. It is logically expanded
 * via interpolation to be 256 pixels wide. The S axis is used to select
 * colours via the R channel of the mipmap texture; the T axis specifies
 * variance by time.
 *
 * @param texture The texture to edit.
 * @param d64 The 64x64 level of the mipmap. Size 64*64*3 = 12288.
 * @param d32 The 32x32 level of the mipmap. Size 32*32*3 = 3072.
 * @param d16 The 16x16 level of the mipmap. Size 16*16*3 = 768.
 * @param d8 The 8x8 level of the mipmap. Size 8*8*3 = 192.
 * @param d4 The 4x4 level of the mipmap. Size 4*4*3 = 48.
 * @param d2 The 2x2 level of the mipmap. Size 2*2*3 = 12.
 * @param d1 The 1x1 level of the mipmap. Size 1*1*3 = 3.
 * @param ncolours The number of colours in the palette; ie, the size of the
 * palette's S axis.
 * @param ntimes The number of time-steps in the palette; ie, the size of the
 * palette's T axis.
 * @return Whether successful.
 */
unsigned rl_texture_load64x64rgbmm_NxMrgba(
  unsigned texture,
  const void* d64, const void* d32,
  const void* d16, const void* d8,
  const void* d4, const void* d2,
  const void* d1,
  unsigned ncolours, unsigned ntimes,
  const void* palette);

#endif /* RESOURCE_RESOURCE_LOADER_H_ */
