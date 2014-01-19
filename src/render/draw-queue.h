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
#ifndef RENDER_DRAW_QUEUE_H_
#define RENDER_DRAW_QUEUE_H_

#include <assert.h>
#include <stdlib.h>

#include "../graphics/abstract.h"
#include "../graphics/canvas.h"

/**
 * A drawing_queue stores a list of abstract drawing_method calls, which can
 * later be executed on an arbitrary canvas at an arbitrary offset.
 *
 * The purpose of this system is to support multi-threaded rendering. In order
 * to eliminate sharing and needing to choose between either the penalty of
 * atomic canvas updates or race condition artefacts, each thread must write to
 * a fixed region of the canvas. (The earlier plan, to have each thread write
 * its own set of objects to its own canvas, then merge them, is too expensive,
 * due to the required memory bandwidth.)
 *
 * In this system, each thread records how to draw its own set of objects into
 * its own drawing_queue. Then, all threads render all drawing queues into
 * their own slices of the canvas to produce the full image without needing
 * copying or atomic updates.
 *
 * The queue is populated with sequences of commands divided into
 * "bursts". Each burst writes into exactly two contiguous regions of memory;
 * it is therefore required that no burst is more than 4096 instructions long
 * or 1MB of data long.
 */
typedef struct drawing_queue_s drawing_queue;

typedef unsigned drawing_queue_instruction;

/**
 * Stores the state used by a single drawing_queue burst. This structure is
 * initialised by drawing_queue_start_burst() and destroyed by
 * drawing_queue_end_burst(), though the caller is expected to provide storage
 * for it (ie, a stack variable).
 */
typedef struct {
  drawing_queue_instruction*restrict instructions;
  char*restrict data;
} drawing_queue_burst;

enum drawing_queue_opcode {
  dqinstr_code_method = 0,
  dqinstr_code_accum,
  dqinstr_code_accum_canvas,
  dqinstr_code_1point,
  dqinstr_code_2point,
  dqinstr_code_line,
  dqinstr_code_point,
  dqinstr_code_flush
};
#define DQINSTR_METHOD(sz)    (            (sz<<8) | dqinstr_code_method)
#define DQINSTR_ACCUM(sz,off) ((off<<24) | (sz<<8) | dqinstr_code_accum)
#define DQINSTR_ACCUM_CANVAS  (                      dqinstr_code_accum_canvas)
#define DQINSTR_1POINT        (                      dqinstr_code_1point)
#define DQINSTR_2POINT        (                      dqinstr_code_2point)
#define DQINSTR_LINE(w)       (            (w <<8) | dqinstr_code_line)
#define DQINSTR_POINT(w)      (            (w <<8) | dqinstr_code_point)
#define DQINSTR_FLUSH         (                      dqinstr_code_flush)

typedef struct {
  vo3 where;
  zo_scaling_factor weight;
} drawing_queue_point;

#define DRAWING_QUEUE_POINT_SZ                  \
  ((sizeof(drawing_queue_point) + sizeof(void*) - 1) /  \
   sizeof(void*) * sizeof(void*))

/**
 * Allocates and returns a new, empty drawing_queue.
 */
drawing_queue* drawing_queue_new(void);
/**
 * Frees the memory held by the given drawing_queue.
 */
void drawing_queue_delete(drawing_queue*);

/**
 * Resets the given drawing_queue so that it contains no elements.
 */
void drawing_queue_clear(drawing_queue*);

/**
 * Begins a burst on the given drawing_queue, storing state in the given burst
 * pointer. drawing_queue_end_burst() MUST be called on the same pair of values
 * before the queue itself is ever used again.
 */
void drawing_queue_start_burst(drawing_queue_burst*, drawing_queue*);

/**
 * Sets the drawing_method that is to be used for all subsequent drawing
 * operations in this queue. The method is copied into the data of the queue,
 * so the caller need not preserve meth. The maximum size is 64kB minus one
 * pointer, inclusive.
 */
static inline void drawing_queue_put_method(drawing_queue_burst*restrict burst,
                                            const drawing_method*restrict meth,
                                            size_t sz) {
  assert(sz <= 65536-sizeof(void*));
  memcpy(burst->data, meth, sz);
  /* Increase sz to pointer alignment (*after* copying from the input) */
  sz = (sz + sizeof(void*) - 1) / sizeof(void*) * sizeof(void*);
  burst->data += sz;
  *burst->instructions++ = DQINSTR_METHOD(sz);
}

