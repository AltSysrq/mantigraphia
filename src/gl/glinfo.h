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
#ifndef GL_GLINFO_H_
#define GL_GLINFO_H_

/* This file holds global variables that provide information on the
 * capabilities of the of the host OpenGL implementation.
 */

/**
 * The maximum size, in pixels, of point sprites that can be rendered, as
 * passed into glPointSize(). Points which would exceed this size are clamped
 * to it.
 *
 * Typically, when a point larger than this is to be drawn, the calling code
 * must instead fall back on using polygons to achieve the desired effect.
 */
extern unsigned max_point_size;

/**
 * If true, the OpenGL implementation correctly handles point clipping; that
 * is, a point will still be rendered if the centre is off-screen, but due to
 * point size, part of the point should still be visible. The nVidia OpenGL
 * implementation is an example of one that handles this correctly.
 *
 * When false, a point with an off-screen centre must manually be drawn with
 * polygons. The Mesa OpenGL implementation is an example of one that culls
 * points solely based upon their centre.
 */
extern int can_draw_offscreen_points;

/**
 * Detects the values to use for the variables declared in this file. This
 * clobbers the write colour buffer, and involves vid-sys blits. This must be
 * called after the matrices have been set up to restore physical pixel
 * correspondence.
 *
 * @param wh The Y coordinate of the bottom of the viewport.
 */
void glinfo_detect(unsigned wh);

#endif /* GL_GLINFO_H_ */
