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
#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include "matrix.h"

/* So we can define column-major matrices in row-major order. Otherwise the
 * result is unreadable.
 */
#define CMAJ(f00, f01, f02, f03,                         \
             f10, f11, f12, f13,                         \
             f20, f21, f22, f23,                         \
             f30, f31, f32, f33)                         \
  { { f00, f10, f20, f30 },                              \
    { f01, f11, f21, f31 },                              \
    { f02, f12, f22, f32 },                              \
    { f03, f13, f23, f33 } }

const mat44fgl mat44fgl_identity = {
  CMAJ(1, 0, 0, 0,
       0, 1, 0, 0,
       0, 0, 1, 0,
       0, 0, 0, 1)
};

mat44fgl mat44fgl_ortho(float left, float right,
                        float bottom, float top,
                        float nearval, float farval) {
  /* See https://www.opengl.org/sdk/docs/man2/xhtml/glOrtho.xml */
  float tx = - (right + left) / (right - left);
  float ty = - (top + bottom) / (top - bottom);
  float tz = - (farval + nearval) / (farval - nearval);
  mat44fgl ret = {
    CMAJ(2.0f / (right - left), 0,                     0,                          tx,
         0,                     2.0f / (top - bottom), 0,                          ty,
         0,                     0,                     -2.0f / (farval - nearval), tz,
         0,                     0,                     0,                          1.0f)
  };

  return ret;
}

mat44fgl mat44fgl_scale(float x, float y, float z) {
  /* https://www.opengl.org/sdk/docs/man2/xhtml/glScale.xml */
  mat44fgl ret = {
    CMAJ(x, 0, 0, 0,
         0, y, 0, 0,
         0, 0, z, 0,
         0, 0, 0, 1)
  };

  return ret;
}

mat44fgl mat44fgl_translate(float x, float y, float z) {
  /* https://www.opengl.org/sdk/docs/man2/xhtml/glTranslate.xml */
  mat44fgl ret = {
    CMAJ(1, 0, 0, x,
         0, 1, 0, y,
         0, 0, 1, z,
         0, 0, 0, 1)
  };

  return ret;
}

mat44fgl mat44fgl_multiply(const mat44fgl a,
                           const mat44fgl b) {
  mat44fgl r;
  unsigned i, j, k;
  float sum;

  for (i = 0; i < 4; ++i) {
    for (j = 0; j < 4; ++j) {
      sum = 0.0f;
      for (k = 0; k < 4; ++k)
        sum += a.m[k][i] * b.m[j][k];

      r.m[j][i] = sum;
    }
  }

  return r;
}
