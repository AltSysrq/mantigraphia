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
#ifndef RENDER_FOG_H_
#define RENDER_FOG_H_

#include "../frac.h"
#include "../coords.h"
#include "../graphics/canvas.h"
#include "../graphics/parchment.h"

/**
 * The fog effect simulates fog similar to what can often be seen in paintings
 * (especially the references). Unlike fog effects in traditional computer
 * graphics, it is applied post-facto. (Though the depth buffer should
 * generally be initialised to the far plane anyway.)
 *
 * The state of a fog effect is maintained as a number of "wisps" which drift
 * across a square region. The length of the region can be assumed equal to the
 * greater dimension of the drawing area. Each wisp has an ovular, axis-aligned
 * shape whose dimensions are prortional to the square space (rather than the
 * screen). When a wisp would pass out of the region, it instead gains a new,
 * random velocity proceeding back into the region. Each wisp also has a fixed
 * fractional Z-distance.
 *
 * Fog is applied with a near and a far plane; each wisp offsets these two
 * planes by a value determined by its Z-distance. Anything nearer the near
 * plane is always fully visible and has no effect on the fog; anything behind
 * the far plane is assumed invisible. Pixels between the near and far planes
 * are said to "pierce" the fog. A "middle" region is defined to lie somewhere
 * between the near and far planes. Items between the near and far-middle
 * planes pierce the fog, but are not occluded by it. Items between the
 * far-middle and far planes pierce the fog, but may be occluded by it.
 *
 * Fog piercing, which maximises at the middle plane, causes the boundaries of
 * a wisp to recede. The strength of piercing for a particular wisp is based on
 * both the distance of a pixel from the middle region, as well as the distance
 * between the centre of the wisp and the point in question. At maximum
 * piercing (middle region, zero distance). Piercing for the whole wisp is
 * based on the maximum value of any pixel found near it.
 *
 * The effective area for each wisp is used to replace the screen pixels with
 * the parchment background, at a distance equal to the far-middle plane.
 */
typedef struct fog_effect_s fog_effect;

/**
 * Creates and returns a new fog_effect with the given number of wisps.
 *
 * @param num_wisps The number of wisps to be present in the effect.
 * @param wisp_size_x The size (relative to the greater screen dimension) for
 * each wisp.
 * @param wisp_size_y The size (relative to the greater screen dimension) for
 * each wisp.
 * @param seed Random seed to use.
 */
fog_effect* fog_effect_new(unsigned num_wisps,
                           fraction wisp_size_x,
                           fraction wisp_size_y,
                           unsigned seed);
/**
 * Frees the memory held by the given fog effect.
 */
void fog_effect_delete(fog_effect*);

/**
 * Updates the fog effect for the given amount of elapsed time.
 */
void fog_effect_update(fog_effect*, chronon et);
/**
 * Applies the fog effect to the given canvas, drawing replacement pixels from
 * the given parchment.
 */
void fog_effect_apply(canvas*, fog_effect*, const parchment*,
                      coord near, coord near_mid,
                      coord far_mid, coord far);

#endif /* RENDER_FOG_H_ */
