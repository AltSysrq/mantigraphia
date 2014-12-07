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
#ifndef RENDER_PROPS_H_
#define RENDER_PROPS_H_

#include "../math/frac.h"
#include "../world/terrain-tilemap.h"
#include "../world/props.h"
#include "context.h"

/**
 * Specifies a method to render a particular type of prop. These are usually
 * stored in tables passed to render_world_props().
 */
typedef void (*prop_renderer)(const world_prop*,
                              const terrain_tilemap*,
                              unsigned level, fraction level_progression,
                              const rendering_context*restrict);

/**
 * Renders all props in the select region whose effective level of detail is
 * greater than zero. Algorithmic complexity is primarily based upon the
 * distance between zmax and zmin, and far less so on xmax and xmin, so only
 * the Z axis should be divided when dividing work across threads.
 *
 * It is legal for either maximum to be less than the corresponding
 * minimum. This is interpreted to mean that the axis starts at the minimum and
 * increases until it wraps around and eventually reaches the maximum.
 *
 * @param props An array of props to be drawn.
 * @param num_props The number of props within props.
 * @param world The terrain of the world being rendered.
 * @param xmin The minimum X coordinate of any prop to draw.
 * @param xmax The maximum X coordinate of any prop to draw.
 * @param zmin The minimum Z coordinate of any prop to draw.
 * @param zmax The maximum Z coordinate of any prop to draw.
 * @param distsq_shift Amount by which to right-shift the square distance of
 * any prop within the bounding rectangle, in order to determine whether to
 * draw it. Effectively, the distance is divided by (sqrt(2)**dstsq_shift);
 * only props whose resulting distance is less than 64 will be drawn.
 * @param renderers A table of renderers, indexed by world_prop.type, which are
 * appropriate to render this group of props.
 */
void render_world_props(const world_prop* props,
                        unsigned num_props,
                        const terrain_tilemap* world,
                        coord xmin, coord xmax,
                        coord zmin, coord zmax,
                        unsigned char distsq_shift,
                        const prop_renderer* renderers,
                        const rendering_context*restrict);

#endif /* RENDER_PROPS_H_ */
