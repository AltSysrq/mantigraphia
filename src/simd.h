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

#define simd_init4(a,b,c,d) { a,b,c,d }
#define simd_addvv(a,b) ((a)+(b))
#define simd_mulvs(a,b) ((a)*simd_inits(b))
#define simd_divvs(a,b) ((a)/simd_inits(b))
#define simd_vs(a,ix) ((a)[ix])

static inline simd4 simd_inits(signed s) {
  simd4 ret = simd_init4(s,s,s,s);
  return ret;
}

#else

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
#endif /* Not GCC or clang */

#endif /* SIMD_H_ */
