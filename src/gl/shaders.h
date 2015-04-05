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
#ifndef GL_SHADERS_H_
#define GL_SHADERS_H_

#include <SDL_opengl.h>

#include "../math/matrix.h"

extern mat44fgl implicit_projection_matrix;

typedef GLuint shader_type_tex2d;
typedef float shader_type_float;
typedef float shader_type_vec2[2];
typedef float shader_type_vec3[3];

#define shader(name)                                                    \
  ;typedef struct shader_##name##_uniform_s                             \
  shader_##name##_uniform;                                              \
  void shader_##name##_activate(const shader_##name##_uniform*);        \
  struct shader_##name##_uniform_s
#define composed_of(x,y)
#define uniform(type, name) shader_type_##type name;
#define no_uniforms int dummy;
#define attrib(cnt,name)
#define padding(cnt,name)
extern int dummy_decl
#include "shaders.inc"
;
#undef padding
#undef attrib
#undef no_uniforms
#undef uniform
#undef composed_of
#undef shader

#define shader(name)                            \
  ;typedef struct shader_##name##_vertex_s      \
  shader_##name##_vertex;                       \
  void shader_##name##_configure_vbo(void);     \
  struct shader_##name##_vertex_s
#define composed_of(x,y) float v[3];
#define uniform(x,y)
#define no_uniforms
#define attrib(cnt,name) float name[cnt];
#define padding(cnt,name) float name[cnt];
extern int dummy_decl
#include "shaders.inc"
;
#undef padding
#undef attrib
#undef no_uniforms
#undef uniform
#undef composed_of
#undef shader

#endif /* GL_SHADERS_H_ */
