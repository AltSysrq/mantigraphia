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
#include "../render/env-voxel-graphic.h"

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
 * Sets the graphic blob associated with the given voxel graphic.
 *
 * @param graphic The voxel graphic to modify (see rl_voxel_graphic_new()).
 * @param blob The blob to assign (see rl_graphic_blob_new()).
 * @return Whether successful.
 */
unsigned rl_voxel_graphic_set_blob(unsigned graphic, unsigned blob);
/**
 * Allocates a new graphic blob with an unspecified palette.
 *
 * The initial noise parameters are bias=0.0, amp=1.0, xfreq=1.0, yfreq=1.0.
 *
 * @return The new graphic blob index.
 */
unsigned rl_graphic_blob_new(void);
/**
 * Sets the palette used by the given graphic blob.
 *
 * @param blob The graphic blob to edit (see rl_graphic_blob_new()).
 * @param palette The palette to associate with the blob (see
 * rl_palette_new()).
 * @return Whether successful.
 */
unsigned rl_graphic_blob_set_palette(unsigned blob, unsigned palette);
/**
 * Configures the noise parameters for the given graphic blob.
 *
 * @param blob The graphic blob to edit (see rl_graphic_blob_new()).
 * @param bias The 16.16 fixed-point noise bias.
 * @param amp The 16.16 fixed-point noise amplitude.
 * @param xfreq The 16.16 fixed-point noise frequency on the X axis, relative
 * to screen width.
 * @param yfreq The 16.16 fixed-point noise frequency on the Y axis, relative
 * to screen width.
 * @return Whether successful.
 */
unsigned rl_graphic_blob_set_noise(unsigned blob, unsigned bias, unsigned amp,
                                   unsigned xfreq, unsigned yfreq);
/**
 * Configures the perturbation factor for the given graphic blob.
 *
 * @param blob The graphic blob to edit (see rl_graphic_blob_new()).
 * @param perturbation The perturbation, in space units, by which blob faces
 * are perturbed during subdivision. The default is zero.
 * @return Whether successful.
 */
unsigned rl_graphic_blob_set_perturbation(unsigned blob, unsigned perturbation);
/**
 * Allocates a new palette texture with unspecified content.
 *
 * These palettes are unrelated to those related to
 * rl_texture_load64x64rgbmm_NxMrgba().
 *
 * @return The new palette texture index.
 */
unsigned rl_palette_new(void);
/**
 * Edits the RGBA texture data of the given palette.
 *
 * @param palette The palette to edit.
 * @param ncolours The number of colours in the palette; ie, the size of the
 * palette's S axis.
 * @param ntimes The number of time-steps in the palette; ie, the size of the
 * palette's T axis.
 * @param data The RGBA data, size ncolours*ntimes*4.
 * @return Whether successful.
 */
unsigned rl_palette_loadMxNrgba(unsigned palette,
                                unsigned ncolours, unsigned ntimes,
                                const void* data);

#endif /* RESOURCE_RESOURCE_LOADER_H_ */
