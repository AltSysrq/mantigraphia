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

#include <string.h>
#include <stdarg.h>
#include <assert.h>

#include "../bsd.h"
#include "../rand.h"

#include "lsystem.h"

void lsystem_compile(lsystem* this, ...) {
  va_list args;
  const char* rule, * next;
  unsigned ix, r, nr;

  va_start(args, this);

  while ((rule = va_arg(args, const char*))) {
    ix = rule[0];
    assert(!this->rules[ix].replacement[0]);
    assert(' ' == rule[1]);
    rule += 2;
    r = 0;
    do {
      assert(r < LSYSTEM_MAX_REPLS);
      next = strchr(rule, ' ');
      this->rules[ix].replacement[r] = rule;
      if (next)
        this->rules[ix].replacement_size[r] = next - rule;
      else
        this->rules[ix].replacement_size[r] = strlen(rule);

      rule = next + 1;
      ++r;
    } while (next);

    /* Populate remaining replacements */
    for (nr = r; r < LSYSTEM_MAX_REPLS; ++r) {
      this->rules[ix].replacement[r] = this->rules[ix].replacement[r-nr];
      this->rules[ix].replacement_size[r] =
        this->rules[ix].replacement_size[r-nr];
    }
  }

  va_end(args);
}

void lsystem_execute(lsystem_state* state,
                     const lsystem* this,
                     const char* initial,
                     unsigned steps,
                     unsigned random) {
  char* srcbuff, * dstbuff, * src, * dst;
  unsigned size = strlen(initial)+1;
  size_t cpysz;
  unsigned rule, repl;

 srcbuff = (char*)initial;
 dstbuff = state->buffer;

  while (steps--) {
    src = srcbuff;
    dst = dstbuff;

    while ((rule = *src)) {
      if (this->rules[rule].replacement[0]) {
        repl = lcgrand(&random) % LSYSTEM_MAX_REPLS;

        /* Replace according to rule. Stop now if this would exceed the buffer
         * size.
         */
        if (size + this->rules[rule].replacement_size[repl] - 1 >=
            LSYSTEM_MAX_SZ)
          goto finish;

        memcpy(dst, this->rules[rule].replacement[repl],
               this->rules[rule].replacement_size[repl]);
        dst += this->rules[rule].replacement_size[repl];
        size += this->rules[rule].replacement_size[repl] - 1;
        ++src;
      } else {
        /* No replacement */
        *dst++ = *src++;
      }
    }

    /* NUL-terminate dst */
    *dst = 0;

    /* Swap buffers. This can't be a variable swap, as the initial srcbuff is
     * the read-only input argument of unknown size.
     */
    if (dstbuff == state->buffer) {
      dstbuff = state->temp;
      srcbuff = state->buffer;
    } else {
      dstbuff = state->buffer;
      srcbuff = state->temp;
    }
  }

  finish:
  /* Copy current source into the main buffer if that isn't the current */
  if (srcbuff != state->buffer) {
    cpysz = strlcpy(state->buffer, srcbuff, LSYSTEM_MAX_SZ);
    assert(cpysz < LSYSTEM_MAX_SZ);
  }
}
