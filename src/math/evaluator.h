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
#ifndef MATH_EVALUATOR_H_
#define MATH_EVALUATOR_H_

/**
 *@file
 *
 * Evaluators provide a way to evaluate mathematical functions quickly,
 * making it practical for non-C code to describe, eg, how certain physics or
 * interactions should work. These evaluators are deliberately designed to have
 * the following properties:
 *
 * - Runtime for a particular function is finite and constant. Script-provided
 *   functions can thus not freeze the process, and the developer who specifies
 *   them can be certain there aren't pathological cases that slow the process
 *   down.
 *
 * - The domain for all evaluator functions is the set of all integers that fit
 *   in an evaluator_value; ie, an evaluator will never "fail".
 *
 * An evaluator function is relised as a sequence of function calls each taking
 * up to 3 values and returning one value. These functions are executed
 * in-order and may depend upon the values of prior functions. Inputs to the
 * evaluator function are implemented by pre-populating the result field before
 * evaluation, and using a function that simply returns the already-present
 * result.
 *
 * Functions which add cells through an evaluator_builder return the index of
 * the cell they define. Those which take runtime values as arguments take the
 * index of those cells. Eg, to calculate 2+3 at runtime, one would write
 *
 *   unsigned five_ix = evaluator_add(&builder,
 *                                    evaluator_const(&builder, 2),
 *                                    evaluator_const(&builder, 3));
 *
 * Which could be later accessed similar to
 *
 *   evaluator_execute(values, cells, 3);
 *   evaluator_value five = values[five_ix];
 */

typedef struct evaluator_cell_s evaluator_cell;
/**
 * The single type used by evaluators.
 */
typedef signed long long evaluator_value;
/**
 * A component function of an evaluator.
 *
 * For direct evaluators, only the first argument has a defined meaning. For
 * indirect evaluators, all arguments are defined, provided the indirect
 * indices point to elements in the result vector which hold defined values.
 */
typedef evaluator_value (*evaluator_f)(evaluator_value, evaluator_value,
                                       evaluator_value);
/**
 * Indicates which field of evaluator_cell::value is to be used.
 */
typedef enum {
  ecf_direct,
  ecf_indirect
} evaluator_cell_format;

/**
 * Describes how to produce the result for a single evaluation cell.
 */
struct evaluator_cell_s {
  /**
   * The C function call.
   */
  evaluator_f f;
  /**
   * How to generate the input arguments to f.
   */
  evaluator_cell_format format;

  union {
    /**
     * In the direct format, this exact value is passed into f as the first
     * argument; the other two arguments are undefined.
     */
    evaluator_value direct;
    /**
     * In the indirect format, values are read from the result array at indices
     * specified by each element of this array.
     */
    unsigned short indirect[3];
  } value;
};

/**
 * Executes the given evaluator function.
 *
 * @param dst Storage space for the n output values. Entries need only be
 * initialised if they will be read before they are written.
 * @param eval The cells to evaluate.
 * @param n The length of the dst and eval arrays.
 */
void evaluator_execute(evaluator_value* dst, const evaluator_cell* eval,
                       unsigned n);

/**
 * Stores temporary state used to build an evaluator.
 *
 * This should be treated as an opaque struct.
 */
typedef struct {
  unsigned ix, max;
  evaluator_cell* cells;
} evaluator_builder;

/**
 * Initialises the given evaluator_builder.
 *
 * @param cells The cells to which to write.
 * @param n The maximum number of cells to write.
 */
void evaluator_builder_init(evaluator_builder*,
                            evaluator_cell* cells,
                            unsigned n);

/**
 * Returns whether the given evaluator builder can have more elements added to
 * it.
 *
 * If false, attempting to add any more elements to the builder has undefined
 * effect.
 */
static inline int evaluator_builder_is_full(const evaluator_builder* b) {
  return b->ix >= b->max;
}

/**
 * Returns the number of cells consumed by the given builder.
 */
static inline unsigned evaluator_builder_n(const evaluator_builder* b) {
  return b->ix;
}

/**
 * Creates a cell which simply returns the given constant.
 */
unsigned evaluator_const(evaluator_builder*, evaluator_value);
/**
 * Creates a cell which returns the sum of two other cells.
 */
unsigned evaluator_add(evaluator_builder*, unsigned, unsigned);
/**
 * Creates a cell which returns the difference of two other cells.
 */
unsigned evaluator_sub(evaluator_builder*, unsigned, unsigned);
/**
 * Creates a cell which returns the product of two other cells.
 */
unsigned evaluator_mul(evaluator_builder*, unsigned, unsigned);
/**
 * Creates a cell which returns the quotient of two other cells.
 *
 * Division by 0 or -1 is interpreted as multiplication instead.
 */
unsigned evaluator_div(evaluator_builder*, unsigned, unsigned);
/**
 * Creates a cell which returns the unsigned modulus of two other cells.
 *
 * If the denominator is non-positive, the result is zero.
 */
unsigned evaluator_mod(evaluator_builder*, unsigned, unsigned);
/**
 * Creates a cell which returns the negation of another cell.
 *
 * Negating the most negative value has no effect.
 */
