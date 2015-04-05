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

#include <stdio.h>

#include <glew.h>

#include "../bsd.h"
#include "shaders.h"
#include "glinfo.h"

unsigned max_point_size;
int can_draw_offscreen_points;

void glinfo_detect(unsigned wh) {
  shader_solid_vertex vertex;
  GLuint vao, vbo;
  float point_size[2];
  unsigned pixel;
  int max_vertex_texture_image_units;

  glGetFloatv(GL_POINT_SIZE_RANGE, point_size);
  max_point_size = point_size[1 /* maximum */];

  /* I haven't found any way to directly determine whether point sprites can be
   * drawn with off-screen centres. Just try it and see what happens.
   */
  glPushAttrib(GL_ALL_ATTRIB_BITS);
  glClearColor(0.0f,0.0f,0.0f,1.0f);
  glClear(GL_COLOR_BUFFER_BIT);

  glGenVertexArrays(1, &vao);
  glGenBuffers(1, &vbo);
  glBindVertexArray(vao);
  glBindBuffer(GL_ARRAY_BUFFER, vbo);
  vertex.v[0] = -1.0f;
  vertex.v[1] = -1.0f;
  vertex.v[2] = 0.0f;
  vertex.colour[0] = 1.0f;
  vertex.colour[1] = 1.0f;
  vertex.colour[2] = 1.0f;
  vertex.colour[3] = 1.0f;
  glBufferData(GL_ARRAY_BUFFER, sizeof(vertex), &vertex, GL_STREAM_DRAW);
  shader_solid_activate(NULL);
  shader_solid_configure_vbo();
  glPointSize(max_point_size);
  glDrawArrays(GL_POINTS, 0, 1);
  glDeleteBuffers(1, &vbo);
  glDeleteVertexArrays(1, &vao);

  glReadPixels(0, wh-1, 1, 1, GL_RGB, GL_UNSIGNED_INT, &pixel);
  can_draw_offscreen_points = !!pixel;

  glPopAttrib();

  max_vertex_texture_image_units = -1;
  glGetIntegerv(GL_MAX_VERTEX_TEXTURE_IMAGE_UNITS, &max_vertex_texture_image_units);

  printf("GL info: max point size = %d; off screen point support = %s\n",
         max_point_size, can_draw_offscreen_points? "yes" : "no");
  printf("Max vertex texture image units: %d\n", max_vertex_texture_image_units);
  if (max_vertex_texture_image_units < 1)
    errx(EX_OSERR,
         "Your graphics card's OpenGL implementation does not support "
         "use of textures in vertex shaders.");
}
