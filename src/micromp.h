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
#ifndef MICROMP_H_
#define MICROMP_H_

/**
 * @file
 *
 * This file defines the interface for "microMP" (ump), which we use as a
 * replacement for OpenMP. The interface obviously isn't as nice as OpenMP, but
 * there's a number of reasons not to use the latter:
 *
 * - This is more portable. We don't need a compiler that supports OMP.
 *
 * - We're not at the mercy of whatever OMP implementation hapens to live with
 *   us; we can rely on particular performance characteristics.
 *
 * - Even good OMP implementations like GNU's are built more with throughput
 *   than latency in mind. For example, on FreeBSD, delays of up to 250ms are
 *   occasionally seen, which rather detracts from the graphical experience.
 *
 * - OMP isn't hugely efficient with micro-tasks we usually get when
 *   parallelising graphics code with simple OMP directives; making larger
 *   tasks would generally require surrounding too much code with parallel
 *   sections, and peppering everything with master-only sections.
 */

/**
 * The size of the cache lines on the host system. It is important for
 * performance that this is not underestimated, as this may cause false sharing
 * between the workers.
 */
#define UMP_CACHE_LINE_SZ 64
/**
 * The maximum supported number of threads.
 */
#define UMP_MAX_THREADS 64

/**
 * Describes the parameters for executing uMP tasks. This struct may be
 * modified at run-time by uMP.
 */
typedef struct {
  /**
   * The function to execute on each worker. It is up to the calling code to
   * arrange some means of communicating parameters to the workers.
   *
   * @param ix The index of the division the function is to perform.
   * @param n The total number of divisions the into which the work is divided
   */
  void (*exec)(unsigned ix, unsigned n);
  /**
   * The maximum number of divisions into which to split any given task.
   */
  unsigned num_divisions;
  /**
   * For async tasks, indicates the number of divisions that are assigned to
   * the master thread, which it performs before the async call returns. The
   * initial value is a hint to the scheduler. The scheduler will adjust it at
   * run-time so as to minimise the time between the work actually being
   * completed and ump_join() being called.
   *
   * This value has no effect for sync calls.
   */
  unsigned divisions_for_master;
} ump_task;

/**
 * Starts the given number of background worker threads and otherwise
 * initialises uMP. If anything goes wrong, the program exits.
 */
void ump_init(unsigned num_threads);
/**
 * Executes the given task synchronously. The calling thread will perform a
 * share of work equal to the other workers. When this call returns, the task
 * is guaranteed to be entirely done.
 */
void ump_run_sync(ump_task*);
/**
 * Executes the given task semi-asynchronously. The calling thread will usually
 * perform some work on the task itself before this call returns, though that
 * amount is always less than the other workers.
 *
 * After this call, no other ump_* functions may be called other than
 * ump_join() or any functions that merely query state.
 */
void ump_run_async(ump_task*);
/**
 * Blocks the calling thread until the current task, if any, has completed.
 */
void ump_join(void);
/**
 * Checks whether the most recently assigned task is still in-progress. If it
 * is, returns 0. If it has completed, returns 1, and it is safe to call other
 * ump_ functions again.
 */
int ump_is_finished(void);
/**
 * Returns the number of background worker threads are running in uMP.
 */
unsigned ump_num_workers(void);

#endif /* MICROMP_H_ */
