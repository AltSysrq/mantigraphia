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
#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include <stdlib.h>

#include "rand.h"
#include "coords.h"
#include "frac.h"
#include "evaluator.h"

void evaluator_execute(evaluator_value* dst, const evaluator_cell* eval,
                       unsigned n) {
  unsigned i;
  evaluator_value a0, a1 = 0, a2 = 0;

  for (i = 0; i < n; ++i) {
    switch (eval[i].format) {
    case ecf_direct:
      a0 = eval[i].value.direct;
      break;

    case ecf_indirect:
      a0 = dst[eval[i].value.indirect[0]];
      a1 = dst[eval[i].value.indirect[1]];
      a2 = dst[eval[i].value.indirect[2]];
      break;

    default:
      abort();
      break;
    }

    dst[i] = (*eval[i].f)(a0, a1, a2);
  }
}

void evaluator_builder_init(evaluator_builder* this,
                            evaluator_cell* cells,
                            unsigned n) {
  this->ix = 0;
  this->max = n;
  this->cells = cells;
}

static unsigned evaluator_builder_next(evaluator_builder* this) {
  if (evaluator_builder_is_full(this))
    abort();

  return this->ix++;
}

static evaluator_value evaluator_const_f(evaluator_value v,
                                         evaluator_value b,
                                         evaluator_value c) {
  return v;
}

unsigned evaluator_const(evaluator_builder* this, evaluator_value v) {
  unsigned ix = evaluator_builder_next(this);
  this->cells[ix].format = ecf_direct;
  this->cells[ix].value.direct = v;
  this->cells[ix].f = evaluator_const_f;
  return ix;
}

unsigned evaluator_nop(evaluator_builder* this) {
  unsigned ix = evaluator_builder_next(this);
  this->cells[ix].format = ecf_indirect;
  this->cells[ix].value.indirect[0] = ix;
  this->cells[ix].value.indirect[1] = ix;
  this->cells[ix].value.indirect[2] = ix;
  this->cells[ix].f = evaluator_const_f;
  return ix;
}

static unsigned evaluator_i3(evaluator_builder* this,
                             evaluator_f f,
                             unsigned a, unsigned b, unsigned c) {
  unsigned ix = evaluator_builder_next(this);
  this->cells[ix].format = ecf_indirect;
  this->cells[ix].value.indirect[0] = a;
  this->cells[ix].value.indirect[1] = b;
  this->cells[ix].value.indirect[2] = c;
  this->cells[ix].f = f;
  return ix;
}

#define EVF1(name,a)                                            \
  static evaluator_value evaluator_##name##_f(                  \
    evaluator_value a, evaluator_value _b, evaluator_value _c); \
  unsigned evaluator_##name(                                    \
    evaluator_builder* this, unsigned a                         \
  ) {                                                           \
    return evaluator_i3(this, evaluator_##name##_f, a, a, a);   \
  }                                                             \
  static evaluator_value evaluator_##name##_f(                  \
    evaluator_value a, evaluator_value _b, evaluator_value _c)
#define EVF2(name,a,b)                                          \
  static evaluator_value evaluator_##name##_f(                  \
    evaluator_value a, evaluator_value b, evaluator_value _c);  \
  unsigned evaluator_##name(                                    \
    evaluator_builder* this, unsigned a, unsigned b             \
  ) {                                                           \
    return evaluator_i3(this, evaluator_##name##_f, a, b, b);   \
  }                                                             \
  static evaluator_value evaluator_##name##_f(                  \
    evaluator_value a, evaluator_value b, evaluator_value _c)
#define EVF3(name,a,b,c)                                        \
  static evaluator_value evaluator_##name##_f(                  \
    evaluator_value a, evaluator_value b, evaluator_value c);   \
  unsigned evaluator_##name(                                    \
    evaluator_builder* this, unsigned a,unsigned b, unsigned c  \
  ) {                                                           \
    return evaluator_i3(this, evaluator_##name##_f, a, b, c);   \
  }                                                             \
  static evaluator_value evaluator_##name##_f(                  \
    evaluator_value a, evaluator_value b, evaluator_value c)

EVF2(add, a, b) { return a + b; }
EVF2(sub, a, b) { return a - b; }
EVF2(mul, a, b) { return a * b; }
EVF2(div, a, b) {
  if (0 == b || -1LL == b)
    return a * b;
  else
    return a / b;
}

EVF2(mod, a, b) {
  evaluator_value m;

  if (b <= 0)
    return 0;

  m = a % b;
  if (m < 0) m += b;
  return m;
}

EVF1(neg, a) { return -a; }
EVF1(abs, a) { return a < 0? -a : +a; }
EVF1(to_angle, a) { return (evaluator_value)(angle)a; }
EVF1(cos, a) { return zo_cos(a); }
EVF1(sin, a) { return zo_sin(a); }
EVF1(sqrt, a) {
  if (a < 0)
    return -(evaluator_value)(signed)isqrt(-a);
  else
    return isqrt(a);
}

EVF3(magnitude, a, b, c) { return isqrt(a*a + b*b + c*c); }

EVF2(logand, a, b) { return !a? a : b; }
EVF2(logor, a, b) { return a? a : b; }
EVF1(lognot, a) { return !a; }
EVF3(if, a, b, c) { return a? b : c; }
EVF3(clamp, min, max, v) {
  if (v <= min) return min;
  if (v >= max) return max;
  return v;
}

EVF2(clamp_min, a, b) { return a > b? a : b; }
EVF2(clamp_max, a, b) { return a < b? a : b; }

EVF1(fraction_of, denom) {
  if (denom <= 0) denom = 1;
  return fraction_of(denom);
}

EVF2(fraction_smul, a, b) { return fraction_smul(a, b); }
EVF2(fraction_umul, a, b) { return fraction_umul(a, b); }
EVF2(zoscale, a, b) { return zo_scale(a, b); }

EVF3(chaos, a, b, c) {
  return chaos_of(
    chaos_accum(
      chaos_accum(
        chaos_accum(0, a), b), c));
}
