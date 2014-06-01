/*-
 * Copyright (c) 2013, 2014 Jason Lingle
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
#ifndef COORDS_H_
#define COORDS_H_

#include <stdlib.h>

/**
 * @file
 * Defines types, units, and common functions for manipulating Mantigraphia's
 * integer-based coordinate system, including angle and time.
 *
 * Using integer-based coordinates is important for a number of
 * reasons. Scalar integer operations are generally faster on most processors
 * than floating-point operations. Fixed-point representations maintain an
 * equal and known precision across the whole space. Finally, we can be fully
 * certain that identical calculations involving them will have exactly equal
 * results across all compilers, optimisation levels, and architectures, and
 * that commutative operations really are commutative.
 */

/**
 * Spacial coordinates are represented by 32-bit integers, where the high word
 * is metres and the low word is 65536ths of a metre. A "millimetre" is
 * 1/1024th of a metre.
 *
 * Space is torroidal at some offset for the X and Z axes, so a signed 32-bit
 * integer is sufficient to represent the offset between any two points on
 * those axes. The Y axis is expected to be capped at some reasonable point
 * below 32 km.
 *
 * Coordinates and offsets may be converted to OpenGL floating-point
 * coordinates, but may not be converted back. Floating-point units are based
 * on metres.
 */
typedef unsigned coord;
typedef signed coord_offset;
#define METRE      ((coord_offset)0x00010000)
#define MILLIMETRE ((coord_offset)0x00000040)

typedef coord vc3[3];
typedef coord_offset vo3[3];

static inline coord_offset torus_dist(coord_offset base_off, coord wrap_point) {
  /* By definition, abs(base_off) < wrap_point. If abs(base_off) <=
   * wrap_point/2, wrapping would not result in a shorter distance, so base_off
   * is the correct distance. Otherwise, wrapping will produce a shorter
   * distance.
   *
   * The wrapped value will always be of opposite sign as the unwrapped offset,
   * since they naturally must point in opposite directions around the torus.
   */
  if (((unsigned)abs(base_off)) <= wrap_point / 2)
    return base_off;
  else if (base_off < 0)
    /* Want positive. wrap_point > abs(base_off), so wrap_point + negative
     * base_off is always positive.
     */
    return wrap_point + /*negative*/ base_off;
  else
    /* Want negative. base_off is positive, but less than wrap_point */
    return base_off - wrap_point;
}

static inline void vc3dist(vo3 dst, const vc3 a, const vc3 b,
                           coord x_wrap_point, coord z_wrap_point) {
  dst[0] = torus_dist(a[0] - b[0], x_wrap_point);
  dst[1] = a[1] - b[1];
  dst[2] = torus_dist(a[2] - b[2], z_wrap_point);
}

static inline unsigned clampu(unsigned min, unsigned x, unsigned max) {
  if (min > x) return min;
  if (max < x) return max;
  else         return x;
}

static inline signed clamps(signed min, signed x, signed max) {
  if (min > x) return min;
  if (max < x) return max;
  else         return x;
}

/**
 * Time is tracked in terms of chronons, there being 64 chronons in one wall
 * second. A 32-bit integer is sufficient for about 2 years of simulation at
 * this rate.
 */
typedef unsigned chronon;
#define SECOND     ((chronon)0x00000040)

/**
 * Velocity is expressed as space quanta per chronon. The minimum expressable
 * speed is 1 millimetre per second.
 */
typedef coord_offset velocity;
#define METRES_PER_SECOND ((velocity)(METRE / SECOND))
#define MM_PER_SECOND     ((velocity)1)

/**
 * Acceleration is expressed as velocity quanta per chronon. The minimum
 * expressable acceleration is 64 mm/s/s.
 *
 * Gravity is negative acceleration along the Y axis. It is about 9.81 m/s/s
 * (actually 9m+768mm / s / s).
 */
typedef coord_offset acceleration;
#define METRES_PER_SS ((acceleration)(METRE / SECOND / SECOND))
#define GRAVITY ((acceleration)-((9*METRE + 810*MILLIMETRE) / SECOND / SECOND))

/**
 * Angle is expressed in units of 1/65536th of a circle. Most calculations care
 * only about the upper 12 bits (1/4096th of a circle), however. The lower bits
 * only exist to support intermediate values.
 */
typedef signed short angle;
#define DEG_90  ((angle)0x4000)
#define DEG_180 ((angle)0x8000)
#define DEG_270 ((angle)0xC000)

/**
 * Angular velocity is expressed in terms of angular quanta per chronon.
 */
typedef signed short angular_velocity;

/**
 * A zo_scaling_factor is a 15-bit signed integer which can be used to scale a
 * coordinate or offset by a logical value between 0 and 1, inclusive. In order
 * to support +1.0, the legal range of this type is -16384 to +16384 instead of
 * the more expected -32768 to +32767.
 */
typedef signed short zo_scaling_factor;
#define ZO_SCALING_FACTOR_BITS 14
#define ZO_SCALING_FACTOR_MAX ((zo_scaling_factor)(1 << ZO_SCALING_FACTOR_BITS))

static inline signed zo_scale(signed input, zo_scaling_factor factor) {
  signed long long value = input;
  value *= factor;
  value /= ZO_SCALING_FACTOR_MAX;

  return (signed)value;
}

#define ZO_COSINE_COUNT 4096
extern const zo_scaling_factor zo_cosine[ZO_COSINE_COUNT];

static inline zo_scaling_factor zo_cos(angle ang) {
  return zo_cosine[(ang >> 4) & (ZO_COSINE_COUNT-1)];
}

static inline zo_scaling_factor zo_sin(angle ang) {
  return zo_cos(ang - DEG_90);
}

static inline signed zo_cosms(angle ang, signed value) {
  return zo_scale(value, zo_cos(ang));
}

static inline signed zo_sinms(angle ang, signed value) {
  return zo_scale(value, zo_sin(ang));
}

static inline void cossinms(signed* x, signed* y, angle ang, signed dist) {
  *x = zo_cosms(ang, dist);
  *y = zo_sinms(ang, dist);
}

unsigned isqrt(unsigned long long);

#define FISQRT_CNT 4096
extern const unsigned char fisqrt_table[FISQRT_CNT];

/* Like isqrt(), but result is undefined if input >= FISQRT_CNT */
static inline unsigned fisqrt(unsigned short i) {
  return fisqrt_table[i];
}

static inline unsigned cmagnitude(const vc3 coords) {
  return isqrt(
    ((unsigned long long)coords[0])*coords[0] +
    ((unsigned long long)coords[1])*coords[1] +
    ((unsigned long long)coords[2])*coords[2]);
}

static inline unsigned omagnitude(const vo3 coords) {
  return isqrt(
    ((signed long long)coords[0])*coords[0] +
    ((signed long long)coords[1])*coords[1] +
    ((signed long long)coords[2])*coords[2]);
}

#endif /* COORDS_H_ */
