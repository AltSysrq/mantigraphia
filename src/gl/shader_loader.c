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

#include <stdio.h>
#include "../bsd.h"

#include "shader_loader.h"

static const char glsl_version[] = "#version 130\n";

void load_shader(GLuint* shader, const char* basename) {
  static char buffer[65536];
  char filename[64];
  const char* source[2] = { glsl_version, buffer };
  FILE* file;
  int nread;
  GLint status;
  GLsizei logsz;

  if (*shader) /* already loaded */ return;

  *shader = glCreateShader(basename[0] == 'f'?
                           GL_FRAGMENT_SHADER : GL_VERTEX_SHADER);
  if (!*shader)
    errx(EX_OSERR, "Unable to allocate shader %s: %d",
         basename, glGetError());

  snprintf(filename, sizeof(filename)-1, "share/glsl/%s.glsl", basename);

  file = fopen(filename, "r");
  if (!file)
    err(EX_NOINPUT, "Unable to open shader %s at %s", basename, filename);

  nread = fread(buffer, 1, sizeof(buffer)-1, file);
  if (nread >= (signed)(sizeof(buffer)-1))
    errx(EX_DATAERR, "Shader %s is too big", filename);
  if (nread <= 0)
    err(EX_IOERR, "Failed to read from shader %s", filename);

  fclose(file);

  buffer[nread] = 0;

  glShaderSource(*shader, 2, source, NULL);
  glCompileShader(*shader);
  glGetShaderiv(*shader, GL_COMPILE_STATUS, &status);
  if (!status) {
    buffer[0] = 0;
    glGetShaderInfoLog(*shader, sizeof(buffer), &logsz, buffer);
    if (logsz > 0) {
      buffer[logsz] = 0;
      errx(EX_DATAERR, "Shader %s failed to compile.\n%s", filename, buffer);
    } else {
      errx(EX_DATAERR, "Shader %s failed to compile, but no error information"
           " is available", filename);
    }
  }
}
