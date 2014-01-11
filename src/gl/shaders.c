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
#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include <glew.h>
#include "../bsd.h"

#include "shader_loader.h"
#include "shaders.h"

static inline void put_uniform_tex2d(GLint ix, GLuint tex) {
  glUniform1i(ix, tex);
}

#define shader_source(name) ;static GLuint shader_part_##name
#define shader(name) ;struct shader_##name##_info
#define composed_of(x,y) GLuint program;
#define uniform(type,name) GLint name##_ix;
extern int dummy_decl
#include "shaders.inc"
;
#undef uniform
#undef composed_of
#undef shader
#undef shader_source

#define shader_source(name)

static char link_error_log[65536];

#define shader(name)                                                    \
  static void shader_##name##_assemble(struct shader_##name##_info* info)
#define composed_of(fpart, vpart)                         \
  GLint status;                                           \
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
  }
#define uniform(type,name)                                      \
  info->name##_ix = glGetUniformLocation(info->program, #name); \
  if (-1 == info->name##_ix)                                    \
    errx(EX_SOFTWARE, "Failed to link uniform " #type           \
         " in shader");
#include "shaders.inc"
#undef uniform
#undef composed_of
#undef shader

#define shader(name)                            \
  static void shader_##name##_do_activate(      \
    struct shader_##name##_info* info,          \
    const shader_##name##_uniform* uniform)
#define composed_of(x,y) glUseProgram(info->program);
#define uniform(type, name)                             \
  put_uniform_##type(info->name##_ix, uniform->name);
#include "shaders.inc"
#undef uniform
#undef composed_of
#undef shader

#define shader(name)                                                    \
  void shader_##name##_activate(                                        \
    const shader_##name##_uniform* uniform)                             \
  {                                                                     \
    static struct shader_##name##_info info;                            \
    shader_##name##_assemble(&info);                                    \
    shader_##name##_do_activate(&info, uniform);                        \
  } static inline void name_##dummy()
#define composed_of(x,y)
#define uniform(x,y)
#include "shaders.inc"
#undef uniform
#undef composed_of
#undef shader
