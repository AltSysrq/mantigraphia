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
#ifndef GL_AUXBUFF_H_
#define GL_AUXBUFF_H_

/**
 * @file
 *
 * Convenience routines for rendering to textures.
 *
 * Provides access to an FBO with a 32-bit depth buffer of the same size as the
 * main framebuffer, which can be easily used to render into textures.
 */

/**
 * Initialises the auxiliary buffers with the given dimensions.
 */
void auxbuff_init(unsigned ww, unsigned wh);

/**
 * Reconfigures OpenGL to capture into a texture of the given dimensions. The
 * texture must already be initialised with the desired format and dimensions.
 *
 * A texture of 0 refers to the main framebuffer.
 */
void auxbuff_target_immediate(unsigned tex, unsigned ww, unsigned wh);

/**
 * Arranges to run auxbuff_target_immediate() with the givn parameters via glm.
 */
void auxbuff_target(unsigned tex, unsigned ww, unsigned wh);

#endif /* GL_AUXBUFF_H_ */
