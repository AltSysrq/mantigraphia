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
 * The swizzled representation of the voxel graphic table specified by calls to
 * the resource loader.
 */
extern env_voxel_graphic* res_voxel_graphics[NUM_ENV_VOXEL_TYPES];

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
 * Allocates a new voxel type with no graphics.
 *
 * This is the scarcest resource exposed: A maximum of 255 can be allocated.
 *
 * @return The new voxel type.
 */
unsigned rl_voxel_type_new(void);
/**
 * Sets the graphic associated with the given contextual voxel type.
 *
 * @param voxel The voxel type to alter.
 * @param graphic The graphic to assign to this contextual voxel type (see
 * rl_voxel_graphic_new()).
 * @return Whether successful.
 */
unsigned rl_voxel_set_voxel_graphic(unsigned voxel, unsigned graphic);
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
 * Sets the noise texture used by the given graphic blob.
 *
 * @param blob The graphic blob to edit (see rl_graphic_blob_new()).
 * @param noise The value texture to associate with the given blob (see
 * rl_valtex_new()).
 * @return Whether successful.
 */
unsigned rl_graphic_blob_set_valtex(unsigned blob, unsigned valtex);
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
/**
 * Allocates a new value texture (valtex) with unspecified content.
 *
 * A valtex is a 64x64 texture with only an R channel. It is typically used to
 * index palettes.
 *
 * @return The new valtex index.
 */
unsigned rl_valtex_new(void);
/**
 * Edits the R texture data of the given valtex.
 *
 * @param valtex The valtex to edit.
 * @param data The R data, size 64*64
 * @return Whether successful.
 */
unsigned rl_valtex_load64x64r(unsigned valtex, const void* data);

#endif /* RESOURCE_RESOURCE_LOADER_H_ */
