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
#include "../micromp.h"
#include "lluas.h"

#define MEMORY_LIMIT (64*1024*1024)

extern void open_module_mg(lua_State*);

static lua_State* interpreters[UMP_MAX_THREADS+1];
static lluas_error_status error_stati[UMP_MAX_THREADS+1];
static size_t memory_in_use = 0;

static lua_State* lluas_create_interpreter(void);
static void* lluas_alloc(void*, void*, size_t, size_t);
static int lluas_panic(lua_State*);
static void lluas_openlibs(lua_State*);
static void lluas_on_ump(const char* name, unsigned instrlimit,
                         void (*)(unsigned,unsigned));
static void lluas_do_load_file(unsigned, unsigned);
static void lluas_do_invoke_global(unsigned, unsigned);
static void lluas_invoke_function(unsigned, const char*, unsigned);
static void lluas_invoke_top_of_stack(unsigned);
static void lluas_error(unsigned interp, const char* prefix,
                        lluas_error_status status);
static int lluas_traceback(lua_State*);

void lluas_init(void) {
  unsigned i, n;

  memset(error_stati, 0 /* ok */, sizeof(error_stati));

  n = ump_num_workers() + 1;
  for (i = 0; i < n; ++i)
    if (interpreters[i])
      lua_close(interpreters[i]);

  assert(0 == memory_in_use);

  for (i = 0; i < n; ++i)
    interpreters[i] = lluas_create_interpreter();
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
  unsigned i;
  lluas_error_status worst;

  worst = les_ok;
  for (i = 0; i < UMP_MAX_THREADS+1; ++i)
    if (error_stati[i] > worst)
      worst = error_stati[i];

  return worst;
}

static const char* lluas_ump_name;
static unsigned lluas_ump_instr_limit;
static ump_task lluas_ump_task;

static void lluas_on_ump(const char* name, unsigned instr_limit,
                         void (*f)(unsigned, unsigned)) {
  lluas_ump_name = name;
  lluas_ump_instr_limit = instr_limit;
  lluas_ump_task.exec = f;
  lluas_ump_task.num_divisions = ump_num_workers() + 1;
  ump_run_sync(&lluas_ump_task);
}

void lluas_load_file(const char* filename, unsigned instr_limit) {
  lluas_on_ump(filename, instr_limit, lluas_do_load_file);
}

static void lluas_error(unsigned interp, const char* prefix,
                        lluas_error_status status) {
  lua_State* L = interpreters[interp];

  warnx("Script error: %s: %s", prefix, lua_tostring(L, -1));
  lua_pop(L, 1);
  if (status > error_stati[interp])
    error_stati[interp] = status;
}

static void lluas_do_load_file(unsigned interp, unsigned n) {
  lua_State* L = interpreters[interp];

  if (les_fatal == error_stati[interp]) return;

  /* TODO: The error reporting could be better; in some cases, it'll be hard to
   * tell which file caused the problem.
   */

  lua_setinstrlimit(L, lluas_ump_instr_limit);
  switch (luaL_loadfilex(L, lluas_ump_name, "t" /* no bytecode allowed */)) {
  case LUA_OK: break;
  case LUA_ERRSYNTAX:
    lluas_error(interp, "Syntax error", les_problematic);
    return;

  case LUA_ERRFILE:
    lluas_error(interp, "Missing file?", les_fatal);
    return;

  case LUA_ERRGCMM:
  case LUA_ERRMEM:
    lluas_error(interp, "Script memory exhausted", les_fatal);
    return;

  default:
    lluas_error(interp, "Unexpected error loading script", les_fatal);
    return;
  }

  lluas_invoke_top_of_stack(interp);
}

void lluas_invoke_global(const char* name, unsigned instr_limit) {
  lluas_on_ump(name, instr_limit, lluas_do_invoke_global);
}

static void lluas_do_invoke_global(unsigned interp, unsigned n) {
  lluas_invoke_function(interp, lluas_ump_name, lluas_ump_instr_limit);
}

void lluas_invoke_local(unsigned interp, const char* name,
                        unsigned instr_limit) {
  lluas_invoke_function(interp, name, instr_limit);
}

static void lluas_invoke_function(unsigned interp, const char* name,
                                  unsigned instr_limit) {
  lua_State* L = interpreters[interp];

  if (les_fatal == error_stati[interp]) return;

  lua_setinstrlimit(L, lluas_ump_instr_limit);
  lua_getglobal(L, lluas_ump_name);
  lluas_invoke_top_of_stack(interp);
}

static void lluas_invoke_top_of_stack(unsigned interp) {
  lua_State* L = interpreters[interp];
  unsigned base = lua_gettop(L);

  /* Push handler to generate stack trace under the thing at the top */
  lua_pushcfunction(L, lluas_traceback);
  lua_insert(L, base);
  switch (lua_pcall(L, 0, 0, base)) {
  case LUA_OK: break;

  case LUA_ERRRUN:
    lluas_error(interp, "Runtime error", les_problematic);
    break;

  case LUA_ERRERR:
    lluas_error(interp, "Runtime error; "
                "supplementary information unavailable", les_problematic);
    break;

  case LUA_ERRMEM:
  case LUA_ERRGCMM:
    lluas_error(interp, "Script memory exhausted", les_fatal);
    break;

  default:
    lluas_error(interp, "Unexpected error", les_fatal);
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
