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
#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include <stdlib.h>
#include <string.h>

#include "../alloc.h"
#include "../frac.h"

#include "draw-queue.h"

#define MIN_AVAIL_INSTRS 4096
#define MIN_AVAIL_DATA (1*1024*1024)
#define MIN_PAGE_BURSTS 4
#define INSTRS_PER_PAGE (MIN_PAGE_BURSTS * MIN_AVAIL_INSTRS)
#define DATA_PER_PAGE (MIN_PAGE_BURSTS * MIN_AVAIL_DATA)
#define MAX_PAGES 32

struct drawing_queue_s {
  unsigned num_pages, current_page;
  int is_active;
  unsigned num_instrs[MAX_PAGES];
  unsigned data_offset[MAX_PAGES];
  drawing_queue_instruction*restrict instructions[MAX_PAGES];
  char*restrict data[MAX_PAGES];
};

static void alloc_page(drawing_queue* this, unsigned page) {
  this->instructions[page] = xmalloc(sizeof(drawing_queue_instruction) *
                                     INSTRS_PER_PAGE);
  this->data[page] = xmalloc(DATA_PER_PAGE);
  this->num_instrs[page] = 0;
  this->data_offset[page] = 0;
}

drawing_queue* drawing_queue_new(void) {
  drawing_queue* this = xmalloc(sizeof(drawing_queue));
  drawing_queue_clear(this);
  this->num_pages = 1;
  this->is_active = 0;
  alloc_page(this, 0);
  return this;
}

void drawing_queue_delete(drawing_queue* this) {
  unsigned i;

  for (i = 0; i < this->num_pages; ++i) {
    free(this->instructions[i]);
    free(this->data[i]);
  }

  free(this);
}

void drawing_queue_clear(drawing_queue* this) {
  memset(this->num_instrs, 0, sizeof(this->num_instrs));
  memset(this->data_offset, 0, sizeof(this->data_offset));
  this->current_page = 0;
}

void drawing_queue_start_burst(drawing_queue_burst* burst,
                               drawing_queue* this) {
  assert(!this->is_active);
  this->is_active = 1;

  /* Ensure there is enough space for the next burst */
  if (DATA_PER_PAGE - this->data_offset[this->current_page] < MIN_AVAIL_DATA ||
      INSTRS_PER_PAGE-this->num_instrs[this->current_page] < MIN_AVAIL_INSTRS) {
    ++this->current_page;
    if (this->current_page == this->num_pages) {
      if (this->num_pages >= MAX_PAGES)
        abort();

      ++this->num_pages;
      alloc_page(this, this->current_page);
    }
  }

  /* OK, copy offsets */
  burst->instructions = this->instructions[this->current_page] +
                        this->num_instrs[this->current_page];
  burst->data = this->data[this->current_page] +
                this->data_offset[this->current_page];
}

void drawing_queue_end_burst(drawing_queue* this,
                             const drawing_queue_burst* burst) {
  assert(this->is_active);
  this->is_active = 0;

  this->num_instrs[this->current_page] =
    burst->instructions - this->instructions[this->current_page];
  this->data_offset[this->current_page] =
    burst->data - this->data[this->current_page];

  assert(this->num_instrs[this->current_page] <= INSTRS_PER_PAGE);
  assert(this->data_offset[this->current_page] <= DATA_PER_PAGE);
}

static inline int line_intersects_canvas(const canvas*restrict dst,
                                         const vo3 pa,
                                         const vo3 pb,
                                         coord_offset w) {
  /* For now, determine a bounding box for the line, expand in each direction
   * by width, and check that. The code's simpler, but the precise check will
   * probably be faster than actually drawing anything. However, the bounding
   * box method probably will not result in enough false positives to make it
   * worth implementing the more precise test.
   */
  coord_offset x0, x1, y0, y1;

  if (pa[0] < pb[0]) {
    x0 = pa[0] - w;
    x1 = pb[0] + w;
  } else {
    x0 = pb[0] - w;
    x1 = pa[0] + w;
  }

  if (pa[1] < pb[1]) {
    y0 = pa[1] - w;
    y1 = pb[1] + w;
  } else {
    y0 = pb[1] - w;
    y1 = pa[1] + w;
  }

  return
    x0 < (signed)dst->w &&
    x1 >= 0 &&
    y0 < (signed)dst->h &&
    y1 >= 0;
}

static inline int point_intersects_canvas(const canvas*restrict dst,
                                          const vo3 point,
                                          coord_offset width) {
  return
    point[0] + width >= 0 &&
    point[0] - width < (signed)dst->w &&
    point[1] + width >= 0 &&
    point[1] - width < (signed)dst->h;
}

