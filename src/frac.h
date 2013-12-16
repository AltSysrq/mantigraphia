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
#ifndef FRAC_H_
#define FRAC_H_

/**
 * A fraction represents essentially a cached division, and thus ranges from
 * 0..1 (similar to a zo_scaling_factor). Multiplications with a fraction are
 * much faster than a division by the original denominator, at the cost of some
 * precision.
 */
typedef unsigned fraction;
#define FRACTION_BITS 31
#define FRACTION_BASE (((fraction)1) << FRACTION_BITS)

static inline fraction fraction_of(unsigned denominator) {
  return FRACTION_BASE / denominator;
}

static inline unsigned fraction_umul(unsigned numerator, fraction mult) {
  unsigned long long tmp64, mult64;
  mult64 = mult;
  tmp64 = numerator;
  tmp64 *= mult64;
  return tmp64 >> FRACTION_BITS;
}

static inline signed fraction_smul(signed numerator, fraction mult) {
  signed long long tmp64, mult64;
  mult64 = (signed long long)(unsigned long long)/*zext*/ mult;
  tmp64 = numerator;
  tmp64 *= mult64;
  return tmp64 >> FRACTION_BITS;
}

#endif /* FRAC_H_ */
