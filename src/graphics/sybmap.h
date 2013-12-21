/*-
 * Copyright (c) 2013 Jason Lingle
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
#ifndef GRAPHICS_SYBMAP_H_
#define GRAPHICS_SYBMAP_H_

#include "../coords.h"

/**
 * A sybmap (Screen Y Bounds map) allows for rapid discarding of renderable
 * objects when rendering a scene front-to-back with vertically contiguous
 * objects. This is accomplished by tracking the minimum and maximum Y
 * coordinates known to be already obscured on every X column of the
 * screen. Areas which can be shown to be entirely obscured can be discarded
 * without needing to be rendered and tested against a Z buffer.
 */
typedef struct sybmap_s sybmap;

/**
 * Allocates a new sybmap which can be used to pre-test rendering against a
 * canvas of the given dimensions.
 *
 * The contents of a new sybmap must be initialised by calling sybmap_clear().
 */
sybmap* sybmap_new(unsigned w, unsigned h);
/**
 * Frees the memory held by the given sybmap.
 */
void sybmap_delete(sybmap*);

/**
 * Resets the given sybmap such that nothing is considered obscured.
 */
void sybmap_clear(sybmap*);
/**
 * Tests whether anything in the specified bounding box (in screen coordinates)
 * is actually visible. Coordinates are clamped to screen boundaries. This test
 * is conservative any may sometimes report false positives (but never false
 * negatives).
 *
 * The coordinates need to be given in the correct order: xl <= xh; yl <= yh.
 */
int sybmap_test(const sybmap*,
                coord_offset xl, coord_offset xh,
                coord_offset yl, coord_offset yh);
/**
 * Updates the given sybmap by marking the region indicated by the given
 * triangle, in screen coordinates.
 */
void sybmap_put(sybmap*, const vo3, const vo3, const vo3);

/**
 * Copies the data of the source sybmap into the destination sybmap. Behaviour
 * is undefined if the two sybmaps were created with differring parameters to
 * sybmap_new().
 */
void sybmap_copy(sybmap*, const sybmap*);

#endif /* GRAPHICS_SYBMAP_H_ */
