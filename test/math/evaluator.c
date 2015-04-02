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

#include "test.h"
#include "defs.h"
#include "math/coords.h"
#include "math/frac.h"
#include "math/evaluator.h"

defsuite(evaluator);

#define MOST_NEGATIVE (-0x8000000000000000LL)

#define DATA(n)                                 \
  evaluator_cell c[n];                          \
  evaluator_value val[n];                       \
  evaluator_builder builder;                    \
  evaluator_builder_init(&builder, c, lenof(c))


#define EXECUTE()                                               \
  evaluator_execute(val, c, evaluator_builder_n(&builder))

deftest(single_const) {
  DATA(1);
  ck_assert_int_eq(0, evaluator_const(&builder, 42LL));
  EXECUTE();

  ck_assert_int_eq(42, val[0]);
}

deftest(single_nop) {
  DATA(1);
  ck_assert_int_eq(0, evaluator_nop(&builder));
  val[0] = 42LL;
  EXECUTE();

  ck_assert_int_eq(42, val[0]);
}

#define TRITEST(expected,op,a,b,c)                        \
  DATA(4);                                                \
  evaluator_const(&builder, (a));                         \
  evaluator_const(&builder, (b));                         \
  evaluator_const(&builder, (c));                         \
  ck_assert_int_eq(3, evaluator_##op(&builder, 0, 1, 2)); \
  EXECUTE();                                              \
  ck_assert_int_eq((expected), val[3])
#define BINTEST(expected,a,op,b)                       \
  DATA(3);                                             \
  evaluator_const(&builder, (a));                      \
  evaluator_const(&builder, (b));                      \
  ck_assert_int_eq(2, evaluator_##op(&builder, 0, 1)); \
  EXECUTE();                                           \
  ck_assert_int_eq((expected), val[2])
#define UNITEST(expected,op,a)                         \
  DATA(3);                                             \
  evaluator_const(&builder, (a));                      \
  ck_assert_int_eq(1, evaluator_##op(&builder, 0));    \
  EXECUTE();                                           \
  ck_assert_int_eq((expected), val[1])

deftest(basic_add) {
  BINTEST(5, 2, add, 3);
}

deftest(basic_sub) {
  BINTEST(-1, 2, sub, 3);
}

deftest(basic_mul) {
  BINTEST(6, 2, mul, 3);
}

deftest(mul_MOST_NEGATIVE_by_negative_one) {
  BINTEST(MOST_NEGATIVE, MOST_NEGATIVE, mul, -1);
}

deftest(basic_div) {
  BINTEST(2, 5, div, 2);
}

deftest(div_by_zero) {
  BINTEST(0, 5, div, 0);
}

deftest(div_MOST_NEGATIVE_by_negative_one) {
  BINTEST(MOST_NEGATIVE, MOST_NEGATIVE, div, -1);
}

deftest(div_by_negative_one) {
  BINTEST(-5, 5, div, -1);
}

deftest(basic_mod) {
  BINTEST(1, 6, mod, 5);
}

deftest(mod_num_is_negative) {
  BINTEST(1, -4, mod, 5);
}

deftest(mod_by_zero) {
  BINTEST(0, 1, mod, 0);
}

deftest(mod_by_negative) {
  BINTEST(0, 1, mod, -5);
}

deftest(neg_of_positive) {
  UNITEST(-1, neg, 1);
}

deftest(neg_of_negative) {
  UNITEST(+1, neg, -1);
}

deftest(neg_of_zero) {
  UNITEST(0, neg, 0);
}

deftest(neg_of_most_negative) {
  UNITEST(MOST_NEGATIVE, neg, MOST_NEGATIVE);
}

deftest(abs_of_positive) {
  UNITEST(3, abs, 3);
}

deftest(abs_of_negative) {
  UNITEST(3, abs, -3);
}

deftest(abs_of_zero) {
  UNITEST(0, abs, 0);
}

deftest(abs_of_most_negative) {
  UNITEST(MOST_NEGATIVE, abs, MOST_NEGATIVE);
}

deftest(to_angle_small_positive) {
  UNITEST(42, to_angle, 42);
}

deftest(to_angle_small_negative) {
  UNITEST(-42, to_angle, -42);
}

deftest(to_angle_large_positive) {
  UNITEST(-32768+42, to_angle, 32768+42);
}

deftest(to_angle_large_negative) {
  UNITEST(32768-42, to_angle, -32768-42);
}

deftest(cos_zero) {
  UNITEST(ZO_SCALING_FACTOR_MAX, cos, 0);
}

deftest(cos_quarter) {
  UNITEST(0, cos, -16384);
}

deftest(cos_large_integer) {
  UNITEST(ZO_SCALING_FACTOR_MAX, cos, 65536*4);
}

deftest(sin_zero) {
  UNITEST(0, sin, 0);
}

deftest(sin_quarter) {
  UNITEST(-ZO_SCALING_FACTOR_MAX, sin, -16384);
}

deftest(sin_large_integer) {
  UNITEST(0, sin, 65536*4);
}

deftest(basic_sqrt) {
  UNITEST(2, sqrt, 5);
}

deftest(sqrt_zero) {
  UNITEST(0, sqrt, 0);
}

deftest(sqrt_negative) {
  UNITEST(-2, sqrt, -5);
}

deftest(logand_first_true) {
  BINTEST(2, 1, logand, 2);
}

deftest(logand_first_false) {
  BINTEST(0, 0, logand, 2);
}

deftest(logor_first_true) {
  BINTEST(1, 1, logor, 2);
}

deftest(logor_first_false) {
  BINTEST(2, 0, logor, 2);
}

deftest(logor_both_false) {
  BINTEST(0, 0, logor, 0);
}

deftest(lognot_of_true) {
  UNITEST(0, lognot, 42);
}

deftest(lognot_of_false) {
  UNITEST(1, lognot, 0);
}

deftest(equ_true) {
  BINTEST(1, 42, equ, 42);
}

deftest(equ_false) {
  BINTEST(0, 42, equ, 41);
}

deftest(neq_true) {
  BINTEST(1, 42, neq, 41);
}

deftest(neq_false) {
  BINTEST(0, 42, neq, 42);
}

deftest(lt_true) {
  BINTEST(1, 41, lt, 42);
}

deftest(lt_false) {
  BINTEST(0, 42, lt, 42);
}

deftest(le_true) {
  BINTEST(1, 42, le, 42);
}

deftest(le_false) {
  BINTEST(0, 43, le, 42);
}

deftest(gt_true) {
  BINTEST(1, 43, gt, 42);
}

deftest(gt_false) {
  BINTEST(0, 42, gt, 42);
}

deftest(ge_true) {
  BINTEST(1, 42, ge, 42);
}

deftest(ge_false) {
  BINTEST(0, 41, ge, 42);
}

deftest(if_true) {
  TRITEST(2, if, 1, 2, 3);
}

deftest(if_false) {
  TRITEST(3, if, 0, 2, 3);
}

deftest(clamp_between) {
  TRITEST(2, clamp, 1, 3, 2);
}

deftest(clamp_below_minimum) {
  TRITEST(1, clamp, 1, 3, 0);
}

deftest(clamp_above_maximum) {
  TRITEST(3, clamp, 1, 3, 42);
}

deftest(clamp_impossible_range) {
  TRITEST(1, clamp, 1, -1, 0);
}

deftest(basic_clamp_min) {
  BINTEST(1, 1, clamp_min, 0);
}

deftest(basic_clamp_max) {
  BINTEST(0, 0, clamp_max, 1);
}

deftest(basic_fraction_of) {
  UNITEST(fraction_of(4), fraction_of, 4);
}

deftest(fraction_of_zero) {
  UNITEST(fraction_of(1), fraction_of, 0);
}

deftest(fraction_of_negative) {
  UNITEST(fraction_of(1), fraction_of, -1);
}

deftest(basic_fraction_smul) {
  BINTEST(-2, -4, fraction_smul, fraction_of(2));
}

deftest(basic_fraction_umul) {
  BINTEST(1 << 30, 1 << 31, fraction_umul, fraction_of(2));
}

deftest(basic_evaluator_zoscale) {
  BINTEST(-2, 4, zoscale, -ZO_SCALING_FACTOR_MAX/2);
}
