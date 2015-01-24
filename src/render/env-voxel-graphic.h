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
#ifndef RENDER_ENV_VOXEL_GRAPHIC_H
#define RENDER_ENV_VOXEL_GRAPHIC_H

/**
 * Describes the parameters used to draw a graphic plane of a voxel.
 *
 * Graphic planes are considered equivalent only if they exist at the same
 * memory address.
 */
typedef struct {
  /**
   * The primary texture to use for rendering.
   *
   * This is an RGBA texture. The RGB components represent true colour; they
   * are passed through to the final colour verbatim. The A component controls
   * fragment visibility; a fragment is visible if the A component of this
   * texture is greater than the G component of the control texture at the same
   * location.
   */
  unsigned texture;

  /**
   * The "control" texture to use for rendering.
   *
   * This is an RG texture. The R component is passed through to the alpha
   * value of the fragment (controlling brush shape and direction). The G
   * component controls visibility according to the A value of the primary
   * texture.
   */
  unsigned control;

  /**
   * Factor by which texture coordinates for this plane shall be multiplied,
   * allowing arbitrary affine transforms.
   *
   * tex.t = x * texture_scale[0][0] + y * texture_scale[0][1]
   * tex.t = y * texture_scale[1][0] + x * texture_scale[1][1]
   *
   * This is 16.16 fixed-point, so 1.0 == 65536.
   */
  signed texture_scale[2][2];
} env_voxel_graphic_plane;

/**
 * Describes how a voxel (more specifically, a contextual voxel type) is
 * represented graphically.
 *
 * Two env_voxel_graphic values are considered equivalent only if they exist at
 * the same memory address.
 */
typedef struct {
  /**
   * In the general case, voxels are rendered as three axis-aligned planes
   * centred on the centre of the voxel. These indicate the parameters for the
   * X, Y, and Z planes, respectively.
   *
   * These are pointers so that plane equivalence can be efficiently detected.
   *
   * NULL values indicate planes that should not be drawn.
   */
  const env_voxel_graphic_plane* planes[3];
} env_voxel_graphic;

#endif /* RENDER_ENV_VOXEL_GRAPHIC_H */
