/*-
 * Copyright (c) 2015 Jason Lingle
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
#include <string.h>
#include <assert.h>

#include "../micromp.h"
#include "../math/coords.h"
#include "env-vmap.h"
#include "vmap-painter.h"

/* To permit painting to run asynchronously of the main thread, there are two
 * sets of work queues. At any given time, one is the "busy set", which is
 * read-only and possibly in use by uMP threads; the other is the "append set",
 * which is write-only and may be appended to by the main thread. Swapping
 * between these two occurs when one fills up, when a barrier is requested, or
 * when flushing the whole state. The basic procedure for this is
 * - Wait for uMP threads to be idle.
 * - Swap the busy and append sets.
 * - If work is to be done, start the uMP threads again.
 *
 * Parallelism is achieved on the basis that the vast majority of painting
 * operations are relatively small, and thus can be run in parallel if
 * bucketted appropriately.
 *
 * Thus, the world is partitioned into an 8x8 grid (approximately; this is
 * achieved via bitshifts, so non-power-of-two vmaps might not be partitioned
 * exactly this way). Each operation is added to a queue specific to the grid
 * cell in which it occurs; operations that stradle grid boundaries are split
 * into separate operations within each bucket.
 */

#define NUM_BUCKETS 8
#define QUEUE_SIZE 65536
typedef unsigned short vmap_painter_queue_index;

typedef struct {
  /**
   * Operations in this queue set. They are ordered per-bucket. Different
   * buckets are mixed together.
   *
   * The zeroth element is never used since index 0 is used as a sentinel.
   */
  vmap_paint_operation operations[QUEUE_SIZE];
  /**
   * For each operation, indicates the index of the next operation in the same
   * bucket, or 0 for the end of the list.
   */
  vmap_painter_queue_index index_list[QUEUE_SIZE];
  /**
   * The index of the first operation for each bucket, or 0 if the bucket is
   * empty.
   */
  vmap_painter_queue_index bucket_start[NUM_BUCKETS][NUM_BUCKETS];
  /**
   * The index of the last operation for each bucket, or undefined if the
   * bucket is empty.
   */
  vmap_painter_queue_index bucket_end[NUM_BUCKETS][NUM_BUCKETS];
  /**
   * The number of operations consumed in this queue set, including the unused
   * element at the head of operations.
   */
  unsigned num_operations;
} vmap_painter_queue_set;

static void vmap_painter_execute(unsigned, unsigned);
static void vmap_painter_init_queue_set(vmap_painter_queue_set*);
static void vmap_painter_swap_sets(void);
static void vmap_painter_start_busy(int sync);
static void vmap_painter_add_atomic(const vmap_paint_operation*);

static vmap_painter_queue_set queue_set_alpha, queue_set_beta;
static vmap_painter_queue_set
  *restrict append_set = &queue_set_alpha,
  *restrict busy_set = &queue_set_beta;

static env_vmap* vmap;
static unsigned char bucket_xshift, bucket_zshift;

static ump_task vmap_painter_task = {
  vmap_painter_execute,
  NUM_BUCKETS*NUM_BUCKETS,
  0
};

void vmap_painter_init(env_vmap* v) {
  assert(!vmap);
  assert(v);

  vmap = v;

  bucket_xshift = bucket_zshift = 0;
  while (((v->xmax - 1) >> bucket_xshift) >= NUM_BUCKETS)
    ++bucket_xshift;
  while (((v->zmax - 1) >> bucket_zshift) >= NUM_BUCKETS)
    ++bucket_zshift;

  vmap_painter_init_queue_set(append_set);
}

void vmap_painter_abort(void) {
  if (!vmap) return;

  vmap_painter_init_queue_set(append_set);
  ump_join();
  vmap = NULL;
}

void vmap_painter_flush(void) {
  if (!vmap) return;

  vmap_painter_swap_sets();
  vmap_painter_start_busy(1);
  ump_join();
  vmap = NULL;
}

void vmap_painter_barrier(void) {
  if (!vmap) return;

  vmap_painter_swap_sets();
}

void vmap_painter_add(const vmap_paint_operation* opp) {
  coord x0, x1, z0, z1, bx0, bx1, bz0, bz1;
  coord bx, bz, sx, sz, sw, sh;
  vmap_paint_operation op = *opp;

  if (!vmap) return;

  x0 = op.x;
  x1 = x0 + op.w;
  z0 = op.z;
  z1 = z0 + op.h;
  bx0 = x0 >> bucket_xshift;
  bx1 = (x1-1) >> bucket_xshift;
  bz0 = z0 >> bucket_zshift;
  bz1 = (z1-1) >> bucket_zshift;

  for (bz = bz0; bz <= bz1; ++bz) {
    for (bx = bx0; bx <= bx1; ++bx) {
      sx = umax(bx << bucket_xshift, x0);
      sz = umax(bz << bucket_zshift, z0);
      sw = umin((bx+1) << bucket_xshift, x1) - sx;
      sh = umin((bz+1) << bucket_zshift, z1) - sz;
      if (vmap->is_toroidal) {
        sx &= vmap->xmax - 1;
        sz &= vmap->zmax - 1;
      }

      op.x = sx;
      op.z = sz;
      op.w = sw;
      op.h = sh;
      vmap_painter_add_atomic(&op);
    }
  }
}

static void vmap_painter_add_atomic(const vmap_paint_operation* op) {
  unsigned bx, bz;

  if (!vmap->is_toroidal &&
      (op->x >= vmap->xmax ||
       (op->x + op->w) > vmap->xmax ||
       op->z >= vmap->zmax ||
       (op->z + op->h) > vmap->zmax))
    /* Out of bounds, non-toroidal */
    return;

  if (append_set->num_operations == QUEUE_SIZE) {
    vmap_painter_swap_sets();
    vmap_painter_start_busy(0);
  }

  bx = op->x >> bucket_xshift;
  bz = op->z >> bucket_zshift;

  if (!append_set->bucket_start[bz][bx]) {
    append_set->bucket_start[bz][bx] = append_set->num_operations;
  } else {
    append_set->index_list[append_set->bucket_end[bz][bx]] =
      append_set->num_operations;
  }
  append_set->operations[append_set->num_operations] = *op;
  append_set->bucket_end[bz][bx] = append_set->num_operations;
  append_set->index_list[append_set->num_operations] = 0;
  ++append_set->num_operations;
}

static void vmap_painter_init_queue_set(vmap_painter_queue_set* set) {
  set->num_operations = 1;
  memset(set->bucket_start, 0, sizeof(set->bucket_start));
}

static void vmap_painter_swap_sets(void) {
  vmap_painter_queue_set* tmp;

  ump_join();

  tmp = append_set;
  append_set = busy_set;
  busy_set = tmp;

  vmap_painter_init_queue_set(append_set);
}

static void vmap_painter_start_busy(int sync) {
  if (sync)
    ump_run_sync(&vmap_painter_task);
  else
    ump_run_async(&vmap_painter_task);
}

static void vmap_painter_execute(unsigned ordinal, unsigned divisions) {
  unsigned bz = ordinal / NUM_BUCKETS, bx = ordinal % NUM_BUCKETS;
  const vmap_painter_queue_set*restrict set = busy_set;
  env_vmap* v = vmap;
  vmap_painter_queue_index index;

  for (index = set->bucket_start[bz][bx]; index;
       index = set->index_list[index])
    (*set->operations[index].f)(v, set->operations+index);
}
