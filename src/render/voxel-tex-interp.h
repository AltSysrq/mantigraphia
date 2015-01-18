/*-
 * Copyright (c) 2015 Jason Lingle
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
#ifndef RENDER_VOXEL_TEX_INTERP_H_
#define RENDER_VOXEL_TEX_INTERP_H_

#include "../math/frac.h"

/**
 * Generates the "control" and "selector" format of a voxel texture, given the
 * raw resource representation of the texture.
 *
 * The length of control_rg is 2*src_len_px; the length of selector_r is
 * src_len_px; the length of src_rgb is 3*src_len_px.
 */
void voxel_tex_demux(unsigned char*restrict control_rg,
                     unsigned char*restrict selector_r,
                     const unsigned char*restrict src_rgb,
                     unsigned src_len_px);

/**
 * Writes to dst_rgba the voxel texture in "selector" format transformed
 * according to the given palette and the given time.
 *
 * The length of dst_rgba is 4*src_len_px; the length of src_r is src_len_px.
 * The length of palette_rgba is 4*palette_w_px*palette_h_px.
 */
void voxel_tex_apply_palette(unsigned char*restrict dst_rgba,
                             const unsigned char*restrict src_r,
                             unsigned src_len_px,
                             const unsigned char*restrict palette_rgba,
                             unsigned palette_w_px,
                             unsigned palette_h_px,
                             fraction t);

#endif /* RENDER_VOXEL_TEX_INTERP_H_ */
