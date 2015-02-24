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
#ifndef MATH_SSE_H_
#define MATH_SSE_H_

#include <stdlib.h>

/**
 * @file
 *
 * Exposes the SSE, SSE2, and SSE3 instruction sets in a type-safe manner. This
 * requires compiler support for the GCC-style vector extensions.
 *
 * Where possible, the GCC vector instructions are used directly to invoke the
 * SSE instruction. Failing that, inline assembly is used. Finally, the
 * instructions are emulated.
 *
 * Only instructions actually used by the application are here, since
 * everything else would otherwise be untested.
 */

/* GCC doesn't seem to allow subscripting vectors until around version 4.7 (at
 * least that's the earliest version I've tested this to work on), and there
 * doesn't seem to be any portable way in such versions to extract individual
 * elements from the vectors, so pretend such versions of GCC don't have vector
 * extensions at all.
 */
#if defined(__GNUC__) && (__GNUC__ > 4 || __GNUC__ == 4 && __GNUC_MINOR__ >= 7)
#define GNUC_SUPPORTS_VECTOR_EXTENSIONS
#endif

#if (defined(GNUC_SUPPORTS_VECTOR_EXTENSIONS) || defined(__clang__)) && \
  !defined(EMULATE_VECTOR_EXTENSIONS)
#define USE_VECTOR_EXTENSIONS 1
#ifdef __SSE__
#define USE_SSE 1
#define USE_VEC128 1
#endif
#ifdef __SSE2__
#define USE_SSE2 1
#endif
#ifdef __SSE3__
#define USE_SSE3 1
#endif
#else
#define USE_VECTOR_EXTENSIONS 0
#endif

#ifndef USE_SSE
#define USE_SSE 0
#endif
#ifndef USE_SSE2
#define USE_SSE2 0
#endif
#ifndef USE_SSE3
#define USE_SSE3 0
#endif

#if USE_VECTOR_EXTENSIONS

typedef unsigned char sseb __attribute__((vector_size(16)));
typedef unsigned short ssew __attribute__((vector_size(16)));
typedef signed ssepi __attribute__((vector_size(16)));
typedef float sseps __attribute__((vector_size(16)));

#define SSE_INITV4(a,b,c,d) {a,b,c,d}

#else

typedef struct { unsigned char v[16]; } sseb;
typedef struct { unsigned short v[8]; } ssew;
typedef struct { unsigned v[4]; } ssepi;
typedef struct { float v[4]; } sseps;

#define SSE_INITV4(a,b,c,d) {{a,b,c,d}}

#endif


/* This has no direct equivalent in SSE; just use the vector extensions and let
 * the compiler choose which possible implementation is best.
 *
 * vs == "vector subscript"
 */
#if USE_VECTOR_EXTENSIONS
#define SSE_VS(a,ix) ((a)[ix])
#else
#define SSE_VS(a,ix) ((a).v[ix])
#endif

static inline signed sse_movmskps(sseps a) {
#if USE_SSE
  return __builtin_ia32_movmskps(a);
#else
  union {
    unsigned i;
    float f;
  } p[4];
  p[0].f = SSE_VS(a,0);
  p[1].f = SSE_VS(a,1);
  p[2].f = SSE_VS(a,2);
  p[3].f = SSE_VS(a,3);
#define HIBIT(n) (!!((n) & 0x80000000))
  return HIBIT(p[0].i) | (HIBIT(p[1].i) << 1) |
    (HIBIT(p[2].i) << 2) | (HIBIT(p[3].i) << 3);
#undef HIBIT
#endif
}

static inline sseps sse_addps(sseps a, sseps b) {
#if USE_VECTOR_EXTENSIONS
  a += b;
#else
  a.v[0] += b.v[0];
  a.v[1] += b.v[1];
  a.v[2] += b.v[2];
  a.v[3] += b.v[3];
#endif
  return a;
}

