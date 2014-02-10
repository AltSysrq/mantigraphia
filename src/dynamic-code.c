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

#include <SDL.h>

#if defined(HAVE_SYS_MMAN_H) && defined(HAVE_MMAP) && defined(HAVE_MUNMAP)
#define MMAP_SUPPORTED
#endif

#ifdef MMAP_SUPPORTED
#include <sys/mman.h>
#endif

#include <stdlib.h>
#include <string.h>

#include "bsd.h"

#include "simd.h"
#include "dynamic-code.h"

SDL_TLSID thread_region;

static void* dynamic_code_new_region(void) {
  void* ret = NULL;
  static int has_warned = 0;
#ifdef MMAP_SUPPORTED
  ret = mmap(NULL, 4096, PROT_READ|PROT_WRITE|PROT_EXEC, MAP_ANON, -1, 0);
  if (!ret && !has_warned) {
    has_warned = 1;
    warn("Unable to create page for dynamic code compilation");
  }
#endif

  return ret;
}

static void dynamic_code_delete_region(void* region) {
  if (region) {
#ifdef MMAP_SUPPORTED
    munmap(region, 4096);
#endif
  }
}

void dynamic_code_init(void) {
  thread_region = SDL_TLSCreate();
  if (!thread_region)
    warnx("Unable to init TLS for dynamic code generation: %s", SDL_GetError());
}

void dynamic_code_init_thread(void) {
  void* region;

  if (!thread_region) return;
  region = dynamic_code_new_region();

  if (thread_region && SDL_TLSSet(thread_region, region,
                                  dynamic_code_delete_region)) {
    warnx("Unable to set TLS for dynamic code generation: %s", SDL_GetError());
    dynamic_code_delete_region(region);
  }
}

static void* dynamic_code_get_region(void) {
  if (thread_region)
    return SDL_TLSGet(thread_region);
  else
    return NULL;
}

static simd4 emulated_shufps(simd4 a, simd4 b, unsigned mask) {
  return simd_shuffle2(a, b, simd_initl((mask >> 6) & 3,
                                        (mask >> 4) & 3,
                                        (mask >> 2) & 3,
                                        (mask >> 0) & 3));
}

simd4 (*dynamic_code_shufps(unsigned char mask))
(simd4, simd4, unsigned) {
  void* region = dynamic_code_get_region();
#if defined(__SSE__)
  /*
    0f c6 c1 XX         shufps xmm0, xmm1, mask
    c3                  ret
   */
  const unsigned char code[] = {
    0x0F, 0xC6, 0xC1, mask,
    0xC3,
  };

  if (!region) return emulated_shufps;

  memcpy(region, code, sizeof(code));
  return (simd4 (*)(simd4, simd4, unsigned))region;
#else
  return emulated_shufps;
#endif
}
