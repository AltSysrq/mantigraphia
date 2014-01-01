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
#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include <SDL.h>

#include "bsd.h"

#include "micromp.h"

static unsigned num_workers;
static SDL_atomic_t num_busy_workers;
static SDL_cond* completion_notification, * assignment_notification;
static SDL_mutex* completion_mutex, * assignment_mutex;
static ump_task* current_task;
static SDL_atomic_t current_task_id;
static int current_task_is_sync;

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
  ump_thread_spec spec;
  unsigned work_amt, work_offset;
  unsigned lower_bound, upper_bound, n;
  int prev_task = 0;

  memcpy(&spec, vspec, sizeof(spec));

  while (1) {
    /* Wait for assignment */
    if (SDL_LockMutex(assignment_mutex))
      errx(EX_SOFTWARE, "Unable to lock mutex: %s", SDL_GetError());

    while (prev_task == SDL_AtomicGet(&current_task_id)) {
      if (SDL_CondWait(assignment_notification, assignment_mutex))
        errx(EX_SOFTWARE, "Unable to wait on condition: %s", SDL_GetError());
    }

    SDL_AtomicAdd(&num_busy_workers, +1);
    prev_task = SDL_AtomicGet(&current_task_id);

    if (SDL_UnlockMutex(assignment_mutex))
      errx(EX_SOFTWARE, "Unable to unlock mutex: %s", SDL_GetError());

    /* Calculate work boundaries */
    n = current_task->num_divisions;
    if (current_task_is_sync)
      /* Master gets a fair share of work */
      work_offset = n / (1 + spec.count);
    else
      /* Master gets specific unfair amount of work */
      work_offset = current_task->divisions_for_master;

    work_amt = n - work_offset;

    lower_bound = work_offset + spec.ordinal * work_amt / spec.count;
    upper_bound = work_offset + (spec.ordinal+1) * work_amt / spec.count;

    exec_region(lower_bound, upper_bound);

    /* Done. Notify of completion and go back to waiting for assignment */
    if (SDL_LockMutex(completion_mutex))
      errx(EX_SOFTWARE, "Unable to lock mutex: %s", SDL_GetError());

    SDL_AtomicAdd(&num_busy_workers, -1);
    if (SDL_CondBroadcast(completion_notification))
      errx(EX_SOFTWARE, "Unable to broadcast completion notification: %s",
           SDL_GetError());

    if (SDL_UnlockMutex(completion_mutex))
      errx(EX_SOFTWARE, "Unable to unlock mutex: %s", SDL_GetError());
  }
}

void ump_init(unsigned num_threads) {
  unsigned i;
  char thread_name[32];

  num_workers = num_threads;

  if (!(completion_notification = SDL_CreateCond()))
    errx(EX_SOFTWARE, "Unable to create completion cond: %s", SDL_GetError());

  if (!(completion_mutex = SDL_CreateMutex()))
    errx(EX_SOFTWARE, "Unable to create completion mutex: %s", SDL_GetError());

  if (!(assignment_notification = SDL_CreateCond()))
    errx(EX_SOFTWARE, "Unable to create assignment cond: %s", SDL_GetError());

  if (!(assignment_mutex = SDL_CreateMutex()))
    errx(EX_SOFTWARE, "Unable to create assignment mutex: %s", SDL_GetError());

  for (i = 0; i < num_threads; ++i) {
    thread_specs[i].ordinal = i;
    thread_specs[i].count = num_threads;
    snprintf(thread_name, sizeof(thread_name), "ump-%d", i);
    if (!SDL_CreateThread(ump_main, thread_name, thread_specs+i))
      errx(EX_SOFTWARE, "Unable to create uMP worker %d: %s", i, SDL_GetError());
  }
}

static void ump_run(ump_task* task, int sync) {
  if (SDL_LockMutex(assignment_mutex))
    errx(EX_SOFTWARE, "Unable to lock mutex: %s", SDL_GetError());

  current_task_is_sync = sync;
  current_task = task;
  SDL_AtomicAdd(&current_task_id, +1);

  if (SDL_CondBroadcast(assignment_notification))
    errx(EX_SOFTWARE, "Unable to broadcast assignment notification: %s",
         SDL_GetError());

  if (SDL_UnlockMutex(assignment_mutex))
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
}


void ump_run_sync(ump_task* task) {
  ump_run(task, 1);
}

void ump_run_async(ump_task* task) {
  ump_run(task, 0);
}

void ump_join(void) {
  int done_early = !SDL_AtomicGet(&num_busy_workers);

  if (!done_early) {
    /* Need to wait for others */
    if (SDL_LockMutex(completion_mutex))
      errx(EX_SOFTWARE, "Unable to lock mutex: %s", SDL_GetError());

    while (SDL_AtomicGet(&num_busy_workers)) {
      if (SDL_CondWait(completion_notification, completion_mutex))
        errx(EX_SOFTWARE, "Unable to wait on completion cond: %s",
             SDL_GetError());
    }

    if (SDL_UnlockMutex(completion_mutex))
      errx(EX_SOFTWARE, "Unable to unlock mutex: %s", SDL_GetError());
  }

  /* If this was async, adjust distribution for master */
  if (!current_task_is_sync) {
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
}
