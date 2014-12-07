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
#ifndef MATH_RAND_H_
#define MATH_RAND_H_

/**
 * @file
 * This file contains a number of functions for generating pseudo-random
 * numbers. Having our own implementation is important, as we need to ensure
 * consistency across platforms (for random in-game effects) and time (for
 * graphics as well), as well as isolation between concurrent generation of
 * such numbers.
 */

#ifdef PROFILE
static unsigned short lcgrand(unsigned* state)
__attribute__((noinline,unused));
static unsigned chaos_accum(unsigned, unsigned)
__attribute__((noinline,unused));
static unsigned chaos_of(unsigned)
__attribute__((noinline,unused));
#endif

/**
 * Linear Congruential Generator. State is a 32-bit unsigned integer. Each call
 * to this produces a 16-bit random integer and updates the state in-place.
 *
 * This is the preferred generator when large sequences of numbers are needed,
 * as it is essentially free to initialise, has very little overhead, and does
 * not require a significant amount of storage space.
 */
static
#ifndef PROFILE
inline
#endif
unsigned short lcgrand(unsigned* state) {
  /* Same LCG as glibc, probably a good choice */
  *state = (*state) * 1103515245 + 12345;
  return (*state) >> 16;
}

/**
 * Accumulates "chaos" into a 32-bit value. Chaos allows to produce
 * high-quality (to an observing human) randomness from non-random data, such
 * as spatial coordinates, while being consistent based on the input data. (In
 * other words, it's a hash function.) If a sequence of random numbers are
 * needed, this can be used to generate a seed for lcgrand(), which from then
 * on will be faster than using the pair of chaos functions.
 *
 * Each call to this function returns an updated accumulator, which is to be
 * passed into the next such call. The initial accumulator should generally be
 * zero. Once all inputs have been given to this function, chaos_of() should be
 * called on the final accumulator to obtain the final value.
 *
 * This currently implements the Jenkins hash function.
 */
static
#ifndef PROFILE
inline
#endif
unsigned chaos_accum(unsigned accum, unsigned chaos) {
  accum += chaos & 0xFF;
  accum += accum << 10;
  accum ^= accum >> 6;
  chaos >>= 8;
  accum += chaos & 0xFF;
  accum += accum << 10;
  accum ^= accum >> 6;
  chaos >>= 8;
  accum += chaos & 0xFF;
  accum += accum << 10;
  accum ^= accum >> 6;
  chaos >>= 8;
  accum += chaos & 0xFF;
  accum += accum << 10;
  accum ^= accum >> 6;
  return accum;
}

/**
 * Performs some final manipulations on an accumulator from chaos_accum() to
 * produce better-mixed values.
 */
static
#ifndef PROFILE
inline
#endif
unsigned chaos_of(unsigned accum) {
  accum += accum << 3;
  accum ^= accum >> 11;
  accum += accum << 15;
  return accum;
}

/**
 * Represents the state for the Mersenne Twister algorithm. The MT is generally
 * better as a generator than lcgrand() above, but it requires far more storage
 * space and initialisation time than lcgrand(). It should be preferred
 * whenever these two factors are not a concern.
 *
 * The actual contents of this struct should not be accessed outside of
 * rand.c. The struct is defined here so that it can have immediate storage and
 * be copied.
 */
typedef struct {
  unsigned state[624];
  unsigned ix;
} mersenne_twister;

/**
 * Initialises and seeds a Mersenne Twister with the given seed. After this
 * call, twist() may be called on the object indefinitely.
 *
 * This is a rather expensive call. In many cases, using lcgrand() directly may
 * be preferable due to this cost.
 */
void twister_seed(mersenne_twister*, unsigned);
/**
 * Returns the next 32-bit integer to be produced by the given Mersenne
 * Twister.
 */
unsigned twist(mersenne_twister*);

/**
 * Generates and adds (ie, sums) perlin noise to the given two-dimensional
 * array of integers. The generated noise is tilable.
 *
 * This call will distribute work across uMP workers.
 *
 * @param dst Input/output array to which noise is to be added
 * @param w The width of dst.
 * @param h The height of dst.
 * @param freq The number of points along each dimension for the perlin noise
 * grid. Must be at least 2.
 * @param amp The amplitude of the noise. Values added to dst range from 0,
 * inclusive, to amp, exclusive.
 * @param seed Random number generation seed.
 */
void perlin_noise(unsigned* dst, unsigned w, unsigned h,
                  unsigned freq, unsigned amp,
                  unsigned seed);

#endif /* MATH_RAND_H_ */