static inline sseps sse_subps(sseps a, sseps b) {
#if USE_VECTOR_EXTENSIONS
  a -= b;
#else
  a.v[0] -= b.v[0];
  a.v[1] -= b.v[1];
  a.v[2] -= b.v[2];
  a.v[3] -= b.v[3];
#endif
  return a;
}

static inline sseps sse_mulps(sseps a, sseps b) {
#if USE_VECTOR_EXTENSIONS
  a *= b;
#else
  a.v[0] *= b.v[0];
  a.v[1] *= b.v[1];
  a.v[2] *= b.v[2];
  a.v[3] *= b.v[3];
#endif
  return a;
}

static inline sseps sse_divps(sseps a, sseps b) {
#if USE_VECTOR_EXTENSIONS
  a /= b;
#else
  a.v[0] /= b.v[0];
  a.v[1] /= b.v[1];
  a.v[2] /= b.v[2];
  a.v[3] /= b.v[3];
#endif
  return a;
}

static inline ssepi sse_addpi(ssepi a, ssepi b) {
#if USE_VECTOR_EXTENSIONS
  a += b;
#else
  a.v[0] += b.v[0];
  a.v[1] += b.v[1];
  a.v[2] += b.v[2];
  a.v[3] += b.v[3];
#endif
  return a;
}

static inline ssepi sse_subpi(ssepi a, ssepi b) {
#if USE_VECTOR_EXTENSIONS
  a -= b;
#else
  a.v[0] -= b.v[0];
  a.v[1] -= b.v[1];
  a.v[2] -= b.v[2];
  a.v[3] -= b.v[3];
#endif
  return a;
}

static inline ssepi sse_mulpi(ssepi a, ssepi b) {
#if USE_VECTOR_EXTENSIONS
  a *= b;
#else
  a.v[0] *= b.v[0];
  a.v[1] *= b.v[1];
  a.v[2] *= b.v[2];
  a.v[3] *= b.v[3];
#endif
  return a;
}

static inline ssepi sse_divpi(ssepi a, ssepi b) {
#if USE_VECTOR_EXTENSIONS
  a /= b;
#else
  a.v[0] /= b.v[0];
  a.v[1] /= b.v[1];
  a.v[2] /= b.v[2];
  a.v[3] /= b.v[3];
#endif
  return a;
}

static inline ssepi sse_srad(ssepi a, ssepi b) {
#if USE_VECTOR_EXTENSIONS
  a >>= b;
#else
  a.v[0] >>= b.v[0];
  a.v[1] >>= b.v[1];
  a.v[2] >>= b.v[2];
  a.v[3] >>= b.v[3];
#endif
  return a;
}

static inline ssepi sse_sradi(ssepi a, unsigned b) {
#if USE_VECTOR_EXTENSIONS
  ssepi bv = { b, b, b, b };
  a >>= bv;
#else
  a.v[0] >>= b;
  a.v[1] >>= b;
  a.v[2] >>= b;
  a.v[3] >>= b;
#endif
  return a;
}

#if USE_VECTOR_EXTENSIONS
#define SSE_EQU(a,b) (!!sse_movmskps((sseps)(a==b)))
#else
#define SSE_EQU(a,b) (                          \
  (a).v[0] == (b).v[0] &&                       \
  (a).v[1] == (b).v[1] &&                       \
  (a).v[2] == (b).v[2] &&                       \
  (a).v[3] == (b).v[3]                          \
    )
#endif

static inline ssepi sse_piof(signed a, signed b, signed c, signed d) {
  ssepi r = SSE_INITV4(a, b, c, d);
  return r;
}

static inline sseps sse_psof(float a, float b, float c, float d) {
  sseps r = SSE_INITV4(a, b, c, d);
  return r;
}

static inline ssepi sse_piof1(signed a) {
  return sse_piof(a, a, a, a);
}

static inline sseps sse_psof1(float a) {
  return sse_psof(a, a, a, a);
}

#endif /* MATH_SSE_H_ */
