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

shader_source(fpaint_overlay)
shader_source(fpostprocess)
shader_source(fskybox)
shader_source(fterrabuff)
shader_source(fvoxel)
shader_source(vgenerictc)
shader_source(vpaint_overlay)
shader_source(vterrabuff)
shader_source(vskybox)
shader_source(vvoxel)

shader(fixed_texture) {
  fixed_function
  with_texture_coordinates
}

shader(solid) {
  fixed_function
  with_colour
}

shader(terrabuff) {
  composed_of(fterrabuff, vterrabuff)
  with_texture_coordinates
  with_colour
  with_secondary_colour
  uniform(tex2d, hmap)
  uniform(tex2d, tex)
  uniform(float, ty_below)
  uniform(float, line_thickness)
  uniform(float, xoff)
  uniform(vec2, screen_size)
  attrib(1, side)
}

shader(postprocess) {
  composed_of(fpostprocess, vgenerictc)
  with_texture_coordinates
  uniform(tex2d, framebuffer)
  uniform(float, pocket_size_px)
  uniform(vec2, pocket_size_scr)
  uniform(vec2, px_offset)
}

shader(paint_overlay) {
  composed_of(fpaint_overlay, vpaint_overlay)
  uniform(tex2d, framebuffer)
  uniform(tex2d, brush)
  uniform(vec2, screen_size)
  uniform(vec2, screen_off)
}

shader(voxel) {
  composed_of(fvoxel, vvoxel)
  with_texture_coordinates
  uniform(vec2, torus_sz)
  uniform(vec2, yrot)
  uniform(vec2, rxrot)
  uniform(float, zscale)
  uniform(vec2, soff)
  uniform(vec3, camera_integer)
  uniform(vec3, camera_fractional)
  uniform(tex2d, tex)
  uniform(tex2d, palette)
  uniform(float, palette_t)
  uniform(vec2, texture_scale_s)
  uniform(vec2, texture_scale_t)
}

shader(skybox) {
  composed_of(fskybox, vskybox)
  uniform(vec2, screen_size)
  uniform(float, fov)
  uniform(vec2, yrot)
  uniform(vec2, rxrot)
  uniform(tex2d, clouds)
  uniform(vec2, cloud_offset_1)
  uniform(vec2, cloud_offset_2)
  uniform(float, cloudiness)
}
