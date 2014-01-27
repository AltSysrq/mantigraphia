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
#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include <SDL.h>

#if defined(HAVE_SYS_PARAM_H) && defined(HAVE_SYS_CPUSET_H) &&  \
  defined(HAVE_CPUSET_SETAFFINITY)
#define USE_BSD_CPUSET_SETAFFINITY
#endif

#ifdef USE_BSD_CPUSET_SETAFFINITY
#include <sys/param.h>
#include <sys/cpuset.h>
#endif

#include "bsd.h"

#include "micromp.h"

/* Define UMP_NO_THREADING to do everything single-threaded. */

static unsigned num_workers;
static SDL_atomic_t num_busy_workers;
static SDL_cond* completion_notification, * assignment_notification;
static SDL_mutex* mutex;
static ump_task* current_task;
static SDL_atomic_t current_task_id;
static int current_task_is_sync;

/**
 * Indicates the most recent task ID accepted by each thread. This is used by
 * the "impersonation mechanism" which allows to recover from operating system
 * scheduler delays.
 *
 * On systems with strict interpretation of priorities, in particular FreeBSD,
 * Mantigraphia often ends up with a relatively low priority, allowing other
 * processes to steal the CPU from us for hundreds of milliseconds at a
 * time. This almost (but not always, unfortunately) manifests itself as one or
 * more threads failing to wake up from the assignment sleep for that duration.
 *
 * The impersonation mechanism works as follows. When any thread completes the
 * work that was assigned to it and has decremented num_busy_workers, it
 * attempts a CAS for each element in this array, to change it from the
 * previous task ID to the current. If one succeeds, the thread pretends to be
 * the one whose slot it just overwrote, performing the work on its behalf,
 * including decrementing num_busy_workers. Thereafter, it attempts to
 * impersonate another thread. Once no more threads can be impersonated, the
 * thread signals the completion notification and begins waiting for more
 * work.
 */
static SDL_atomic_t accepted_task_ids[UMP_MAX_THREADS];

typedef struct {
  unsigned ordinal;
  unsigned count;
} ump_thread_spec;

static ump_thread_spec thread_specs[UMP_MAX_THREADS];

static void exec_region(unsigned lower_bound, unsigned upper_bound) {
  unsigned i, n;
  void (*exec)(unsigned,unsigned);

  n = current_task->num_divisions;
  exec = current_task->exec;

  for (i = lower_bound; i < upper_bound; ++i)
    (*exec)(i, n);
}

static int ump_main(void* vspec) {
#ifdef USE_BSD_CPUSET_SETAFFINITY
  cpuset_t affinity;
#endif
  ump_thread_spec spec;
  unsigned effective_id;
  unsigned work_amt, work_offset;
  unsigned lower_bound, upper_bound, n;
  int prev_task = 0;
#ifdef UMP_VERBOSE_TIMING
  unsigned received, completed;
#endif

  memcpy(&spec, vspec, sizeof(spec));

#ifdef USE_BSD_CPUSET_SETAFFINITY
  /* Pin self to CPU n+1 */
  CPU_ZERO(&affinity);
  CPU_SET(spec.ordinal+1, &affinity);
  if (cpuset_setaffinity(CPU_LEVEL_WHICH, CPU_WHICH_TID, -1 /* self */,
                         sizeof(affinity), &affinity))
    warn("Unable to set affinity for uMP thread %d", spec.ordinal);
#endif

  if (SDL_LockMutex(mutex))
    errx(EX_SOFTWARE, "Unable to lock mutex: %s", SDL_GetError());

  while (1) {
    /* Mutex locked at this point */

    /* Cease impersonation */
    effective_id = spec.ordinal;

    /* Wait for assignment */
    while (prev_task == SDL_AtomicGet(&current_task_id)) {
      if (SDL_CondWait(assignment_notification, mutex))
        errx(EX_SOFTWARE, "Unable to wait on condition: %s", SDL_GetError());
    }

    prev_task = SDL_AtomicGet(&current_task_id);

    /* Try to take ownership of this task.
     * This needs to be within the mutex lock, since it can jump back to the
     * beginning of the loop. But we can't assume that the mutex protects our
     * accepted_task_ids slot, since impersonators will write to it without
     * holding the mutex.
     */
    if (!SDL_AtomicCAS(accepted_task_ids+spec.ordinal, prev_task-1, prev_task))
      /* Somebody has impersonated us meanwhile */
      continue;

    if (SDL_UnlockMutex(mutex))
      errx(EX_SOFTWARE, "Unable to unlock mutex: %s", SDL_GetError());

    process_task:

#ifdef UMP_VERBOSE_TIMING
    received = SDL_GetTicks();
    printf("uMP thread %d: Got task at %d\n", spec.ordinal, received);
#endif

    /* Calculate work boundaries */
    n = current_task->num_divisions;
    if (current_task_is_sync)
      /* Master gets a fair share of work */
      work_offset = n / (1 + spec.count);
    else
      /* Master gets specific unfair amount of work */
      work_offset = current_task->divisions_for_master;

    work_amt = n - work_offset;

    lower_bound = work_offset + effective_id * work_amt / spec.count;
    upper_bound = work_offset + (effective_id+1) * work_amt / spec.count;

    exec_region(lower_bound, upper_bound);

#ifdef UMP_VERBOSE_TIMING
    completed = SDL_GetTicks();
    printf("uMP thread %d: Task completed at %d (delta %d)\n",
           spec.ordinal, completed, completed - received);
#endif

    SDL_AtomicAdd(&num_busy_workers, -1);

    /* See if any other threads need be impersonated */
    for (effective_id = 0; effective_id < num_workers; ++effective_id)
      if (SDL_AtomicCAS(accepted_task_ids+effective_id, prev_task-1, prev_task))
        /* Ownership taken. Impersonate this thread. */
        goto process_task;

    /* Done. Notify of completion and go back to waiting for assignment */
    if (SDL_LockMutex(mutex))
      errx(EX_SOFTWARE, "Unable to lock mutex: %s", SDL_GetError());

    if (SDL_CondBroadcast(completion_notification))
      errx(EX_SOFTWARE, "Unable to broadcast completion notification: %s",
           SDL_GetError());

    /* Leave mutex locked as loop entrance needs it */
  }
}

