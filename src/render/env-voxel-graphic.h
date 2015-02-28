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
 * Specifies how a voxel can be rendered in a "blob-like" manner.
 *
 * Two graphic blobs are equivalent if they have the same ordinal.
 */
typedef struct {
  /**
   * The identifier for this graphic blob within its context.
   */
  unsigned char ordinal;

  /**
   * The colour palette to use. This is an RGBA texture indexed by a fixed
   * noise texture (S) and the current time (T).
   */
  unsigned palette;
  /**
   * The noise texture to use. This is a single-channel texture.
   */
  unsigned noise;

  /**
   * The bias added to the noise for this blob type, 16.16 fixed-point. 0.0
   * corresponds to S=0; 1.0 to S=1.
   */
  unsigned noise_bias;
  /**
   * The amplitude of the noise for this blob type, 16.16 fixed-point. 0.0
   * eliminates all noise; 1.0 allows traversing the entire palette.
   */
  unsigned noise_amplitude;
  /**
   * The frequency, relative to the width of the screen, at which the noise
   * texture repeats on the X axis, 16.16 fixed-point.
   */
  unsigned noise_xfreq;
  /**
   * The frequencey, relative to the width of the screen (width, so that the
   * result is square), at which the noise texture repeats on the Y axis, 16.16
   * fixed-point.
   */
  unsigned noise_yfreq;
  /**
   * Distance, in space units, by which faces should be perturbed when
   * subdividing the blobs.
   */
  unsigned perturbation;
} env_voxel_graphic_blob;

/**
 * Describes how a voxel (more specifically, a contextual voxel type) is
 * represented graphically.
 *
 * Two env_voxel_graphic values are considered equivalent only if they exist at
 * the same memory address.
 */
typedef struct {
  /**
   * Voxels may be rendered as "blobs" instead of intersecting planes, which
   * generally produces better results for dense organic objects.
   *
   * The presence of this field generally suppresses usage of the planes above.
   */
  const env_voxel_graphic_blob* blob;
} env_voxel_graphic;

#endif /* RENDER_ENV_VOXEL_GRAPHIC_H */
