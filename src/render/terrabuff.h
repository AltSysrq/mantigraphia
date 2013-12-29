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
#ifndef RENDER_TERRABUFF_H_
#define RENDER_TERRABUFF_H_

#include "../coords.h"
#include "../graphics/canvas.h"
#include "../graphics/perspective.h"
#include "context.h"

/**
 * The terrabuff is used for rendering large-scale terrain features. Rendering
 * with a terrabuff is performed in terms of polar coordinates. Some fixed set
 * of equally-spaced angles are chosen, which are invariant with respect to the
 * perspective. The renderer then runs along each angle in order at a fixed
 * distance, telling the terrabuff about features found within each scanned
 * area; after each scan, that distance increases.
 *
 * The terrabuff is indifferent to the algorithm chosen for the distance, as
 * long as it is strictly increasing. Slices must be presented in incremental
 * order, modulo the buffer slice capacity.
 *
 * The terrabuff is also limited as to how many scans it can hold. The client
 * is not required to fill all scans, but must not exceed them.
 *
 * Once all data have been fed into a terrabuff, terrabuff_render() may be
 * called to write the data onto a canvas.
 */
typedef struct terrabuff_s terrabuff;
/**
 * Unsigned integer type indicating terrabuff slice indices.
 */
typedef unsigned short terrabuff_slice;

/**
 * Initialises static resources needed for terrabuff rendering. This should be
 * called exactly once before any other terrabuff functions.
 */
void terrabuff_init(void);

/**
 * Allocates a new terrabuff with capacity for the given number of
 * slices and scans. slice_capacity MUST be a power of two. scan_capacity is
 * unrestricted.
 */
terrabuff* terrabuff_new(terrabuff_slice slice_capacity,
                         unsigned scan_capacity);
/**
 * Frees the memory held by the given terrabuff.
 */
void terrabuff_delete(terrabuff*);

/**
 * Makes the terrabuff ready for a new rendering frame. All buffered data are
 * erased. The buffer will begin scanning from slice l (inclusive) to h
 * (exclusive).
 */
void terrabuff_clear(terrabuff*, terrabuff_slice l, terrabuff_slice h);
/**
 * Prepares the terrabuff to render the next scan. This MUST only be called
 * after a terrabuff_put() which filled the previous scan. l and h are set to
 * the low (inclusive) and high (exclusive) indices which are to be used for
 * the subsequent scan.
 *
 * Returns whether is is possible to continue scanning, respective to available
 * slices. The return value does not take the current scan into account.
 */
int terrabuff_next(terrabuff*, terrabuff_slice* l, terrabuff_slice* h);

/**
 * Informs the terrabuff of the orthogonal screen coordinates of a terrain
 * feture for the given scan/slice pair, incrementing the slice. xmax is the
 * maximum (exclusive) X coordinate of the drawable area.
 */
void terrabuff_put(terrabuff*, const vo3 where, coord_offset xmax);

/**
 * Renders the buffered terrain into the given canvas.
 *
 * This function assumes it is the first thing to render to the canvas, and
 * largely ignores the depth buffer. Z coordinates generally reflect the
 * back-most feature of any terrain segment. This is cheaper to store and
 * render, and is safe as nothing is ever rendered under the terrain. It also
 * reduces the potential for clipping effects due to things drawn near the
 * terrain.
 */
void terrabuff_render(canvas*restrict,
                      terrabuff*restrict,
                      const rendering_context*restrict);

#endif /* RENDER_TERRABUFF_H_ */
