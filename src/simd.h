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
#ifndef SIMD_H_
#define SIMD_H_

/* GCC doesn't seem to allow subscripting vectors (?) until around version 4.7
 * or so, and I haven't found any portable (ie, non-casting) way to do so in
 * those versions...
 */
#if (defined(__GNUC__) && (__GNUC__ > 4 || (__GNUC__ == 4 && __GNUC_MINOR__ >= 7))) || defined(__clang__)
typedef signed simd4 __attribute__((vector_size(16)));

/* Only defined because some operations we need only work for floats. */
typedef float simd4f __attribute__((vector_size(16)));

#define simd_init4(a,b,c,d) { a,b,c,d }
#define simd_addvv(a,b) ((a)+(b))
#define simd_subvv(a,b) ((a)-(b))
#define simd_mulvs(a,b) ((a)*simd_inits(b))
#define simd_divvs(a,b) ((a)/simd_inits(b))
#define simd_shra(v,s) ((v)>>simd_inits(s))
#define simd_vs(a,ix) ((a)[ix])
static inline int simd_all_false(simd4 a) {
#if defined(__SSE__)
  return !__builtin_ia32_movmskps((simd4f)a);
#else
  return !a[0] && !a[1] && !a[2] && !a[3];
#endif
}

static inline int simd_all_true(simd4 a) {
#if defined(__SSE__)
  return 0xF == __builtin_ia32_movmskps((simd4f)a);
#else
  return a[0] && a[1] && a[2] && a[3];
#endif
}

static inline int simd_eq(simd4 a, simd4 b) {
  return simd_all_true(a == b);
}

#define simd_pairwise_lt(a,b) ((a)<(b))

/* GCC calls it shuffle, Clang calls it shufflevector */
#if defined(__clang__)
#define simd_shuffle(src,mask) __builtin_shufflevector(src,mask)
#else
#define simd_shuffle(src,mask) __builtin_shuffle(src,mask)
#endif

typedef unsigned short usimd8s __attribute__((vector_size(16)));

static inline usimd8s simd_umulhi8s(usimd8s a, usimd8s b) {
#if defined(__SSE2__)
  /*
   * return __builtin_ia32_pmulhuw(a,b);
   *
   * GCC and Clang both seem to think that pulhuw operates on a vector of size
   * 4, despite Intel's docs indicating that it works on 8 (as well as the
   * actual effect of the call here), so they complain if we use the builtin to
   * access it. So just write the assembly ourselves. Oh well.
   *
   * (Unfortunately Clang's partial support for intel syntax is broken here, so
   * we have to use the backwards ATT syntax...)
   */
  __asm__("pmulhuw %1, %0" : "+x"(a) : "x"(b));
  return a;
#else
  unsigned a32 __attribute__((vector_size(32))) =
    { a[0], a[1], a[2], a[3], a[4], a[5], a[6], a[7], };
  unsigned b32 __attribute__((vector_size(32))) =
    { b[0], b[1], b[2], b[3], b[4], b[5], b[6], b[7], };
  unsigned m32 __attribute__((vector_size(32))) = (a32 * b32) >> 16;
  usimd8s ret = { m32[0], m32[1], m32[2], m32[3],
                  m32[4], m32[5], m32[6], m32[7] };
  return ret;
#endif
}

static inline usimd8s simd_umullo8s(usimd8s a, usimd8s b) {
#if defined(__SSE2__)
  __asm__("pmullw %1, %0" : "+x"(a) : "x"(b));
  return a;
#else
  unsigned a32 __attribute__((vector_size(32))) =
    { a[0], a[1], a[2], a[3], a[4], a[5], a[6], a[7], };
  unsigned b32 __attribute__((vector_size(32))) =
    { b[0], b[1], b[2], b[3], b[4], b[5], b[6], b[7], };
  unsigned m32 __attribute__((vector_size(32))) = a32 * b32;
  /* implicit truncation */
  usimd8s ret = { m32[0], m32[1], m32[2], m32[3],
                  m32[4], m32[5], m32[6], m32[7] };
  return ret;
#endif
}

static inline usimd8s simd_ufrac8s(unsigned denominator) {
  unsigned n = 0xFFFF / denominator;
  usimd8s ret = { n, n, n, n, n, n, n, n, };
  return ret;
}

#else /* End actual vector instructions, begin compatibility */

typedef struct {
  signed v[4];
} simd4;

