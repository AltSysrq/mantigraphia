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
#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include <glew.h>
#include "../bsd.h"

#include "../math/matrix.h"
#include "shader_loader.h"
#include "shaders.h"

mat44fgl implicit_projection_matrix;

static GLuint current_program = 0;

/* Like offsetof(), but works off of the value of a pointer instead. */
#define ptroffof(ptr,fld) (((char*)&(ptr)->fld) - (char*)(ptr))

static inline void put_uniform_tex2d(GLint ix, GLuint tex) {
  glUniform1i(ix, tex);
}

static inline void put_uniform_float(GLint ix, float f) {
  glUniform1f(ix, f);
}

static inline void put_uniform_vec2(GLint ix, const float* f) {
  glUniform2fv(ix, 1, f);
}

static inline void put_uniform_vec3(GLint ix, const float* f) {
  glUniform3fv(ix, 1, f);
}

#define no_uniforms

#define shader(name) ;struct shader_##name##_info
#define composed_of(x,y) GLuint program; GLint projection_matrix_ix;
#define uniform(type,name) GLint name##_ix;
#define attrib(cnt,name) unsigned name##_va;
#define padding(cnt,name)
extern int dummy_decl
#include "shaders.inc"
;
#undef attrib
#undef uniform
#undef composed_of
#undef shader

static char link_error_log[65536];

#define shader(name)                                                    \
  static void shader_##name##_assemble(struct shader_##name##_info* info)
#define composed_of(fpart, vpart)                         \
  static GLuint shader_part_##fpart;                      \
  static GLuint shader_part_##vpart;                      \
  GLint status;                                           \
  const char*const composition __unused =                 \
    #fpart "+" #vpart;                                    \
  if (info->program) return;                              \
  info->program = glCreateProgram();                      \
  if (!info->program)                                     \
    errx(EX_OSERR, "Unable to create shader program: %d", \
         glGetError());                                   \
  load_shader(&shader_part_##fpart, #vpart);              \
  load_shader(&shader_part_##vpart, #fpart);              \
  glAttachShader(info->program, shader_part_##vpart);     \
  glAttachShader(info->program, shader_part_##fpart);     \
  glLinkProgram(info->program);                           \
  glGetProgramiv(info->program, GL_LINK_STATUS, &status); \
  if (!status) {                                          \
    glGetProgramInfoLog(info->program,                    \
                        sizeof(link_error_log),           \
                        NULL, link_error_log);            \
    errx(EX_DATAERR, "Failed to link shaders "            \
         "" #fpart "and" #vpart ":\n%s", link_error_log); \
  }                                                       \
  info->projection_matrix_ix = glGetUniformLocation(      \
    info->program, "projection_matrix");                  \
  if (-1 == info->projection_matrix_ix)                   \
    errx(EX_SOFTWARE, "Failed to link projection_matrix"  \
         " in shader, using %s", composition);

#define uniform(type,name)                                      \
  info->name##_ix = glGetUniformLocation(info->program, #name); \
  if (-1 == info->name##_ix)                                    \
    errx(EX_SOFTWARE, "Failed to link uniform " #name           \
         " in shader, using %s", composition);
#define attrib(cnt, name)                                       \
  status = glGetAttribLocation(info->program, #name);           \
  if (-1 == status)                                             \
    errx(EX_SOFTWARE, "Failed to link vertex attribute " #name  \
         " in shader, using %s", composition);                  \
  info->name##_va = status;
#include "shaders.inc"
#undef attrib
#undef uniform
#undef composed_of
#undef shader

#define shader(name)                            \
  static void shader_##name##_do_activate(      \
    struct shader_##name##_info* info,          \
    const shader_##name##_uniform* uniform)
#define composed_of(x,y)                                \
  if (current_program != info->program)                 \
    glUseProgram(info->program);                        \
  current_program = info->program;                      \
  glUniformMatrix4fv(                                   \
    info->projection_matrix_ix,                         \
    1, 0,                                               \
    (const float*)implicit_projection_matrix.m);

#define uniform(type, name)                             \
  put_uniform_##type(info->name##_ix, uniform->name);
#define attrib(cnt, name)
#include "shaders.inc"
#undef attrib
#undef uniform
#undef composed_of
#undef shader

#define shader(name)                                                    \
  static struct shader_##name##_info name##_shader_info;                \
  void shader_##name##_activate(                                        \
    const shader_##name##_uniform* uniform)                             \
  {                                                                     \
    shader_##name##_assemble(&name##_shader_info);                      \
    shader_##name##_do_activate(&name##_shader_info, uniform);          \
  } static inline void name##_dummy()
#define composed_of(x,y)
#define uniform(x,y)
#define attrib(cnt,name)
#include "shaders.inc"
#undef attrib
#undef uniform
#undef composed_of
#undef shader

#define shader(name)                            \
  static void shader_##name##_configure_vbo_(   \
    shader_##name##_vertex* vertex_format,      \
    struct shader_##name##_info* info);         \
  void shader_##name##_configure_vbo(void) {    \
    shader_##name##_configure_vbo_(             \
      0, &name##_shader_info);                  \
  }                                             \
  static void shader_##name##_configure_vbo_(   \
    shader_##name##_vertex* vertex_format,      \
    struct shader_##name##_info* info)
#define composed_of(x,y)
#define uniform(x,y)
#define attrib(cnt,name)                                                \
  glVertexAttribPointer(info->name##_va, cnt, GL_FLOAT, GL_FALSE,       \
                        sizeof(*vertex_format),                         \
                        (GLvoid*)ptroffof(vertex_format, name));        \
  glEnableVertexAttribArray(info->name##_va);
#include "shaders.inc"
#undef attrib
#undef uniform
#undef composed_of
#undef shader