void ump_init(unsigned num_threads) {
  unsigned i;
  char thread_name[32];
#ifdef USE_BSD_CPUSET_SETAFFINITY
  cpuset_t affinity;
#endif

  num_workers = num_threads;

#ifdef UMP_NO_THREADING
  return;
#endif

#ifdef USE_BSD_CPUSET_SETAFFINITY
  /* Pin main thread to CPU 0 */
  CPU_ZERO(&affinity);
  CPU_SET(0, &affinity);
  if (cpuset_setaffinity(CPU_LEVEL_WHICH, CPU_WHICH_TID, -1,
                         sizeof(affinity), &affinity))
    warn("Unable to set CPU affinity of main thread");
#endif

  if (!(completion_notification = SDL_CreateCond()))
    errx(EX_SOFTWARE, "Unable to create completion cond: %s", SDL_GetError());

  if (!(mutex = SDL_CreateMutex()))
    errx(EX_SOFTWARE, "Unable to create mutex: %s", SDL_GetError());

  if (!(assignment_notification = SDL_CreateCond()))
    errx(EX_SOFTWARE, "Unable to create assignment cond: %s", SDL_GetError());

  for (i = 0; i < num_threads; ++i) {
    thread_specs[i].ordinal = i;
    thread_specs[i].count = num_threads;
    snprintf(thread_name, sizeof(thread_name), "ump-%d", i);
    if (!SDL_CreateThread(ump_main, thread_name, thread_specs+i))
      errx(EX_SOFTWARE, "Unable to create uMP worker %d: %s", i, SDL_GetError());
  }
}

static void ump_run(ump_task* task, int sync) {
  ump_join();

#ifdef UMP_NO_THREADING
  current_task_is_sync = 1;
  current_task = task;
  exec_region(0, task->num_divisions);
#else
  if (SDL_LockMutex(mutex))
    errx(EX_SOFTWARE, "Unable to lock mutex: %s", SDL_GetError());

  current_task_is_sync = sync;
  current_task = task;
  SDL_AtomicAdd(&num_busy_workers, +num_workers);
  SDL_AtomicAdd(&current_task_id, +1);

  if (SDL_CondBroadcast(assignment_notification))
    errx(EX_SOFTWARE, "Unable to broadcast assignment notification: %s",
         SDL_GetError());

  if (SDL_UnlockMutex(mutex))
    errx(EX_SOFTWARE, "Unable to unlock mutex: %s", SDL_GetError());

  if (sync) {
    /* Execute the master's fair share */
    exec_region(0, task->num_divisions / (num_workers+1));
    /* Wait for others */
    ump_join();
  } else {
    /* Execute requested amount */
    exec_region(0, task->divisions_for_master);
    /* Don't wait, return now */
  }
#endif
}


void ump_run_sync(ump_task* task) {
  ump_run(task, 1);
}

void ump_run_async(ump_task* task) {
  ump_run(task, 0);
}

void ump_join(void) {
#ifdef UMP_NO_THREADING
  return;
#else
  int done_early = !SDL_AtomicGet(&num_busy_workers);

  if (!done_early) {
    /* Need to wait for others */
    if (SDL_LockMutex(mutex))
      errx(EX_SOFTWARE, "Unable to lock mutex: %s", SDL_GetError());

    while (SDL_AtomicGet(&num_busy_workers)) {
      if (SDL_CondWait(completion_notification, mutex))
        errx(EX_SOFTWARE, "Unable to wait on completion cond: %s",
             SDL_GetError());
    }

    if (SDL_UnlockMutex(mutex))
      errx(EX_SOFTWARE, "Unable to unlock mutex: %s", SDL_GetError());
  }

  /* If this was async, adjust distribution for master */
  if (!current_task_is_sync && current_task) {
    if (done_early) {
      /* The master got to join() only after all the other work was done. Try
       * to give less work to the master so it can join earlier.
       */
      if (current_task->divisions_for_master > 0) {
        --current_task->divisions_for_master;
      }
    } else {
      /* The master got to join() before the other work was done. Try to give
       * more work to the master so it can be productive instead of idling for
       * the background workers to finish.
       *
       * This does favour weighting the master more heavily in some
       * circumstances. This is preferable, as there is less overhead for the
       * master to start doing work than for the other threads to do so.
       */
      if (current_task->divisions_for_master < current_task->num_divisions) {
        ++current_task->divisions_for_master;
      }
    }
  }

  /* Unset the task so we don't readjust if another join() is called */
  current_task = NULL;
#endif /* UMP_NO_THREADING */
}

int ump_is_finished(void) {
#ifdef UMP_NO_THREADING
  return 1;
#else
  return !SDL_AtomicGet(&num_busy_workers);
#endif
}

unsigned ump_num_workers(void) {
  return num_workers;
}

void* align_to_cache_line(void* vinput) {
  char* input = vinput;
  unsigned long long as_int;
  unsigned misalignment;

  as_int = (unsigned long long)input;
  misalignment = as_int & (UMP_CACHE_LINE_SZ - 1);
  if (misalignment)
    input += UMP_CACHE_LINE_SZ - misalignment;

  return input;
}
