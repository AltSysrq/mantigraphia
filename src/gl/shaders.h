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
#ifndef GL_SHADERS_H_
#define GL_SHADERS_H_

#include <SDL_opengl.h>

typedef GLuint shader_type_tex2d;
typedef float shader_type_float;

#define shader_source(x) ;extern int dummy_decl
#define shader(name)                                                    \
  ;typedef struct shader_##name##_uniform_s                             \
  shader_##name##_uniform;                                              \
  void shader_##name##_activate(const shader_##name##_uniform*);        \
  struct shader_##name##_uniform_s
#define composed_of(x,y)
/* Fixed-function shaders never have uniforms; add a member to suppress
 * empty-struct warnings.
 */
#define fixed_function int dummy;
#define uniform(type, name) shader_type_##type name;
#define with_texture_coordinates
#define with_colour
#define with_secondary_colour
#define attrib(cnt,name)
extern int dummy_decl
#include "shaders.inc"
;
#undef attrib
#undef with_secondary_colour
#undef with_colour
#undef with_texture_coordinates
#undef uniform
#undef fixed_function
#undef composed_of
#undef shader

#define shader(name)                            \
  ;typedef struct shader_##name##_vertex_s      \
  shader_##name##_vertex;                       \
  void shader_##name##_configure_vbo(void);     \
  struct shader_##name##_vertex_s
#define fixed_function float v[3];
#define composed_of(x,y) fixed_function
#define uniform(x,y)
#define with_texture_coordinates float tc[2];
#define with_colour short colour[4];
#define with_secondary_colour short sec_colour[4];
#define attrib(cnt,name) float name[cnt];
extern int dummy_decl
#include "shaders.inc"
;
#undef attrib
#undef with_secondary_colour
#undef with_colour
#undef with_texture_coordinates
#undef uniform
#undef fixed_function
#undef composed_of
#undef shader

#undef shader_source

#endif /* GL_SHADERS_H_ */
