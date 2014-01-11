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
#ifndef GL_SHADER_LOADER_H_
#define GL_SHADER_LOADER_H_

#include <SDL_opengl.h>

/**
 * Loads a shader if it has not already been loaded. The first parameter is set
 * to the value of the shader that was loaded. It is assumed to have already
 * been loaded if it is non-zero comming into this function.
 *
 * The basename is simply the filename without a path or extension, eg,
 * "fcanvas". The first character of the basename indicates the type of shader;
 * it must be either 'v' for GL_VERTEX_SHADER or 'f' for GL_FRAGMENT_SHADER.
 *
 * The shader loaded from the file should not define its GLSL version; the
 * appropriate version is implicitly prepended to the source. This is needed
 * because the GLSL versions for equivalent OpenGL and OpenGL ES versions are
 * different; we can't use #if/#else to choose versions, either, because the
 * ATI GLSL compiler (at least some versions) requires the #version declaration
 * to be on the zeroth character.
 *
 * If anything fails, a diagnostic is printed and the program exits.
 */
void load_shader(GLuint*, const char* basename);

#endif /* GL_SHADER_LOADER_H_ */