void drawing_queue_execute(canvas* dst, const drawing_queue* this,
                           coord_offset ox, coord_offset oy) {
  /* Storage for the local copy of the accumulator, if applicable. accum has
   * the actual value of the accumulator, since it isn't necessarily memory
   * under our control.
   */
  char accum_buff[65536];
  /* The pointer to use for the drawing accumulator */
  void* accum = NULL;
  /* The current drawing method. This points into the data for a page. */
  const drawing_method*restrict method = NULL;
  /* The from and to points for drawing */
  drawing_queue_point from, to;
  /* The from and to at time of starting a fast-forward */
  drawing_queue_point from_rewind, to_rewind;
  /* Whether we are are "fast-forwarding" through operations. On any operation
   * that breaks a sequence of drawing operations (changing method/accum/page,
   * or explicit flush), it is set to 1, and the rewind checkpoint is pointed
   * at the next operation. While fast-forwarding, each drawing operation is
   * checked as to whether is possibly intersects the canvas. If it does, the
   * fast_forwarding flag is unset, and the instruction and data pointers are
   * moved back to the rewind checkpoint. While not fast-forwarding, all
   * drawing operations are performed without checking whether they intersect
   * the screen.
   */
  int fast_forwarding;
  /* The current instruction/data page. */
  unsigned page;
  /* The current instruction offset, and rewind-checkpoint for cancelling
   * fast-forward operation.
   */
  unsigned instr_off, instr_off_rewind;
  /* The current data pointer, and rewind-checkpoint for cancelling
   * fast-forward operation.
   */
  const char* data, * data_rewind;
  drawing_queue_instruction instruction;

  for (page = 0; page <= this->current_page; ++page) {
    instr_off = 0;
    data = this->data[page];

#define FAST_FORWARD                            \
    fast_forwarding = 1;                        \
    instr_off_rewind = instr_off;               \
    data_rewind = data;                         \
    from_rewind = from;                         \
    to_rewind = to

#define REWIND                                  \
    fast_forwarding = 0;                        \
    instr_off = instr_off_rewind;               \
    data = data_rewind;                         \
    from = from_rewind;                         \
    to = to_rewind

    FAST_FORWARD;

    while (instr_off < this->num_instrs[page]) {
      instruction = this->instructions[page][instr_off++];
      switch (instruction & 0xFF) {
      default:
        abort();

      case dqinstr_code_method:
        method = (const drawing_method*restrict)data;
        data += (instruction >> 8) & 0xFFFF;
        FAST_FORWARD;
        break;

      case dqinstr_code_accum:
        memcpy(accum_buff, data, (instruction >> 8) & 0xFFFF);
        *(canvas**)(accum_buff + ((instruction >> 24) & 0xFF)) = dst;
        data += (instruction >> 8) & 0xFFFF;
        accum = accum_buff;
        FAST_FORWARD;
        break;

      case dqinstr_code_accum_canvas:
        accum = dst;
        FAST_FORWARD;
        break;

      case dqinstr_code_1point:
        from = to;
        to = *(const drawing_queue_point*restrict)data;
        to.where[0] += ox;
        to.where[1] += oy;
        data += DRAWING_QUEUE_POINT_SZ;
        break;

      case dqinstr_code_2point:
        from = *(const drawing_queue_point*restrict)data;
        data += DRAWING_QUEUE_POINT_SZ;
        to = *(const drawing_queue_point*restrict)data;
        data += DRAWING_QUEUE_POINT_SZ;

        from.where[0] += ox;
        from.where[1] += oy;
        to.where[0] += ox;
        to.where[1] += oy;
        break;

      case dqinstr_code_line:
        if (fast_forwarding) {
          if (line_intersects_canvas(dst, from.where, to.where,
                                     (instruction >> 8) & 0xFFFFFF)) {
            REWIND;
          }
        } else {
          dm_draw_line(accum, method, from.where, from.weight,
                       to.where, to.weight);
        }

        break;

      case dqinstr_code_point:
        if (fast_forwarding) {
          if (point_intersects_canvas(dst, to.where,
                                      (instruction >> 8) & 0xFFFFFF)) {
            REWIND;
          }
        } else {
          dm_draw_point(accum, method, to.where, to.weight);
        }

        break;

      case dqinstr_code_flush:
        /* Only need to actually flush if not fast-forwarding.
         * In either case, this point starts a fast-forward.
         */
        if (!fast_forwarding)
          dm_flush(accum, method);

        FAST_FORWARD;
        break;
      }
    }

    #undef FAST_FORWARD
    #undef REWIND
  }
}
