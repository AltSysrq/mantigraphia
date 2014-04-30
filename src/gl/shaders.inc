/*-
 * Copyright (c) 2014 Jason Lingle
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

shader_source(fbrush)
shader_source(fpencil)
shader_source(fpointcircle)
shader_source(fpostprocess)
shader_source(fsplotch)
shader_source(fterrabuff)
shader_source(vbrush)
shader_source(vgenerictc)
shader_source(vgenericctc)
shader_source(vpencil)
shader_source(vsplotch)
shader_source(vterrabuff)

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

shader(brush) {
  composed_of(fbrush, vbrush)
  with_texture_coordinates
  uniform(tex2d, tex)
  uniform(tex2d, pallet)
  uniform(float, decay)
  uniform(float, noise)
  attrib(2, info)
}

shader(splotch) {
  composed_of(fsplotch, vsplotch)
  attrib(3, parms)
  uniform(tex2d, tex)
  uniform(tex2d, pallet)
  uniform(float, noise)
}

shader(pointcircle) {
  composed_of(fpointcircle, vgenericctc)
  with_colour
  no_uniforms
}

shader(pencil) {
  composed_of(fpencil, vpencil)
  with_colour
  attrib(1, tcoord)
  uniform(tex2d, thickness_tex)
  uniform(float, line_thickness)
  uniform(float, viewport_height)
}

shader(postprocess) {
  composed_of(fpostprocess, vgenerictc)
  with_texture_coordinates
  uniform(tex2d, framebuffer)
  uniform(float, pocket_size_px)
  uniform(vec2, pocket_size_scr)
  uniform(vec2, px_offset)
}