/**
 * Sets the drawing accumulator that is to be used for subsequent drawing
 * operations. data MUST point to a structure containing a canvas* variable at
 * canvas_offset, which must be less than 256 and must have
 * pointer-alignment. The accumulator is copied into the data of the queue, so
 * the caller need not preserve data. On drawing, the destination canvas is
 * written to the given offset before using the accumulator; the field at that
 * offset need not be initialised at the time of this call. The maximum size is
 * 64kB minus one pointer, inclusive.
 *
 * @see drawing_queue_accum_canvas()
 */
static inline void drawing_queue_put_accum(drawing_queue_burst*restrict burst,
                                           const void*restrict data,
                                           size_t sz,
                                           size_t canvas_offset) {
  assert(sz <= 65536-sizeof(void*));
  assert(canvas_offset < 256);
  memcpy(burst->data, data, sz);
  sz = (sz + sizeof(void*) - 1) / sizeof(void*) * sizeof(void*);
  burst->data += sz;
  *burst->instructions++ = DQINSTR_ACCUM(sz, canvas_offset);
}

/**
 * Specifies that the drawing accumulator for subsequent drawing operations is
 * to simply be the target canvas.
 */
static inline void drawing_queue_accum_canvas(
  drawing_queue_burst*restrict burst)
{
  *burst->instructions++ = DQINSTR_ACCUM_CANVAS;
}

/**
 * Sets the screen point that is to be used as the "to" point for the next
 * drawing operation. The current "to" point is moved to the "from" point. The
 * weight corresponds to the value passed into the drawing_method operations.
 */
static inline void drawing_queue_put_point(
  drawing_queue_burst*restrict burst,
  const vo3 p1,
  zo_scaling_factor w1)
{
  drawing_queue_point* pt = (drawing_queue_point*)burst->data;
  *burst->instructions++ = DQINSTR_1POINT;
  memcpy(pt->where, p1, sizeof(vo3));
  pt->weight = w1;
  burst->data += DRAWING_QUEUE_POINT_SZ;
}

/**
 * Sets the from (1) and to (2) points for the next drawing operation. The
 * weights correspond to the values passed in as such into the drawing_method
 * operations.
 */
static inline void drawing_queue_put_points(
  drawing_queue_burst*restrict burst,
  const vo3 p1,
  zo_scaling_factor w1,
  const vo3 p2,
  zo_scaling_factor w2)
{
  drawing_queue_point* pt = (drawing_queue_point*)burst->data;
  *burst->instructions++ = DQINSTR_2POINT;
  memcpy(pt->where, p1, sizeof(vo3));
  pt->weight = w1;
  burst->data += DRAWING_QUEUE_POINT_SZ;
  pt = (drawing_queue_point*)burst->data;
  memcpy(pt->where, p2, sizeof(vo3));
  pt->weight = w2;
  burst->data += DRAWING_QUEUE_POINT_SZ;
}

/**
 * Enqueues the drawing of a line from the current "from" point to the current
 * "to" point, which will have at most the width, in pixels, indicated by
 * width, which must be less than (1<<24).
 */
static inline void drawing_queue_draw_line(
  drawing_queue_burst*restrict burst,
  unsigned width)
{
  assert(width < (1<<24));
  *burst->instructions++ = DQINSTR_LINE(width);
}

/**
 * Enqueues the drawing of a point at the current "to" point, which will have
 * at most an L_inf-radius of size pixels, which must be less than (1<<24).
 */
static inline void drawing_queue_draw_point(
  drawing_queue_burst*restrict burst,
  unsigned size)
{
  assert(size < (1<<24));
  *burst->instructions++ = DQINSTR_POINT(size);
}

/**
 * Enuqeues a flush of the current drawing method.
 */
static inline void drawing_queue_flush(drawing_queue_burst*restrict burst) {
  *burst->instructions++ = DQINSTR_FLUSH;
}

/**
 * Ends the given burst, updating the offsets within the queue and allocating
 * more memory if necessary. After this call, the input burst should be
 * considered destroyed; if needed, reinitialise it with
 * drawing_queue_start_burst().
 */
void drawing_queue_end_burst(drawing_queue*, const drawing_queue_burst*);

/**
 * Executes the operations within the given queue to render to the given
 * canvas. All coordinates are shifted by adding the gixen ox and oy
 * values. Groups of drawing operations that are provably no-ops are not
 * executed.
 */
void drawing_queue_execute(canvas*, const drawing_queue*,
                           coord_offset ox, coord_offset oy);

#define DQMETH(dq,meth)                                                 \
  drawing_queue_put_method(&(dq), (const drawing_method*)&(meth), sizeof(meth))
#define DQACC(dq,acc)                                                   \
  drawing_queue_put_accum(&(dq), &(acc), sizeof(acc), offsetof((acc), dst))
#define DQCANV(dq)                              \
  drawing_queue_accum_canvas(&dq)

#endif /* RENDER_DRAW_QUEUE_H_ */