#define simd_init4(a,b,c,d) { { a, b, c, d } }
#define simd_vs(a,ix) ((a).v[ix])
static inline simd4 simd_addvv(simd4 a, simd4 b) {
  simd4 r = simd_init4(a.v[0] + b.v[0],
                       a.v[1] + b.v[1],
                       a.v[2] + b.v[2],
                       a.v[3] + b.v[3]);
  return r;
}
static inline simd4 simd_subvv(simd4 a, simd4 b) {
  simd4 r = simd_init4(a.v[0] - b.v[0],
                       a.v[1] - b.v[1],
                       a.v[2] - b.v[2],
                       a.v[3] - b.v[3]);
  return r;
}
static inline simd4 simd_mulvs(simd4 a, signed b) {
  simd4 r = simd_init4(a.v[0] * b,
                       a.v[1] * b,
                       a.v[2] * b,
                       a.v[3] * b);
  return r;
}
static inline simd4 simd_divvs(simd4 a, signed b) {
  simd4 r = simd_init4(a.v[0] / b,
                       a.v[1] / b,
                       a.v[2] / b,
                       a.v[3] / b);
  return r;
}
static inline int simd_eq(simd4 a, simd4 b) {
  return
    a.v[0] == b.v[0] &&
    a.v[1] == b.v[1] &&
    a.v[2] == b.v[2] &&
    a.v[3] == b.v[3];
}

static inline int simd_all_false(simd4 a) {
  return !a.v[0] && !a.v[1] && !a.v[2] && !a.v[3];
}

static inline int simd_all_true(simd4 a) {
  return a.v[0] && a.v[1] && a.v[2] && a.v[3];
}

static inline simd4 simd_pairwise_lt(simd4 a, simd4 b) {
  simd4 r = simd_init4(a.v[0] < b.v[0],
                       a.v[1] < b.v[1],
                       a.v[2] < b.v[2],
                       a.v[3] < b.v[3]);
  return r;
}

static inline simd4 simd_shra(simd4 v, unsigned char shift) {
  simd4 ret = simd_init4(v.v[0] >> shift,
                         v.v[1] >> shift,
                         v.v[2] >> shift,
                         v.v[3] >> shift);
  return ret;
}

static inline simd4 simd_shuffle(simd4 src, simd4 mask) {
  simd4 ret = simd_init4(src.v[mask.v[0]],
                         src.v[mask.v[1]],
                         src.v[mask.v[2]],
                         src.v[mask.v[3]]);
  return ret;
}

typedef struct {
  unsigned short v[8];
} usimd8s;

static inline usimd8s simd_umulhi8s(usimd8s a, usimd8s b) {
  usimd8s ret;
  unsigned i, tmp;

  for (i = 0; i < 8; ++i) {
    tmp = a.v[i];
    tmp *= b.v[i];
    tmp >>= 16;
    ret.v[i] = tmp;
  }
}

static inline usimd8s simd_umullo8s(usimd8s a, usimd8s b) {
  usimd8s ret;
  unsigned i, tmp;

  for (i = 0; i < 8; ++i) {
    tmp = a.v[i];
    tmp *= b.v[i];
    ret.v[i] = tmp & 0xFFFF;
  }
}

static inline usimd8s simd_ufrac8s(unsigned denominator) {
  unsigned n = 0xFFFF / denominator;
  usimd8s ret = { { n, n, n, n, n, n, n, n, } };
  return ret;
}
#endif /* Not GCC>=4.7 or clang */


static inline simd4 simd_inits(signed s) {
  simd4 ret = simd_init4(s,s,s,s);
  return ret;
}

static inline simd4 simd_initl(signed a, signed b, signed c, signed d) {
  simd4 ret = simd_init4(a,b,c,d);
  return ret;
}

static inline simd4 simd_of_vo4(const signed v[4]) {
  return simd_initl(v[0], v[1], v[2], v[3]);
}

static inline void simd_to_vo4(signed d[4], simd4 s) {
  d[0] = simd_vs(s, 0);
  d[1] = simd_vs(s, 1);
  d[2] = simd_vs(s, 2);
  d[3] = simd_vs(s, 3);
}

typedef void*restrict simd_aligned_ptr __attribute__((aligned(16)));

static inline simd4 simd_of_aligned(simd_aligned_ptr vmem) {
  return *(const simd4*restrict)vmem;
}

static inline void simd_store_aligned(
  simd_aligned_ptr vmem,
  simd4 s)
{
  *(simd4*restrict)vmem = s;
}

#endif /* SIMD_H_ */
