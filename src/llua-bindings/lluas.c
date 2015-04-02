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

#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <assert.h>

#include "../contrib/lua/lua.h"
#include "../contrib/lua/lauxlib.h"
#include "../contrib/lua/lualib.h"

#include "../bsd.h"
#include "lluas.h"

#define MEMORY_LIMIT (64*1024*1024)

extern void open_module_mg(lua_State*);

static lua_State* interpreter;
static lluas_error_status error_status;
static size_t memory_in_use = 0;

static lua_State* lluas_create_interpreter(void);
static void* lluas_alloc(void*, void*, size_t, size_t);
static int lluas_panic(lua_State*);
static void lluas_openlibs(lua_State*);
static void lluas_invoke_function(const char*, unsigned);
static void lluas_invoke_top_of_stack(void);
static void lluas_error(const char* prefix,
                        lluas_error_status status);
static int lluas_traceback(lua_State*);

void lluas_init(void) {
  error_status = 0;

  if (interpreter)
    lua_close(interpreter);

  assert(0 == memory_in_use);

  interpreter = lluas_create_interpreter();
}

static lua_State* lluas_create_interpreter(void) {
  lua_State* L;

  L = lua_newstate(lluas_alloc, NULL);
  if (!L)
    errx(EX_UNAVAILABLE, "out of memory");

  lua_atpanic(L, lluas_panic);

  lua_gc(L, LUA_GCSTOP, 0);
  lluas_openlibs(L);
  lua_gc(L, LUA_GCRESTART, 0);
  return L;
}

static int lluas_panic(lua_State* L) {
  errx(EX_SOFTWARE, "Llua PANIC: %s", lua_tostring(L, -1));
  /* unreachable */
  return 0;
}

static const luaL_Reg lluas_libraries[] = {
  { "_G", luaopen_base },
  { LUA_COLIBNAME, luaopen_coroutine },
  { LUA_TABLIBNAME, luaopen_table },
  { LUA_STRLIBNAME, luaopen_string },
  { LUA_BITLIBNAME, luaopen_bit32 },
  { NULL, NULL }
};

/* Adapted from linit.c */
static void lluas_openlibs(lua_State* L) {
  const luaL_Reg* lib;

  for (lib = lluas_libraries; lib->func; ++lib) {
    luaL_requiref(L, lib->name, lib->func, 1);
    lua_pop(L, 1);
  }

  open_module_mg(L);
}

static void* lluas_alloc(void* userdata, void* ptr,
                         size_t osize, size_t nsize) {
  size_t new_memory_use;

  if (0 == nsize) {
    if (ptr) {
      free(ptr);
      memory_in_use -= osize;
    }

    return NULL;
  } else {
    new_memory_use = memory_in_use + nsize;
    if (ptr) new_memory_use -= osize;

    if (new_memory_use > MEMORY_LIMIT)
      return NULL;

    /* realloc() doesn't guarantee that shrinks won't fail, though Lua requires
     * it. However, Lua's standard implementation uses it that way anyway, so
     * it seems safe to do so here as well.
     */
    ptr = realloc(ptr, nsize);
    if (ptr)
      memory_in_use = new_memory_use;

    return ptr;
  }
}

lluas_error_status lluas_get_error_status(void) {
  return error_status;
}

void lluas_load_file(const char* filename, unsigned instr_limit) {
  lua_State* L = interpreter;

  if (les_fatal == error_status) return;

  /* TODO: The error reporting could be better; in some cases, it'll be hard to
   * tell which file caused the problem.
   */

  lua_setinstrlimit(L, instr_limit);
  switch (luaL_loadfilex(L, filename, "t" /* no bytecode allowed */)) {
  case LUA_OK: break;
  case LUA_ERRSYNTAX:
    lluas_error("Syntax error", les_problematic);
    return;

  case LUA_ERRFILE:
    lluas_error("Missing file?", les_fatal);
    return;

  case LUA_ERRGCMM:
  case LUA_ERRMEM:
    lluas_error("Script memory exhausted", les_fatal);
    return;

  default:
    lluas_error("Unexpected error loading script", les_fatal);
    return;
  }

  lluas_invoke_top_of_stack();
}

static void lluas_error(const char* prefix, lluas_error_status status) {
  lua_State* L = interpreter;

  warnx("Script error: %s: %s", prefix, lua_tostring(L, -1));
  lua_pop(L, 1);
  if (status > error_status)
    error_status = status;
}

void lluas_invoke_global(const char* name, unsigned instr_limit) {
  lluas_invoke_function(name, instr_limit);
}

static void lluas_invoke_function(const char* name,
                                  unsigned instr_limit) {
  lua_State* L = interpreter;

  if (les_fatal == error_status) return;

  lua_setinstrlimit(L, instr_limit);
  lua_getglobal(L, name);
  lluas_invoke_top_of_stack();
}

static void lluas_invoke_top_of_stack(void) {
  lua_State* L = interpreter;
  unsigned base = lua_gettop(L);

  /* Push handler to generate stack trace under the thing at the top */
  lua_pushcfunction(L, lluas_traceback);
  lua_insert(L, base);
  switch (lua_pcall(L, 0, 0, base)) {
  case LUA_OK: break;

  case LUA_ERRRUN:
    lluas_error("Runtime error", les_problematic);
    break;

  case LUA_ERRERR:
    lluas_error("Runtime error; "
                "supplementary information unavailable", les_problematic);
    break;

  case LUA_ERRMEM:
  case LUA_ERRGCMM:
    lluas_error("Script memory exhausted", les_fatal);
    break;

  default:
    lluas_error("Unexpected error", les_fatal);
    break;
  }

  lua_remove(L, base);
}

static int lluas_traceback(lua_State* L) {
  /* Adapted from lua.c, but without metamethod support */
  const char* msg;

  msg = lua_tostring(L, 1);
  if (msg)
    luaL_traceback(L, L, msg, 1);
  else
    lua_pushliteral(L, "(no error message)");

  return 1;
}