unsigned evaluator_neg(evaluator_builder*, unsigned);
/**
 * Creates a cell which returns the absolute value of another cell.
 *
 * The absolute value of the most negative value is the most negative value.
 */
unsigned evaluator_abs(evaluator_builder*, unsigned);
/**
 * Creates a cell which interprets another cell as an angle, and returns the
 * normalised angle value.
 */
unsigned evaluator_to_angle(evaluator_builder*, unsigned);
/**
 * Creates a cell which interprets another cell as an angle, and returns the
 * cosine of that angle (as a 14-bit signed fraction).
 */
unsigned evaluator_cos(evaluator_builder*, unsigned);
/**
 * Creates a cell which interprets another cell as an angle, and returns the
 * sine of that angle (as a 14-bit signed fraction).
 */
unsigned evaluator_sin(evaluator_builder*, unsigned);
/**
 * Creates a cell which produces the square root of another cell.
 *
 * The square root of negative values the negative of the square root of that
 * value's absolute value.
 */
unsigned evaluator_sqrt(evaluator_builder*, unsigned);
/**
 * Creates a cell which produces the magnitude of the vector conposed of three
 * other cells.
 */
unsigned evaluator_magnitude(evaluator_builder*, unsigned, unsigned, unsigned);
/**
 * Creates a cell which produces the first input if it is zero, and the second
 * input otherwise.
 */
unsigned evaluator_logand(evaluator_builder*, unsigned, unsigned);
/**
 * Creates a cell which produces the first input if it is non-zero, and the
 * second input otherwise.
 */
unsigned evaluator_logor(evaluator_builder*, unsigned, unsigned);
/**
 * Creates a cell which produces 1 iff the input cell is 0, and 0 otherwise.
 */
unsigned evaluator_lognot(evaluator_builder*, unsigned);
/**
 * Creates a cell which produces 1 iff the two inputs are equal.
 */
unsigned evaluator_equ(evaluator_builder*, unsigned, unsigned);
/**
 * Creates a cell which produces 1 iff the two inputs are non-equal.
 */
unsigned evaluator_neq(evaluator_builder*, unsigned, unsigned);
/**
 * Creates a cell which produces 1 iff the first input is less than the second.
 */
unsigned evaluator_lt(evaluator_builder*, unsigned, unsigned);
/**
 * Creates a cell which produces 1 iff the first input is less than or equal to
 * the second.
 */
unsigned evaluator_le(evaluator_builder*, unsigned, unsigned);
/**
 * Creates a cell which produces 1 iff the first input is greater than the
 * second.
 */
unsigned evaluator_gt(evaluator_builder*, unsigned, unsigned);
/**
 * Creates a cell which produces 1 iff the first input is greater than or equal
 * to the second.
 */
unsigned evaluator_ge(evaluator_builder*, unsigned, unsigned);
/**
 * Creates a cell which produces its second input if the first input is
 * non-zero, and the third input otherwise.
 */
unsigned evaluator_if(evaluator_builder*, unsigned, unsigned, unsigned);
/**
 * Creates a cell which returns the third input if it is greater than the first
 * and less than the second. If it is less than or equal to the first, it
 * returns the first; if it is greater than or equal to the second, it returns
 * the second.
 */
unsigned evaluator_clamp(evaluator_builder*, unsigned min, unsigned max,
                         unsigned val);
/**
 * Creates a cell which returns the greater of its two inputs.
 */
unsigned evaluator_clamp_min(evaluator_builder*, unsigned min, unsigned val);
/**
 * Creates a cell which returns the lesser of its two inputs.
 */
unsigned evaluator_clamp_max(evaluator_builder*, unsigned max, unsigned val);
/**
 * Creates a cell which returns a 31-bit unsigned fraction obtained by dividing
 * 1 by the input cell.
 *
 * Inputs <= 0 are treated as if they were 1.
 */
unsigned evaluator_fraction_of(evaluator_builder*, unsigned);
/**
 * Creates a cell which interprets its first input as a 32-bit signed value and
 * the second as a 31-bit fraction, and produces the result of multiplying
 * them.
 */
unsigned evaluator_fraction_smul(evaluator_builder*, unsigned, unsigned);
/**
 * Creates a cell which interprets its first input as a 32-bit unsigned value
 * and the second as a 31-bit fraction, and produces the result of multiplying
 * them.
 */
unsigned evaluator_fraction_umul(evaluator_builder*, unsigned, unsigned);
/**
 * Creates a cell which interprets its first input as a 32-bit signed value and
 * the second as 14-bit signed fraction, and produces the ruslt of multiplying
 * them.
 */
unsigned evaluator_zoscale(evaluator_builder*, unsigned, unsigned);
/**
 * Creates a cell which interprets each argument as a 32-bit integer, and
 * produces an effectively-random unsigned 32-bit output derived purely from
 * the three inputs.
 */
unsigned evaluator_chaos(evaluator_builder*, unsigned, unsigned, unsigned);
/**
 * Creates a cell which returns its own value.
 *
 * This only makes sense if the caller knows that something else will assign
 * that value before the evaluator is executed.
 */
unsigned evaluator_nop(evaluator_builder*);

#endif /* MATH_EVALUATOR_H_ */
