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
#ifndef RENDER_LSYSTEM_H_
#define RENDER_LSYSTEM_H_

/**
 * The maximum buffer size for an lsystem. This includes the terminating NUL
 * byte.
 */
#define LSYSTEM_MAX_SZ 4096
/**
 * Maximum number of replacements for a single rule.
 */
#define LSYSTEM_MAX_REPLS 8

/**
 * An L-System is an iterated string-replacement system which can produce
 * realistic-enough looking vegitation and such. Here, we implement a
 * stochastic, context-free L-System. The alphabet is the 7-bit ASCII character
 * set. On each step, each symbol is replaced with one of LSYSTEM_MAX_REPLS
 * possible replacements, chosen randomly.
 *
 * The contents of this structure should be considered opaque to client
 * code. It is included in the header so that lsystems may be stored in static
 * space.
 */
typedef struct {
  struct {
    const char* replacement[LSYSTEM_MAX_REPLS];
    unsigned replacement_size[LSYSTEM_MAX_REPLS];
  } rules[128];
} lsystem;

/**
 * Compiles a set of rules into an lsystem. Each rule has the form
 *
 *   <character> <space> <replacement...> [<replacement...>]
 *
 * Each character may only be given a rule once. If fewer than
 * LSYSTEM_MAX_REPLS replacements are given, the given replacements are cycled
 * to populate the rest of the array. Characters not assigned rules simply
 * expand to themselves.
 *
 * The rule list must be NULL-terminated.
 *
 * For example, the "Fractal Plant" example on the Wikipedia page for L-Systems
 * could be expressed as
 *
 *   lsystem_compile(&system,
 *                   "x f-[[x]+x]+f[+fx]-x",
 *                   "f ff",
 *                   NULL);
 */
void lsystem_compile(lsystem*, ...)
#if defined(__GNUC__) || defined(__clang__)
__attribute__((sentinel))
#endif
;

/**
 * An lsystem_state serves both as temporary storage for the evolution of an
 * lsystem, and to hold the final result when evolution completes.
 */
typedef struct {
  /**
   * After lsystem_execute(), contains the final NUL-terminated result of
   * evaluating the L-System.
   */
  char buffer[LSYSTEM_MAX_SZ];
  /**
   * Temporary storage for lsystem_execute(). Contents undefined.
   */
  char temp[LSYSTEM_MAX_SZ];
} lsystem_state;

/**
 * Executes an lsystem, writing the final state into the given
 * lsystem_state. initial_state is used for step-0 of evaluation. steps
 * specifies the maximum number of steps to take; 0 indicates that the
 * initial_state is also the result. The system may take fewer steps if the
 * buffers would overflow otherwise. random specifies the initial random seed
 * used to randomise evaluation.
 */
void lsystem_execute(lsystem_state*,
                     const lsystem*,
                     const char* initial_state,
                     unsigned steps,
                     unsigned random);

#endif /* RENDER_LSYSTEM_H_ */
