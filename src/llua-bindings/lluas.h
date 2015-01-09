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
#ifndef LLUA_BINDINGS_LLUAS_H_
#define LLUA_BINDINGS_LLUAS_H_

/**
 * @file
 *
 * Provides simlified access to a Llua interpreter.
 *
 * The interface here is very simplistic. It takes a stance of "If given the
 * choice of crashing and doing something nonsensical, do something
 * nonsensical". Ie, no matter what error occurs (other than unexpected memory
 * exhaustion), all execution will carry on normally. Since the llua
 * modifications ensure total consistency (TODO), any errors will occur
 * uniformly on all hosts. This choice is mostly motivated by the fact that
 * there may be multiple non-cooperating scripts living together; a fault in
 * one shouldn't ruin everything.
 *
 * Nonetheless, if an error _does_ occur, the state as a whole is marked as
 * tainted, so that the user can be notified that something is awry.
 * Additionally, diagnostics are printed to stderr.
 *
 * Note that memory allocation errors (which may happen due to the strict
 * control placed on memory allocation) are *not* consistent, since they depend
 * strongly on the host architecture, ABI, and malloc implementation.
 * Therefore, a memory error is a fatal condition that prevents further
 * execution of scripts in the interpreters.
 *
 * TODO:
 * - Proper sandboxing
 * - Interpreter "freezing"
 * - Stop llua from leaking information about memory layout (for consistency)
 * - Remove support for __gc metatables (?)
 */

/**
 * Describes the error status of the llua interpreters as a whole.
 */
typedef enum {
  /**
   * All interpreters are OK and have not encountered any issues.
   */
  les_ok = 0,
  /**
   * A script error has been encountered in at least one interpreter, but it is
   * expected to be consistent across systems, so execution can continue.
   */
  les_problematic,
  /**
   * A script or runtime error has been encountered in at least one
   * interpreter, and it is not expected to be consistent across hosts. Further
   * execution within the current interpreters is not meaningful.
   */
  les_fatal
} lluas_error_status;

/**
 * Initialises a fresh llua interpreter. This frees any resources held by prior
 * incarnations, and additionally clears the error status.
 */
void lluas_init(void);

/**
 * Returns the current error status of the llua interpreter.
 */
lluas_error_status lluas_get_error_status(void);

/**
 * Loads and executes the given llua file in the interpreter.
 *
 * Only text files are accepted (no bytecode).
 */
void lluas_load_file(const char* filename, unsigned instr_limit);

/**
 * Invokes the global function of the given name in the interpreter, with no
 * arguments.
 */
void lluas_invoke_global(const char* function, unsigned instr_limit);

#endif /* LLUA_BINDINGS_LLUAS_H_ */
