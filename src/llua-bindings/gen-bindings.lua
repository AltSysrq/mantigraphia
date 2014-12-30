---
-- Copyright (c) 2014 Jason Lingle
-- All rights reserved.
--
-- Redistribution and use in source and binary forms, with or without
-- modification, are permitted provided that the following conditions
-- are met:
-- 1. Redistributions of source code must retain the above copyright
--    notice, this list of conditions and the following disclaimer.
-- 2. Redistributions in binary form must reproduce the above copyright
--    notice, this list of conditions and the following disclaimer in the
--    documentation and/or other materials provided with the distribution.
-- 3. Neither the name of the author nor the names of its contributors
--    may be used to endorse or promote products derived from this software
--    without specific prior written permission.
--
-- THIS SOFTWARE IS PROVIDED BY THE AUTHOR ``AS IS'' AND ANY EXPRESS OR
-- IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES
-- OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.
-- IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR ANY DIRECT, INDIRECT,
-- INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT
-- NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
-- DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
-- THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
-- (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
-- THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.

-- Usage: llua gen-bindings.lua packagename
-- (It may be neccessary to set LUA_PATH so that the package can be found.)
--
-- Generates C code (to stdout) which binds Llua to C functions.
--
-- Support is deliberately quite limited (unlike SWIG/tolua) so that there is
-- little to no potential for error. In particular, the bingings are totally
-- stateless.
--
-- Each binding is either a constant or a function. Arguments to functions are
-- always of fixed count and type. Functions are bound in the normal lua
-- manner; constants are copied into the package table, but are not constant as
-- far as Llua is concerned (ie, it does the same thing as SWIG).
--
-- Types:
--
-- - nil: Not an actual type. Indicates void return from functions.
-- - sint(w): Signed integer of width w.
-- - uint(w): Unsigned integer of width w.
-- - bytes(n): An array of exactly n bytes. Converts to Llua string. n may be
--   an arbitrary C expression.
-- - ntbs(): An arbitrary NUL-terminated byte string.
-- - bool
-- - pointer(t): For arguments to C functions, indicates that they take a
--   pointer to a value of t. Cannot be imported into Llua.
--
-- Byte strings are always pushed around on a borrowing basis: As inputs to
-- functions, it is assumed the function copies the string itself if necessary.
-- As outputs, the return value is copied as a new Llua string.
--
-- Byte strings are not nullable by default; this can be changed by calling
-- :nullable() on the value. Additionally, they are immutable to the C function
-- that receives them; use :mutable() to change this.
--
-- Things that are conceivably mutated by a C function can be "echoed", ie,
-- have their new value returned as an additional return value to Llua.
--
-- The module defining the package is expected to set the global `module_name`
-- to the name of the module to define, `preamble` to the text to insert at the
-- top of the generated code, `constants` to a table of constant names mapped
-- to types, and `functions` to a table of function names mapped to function
-- descriptors.

preamble = ""
constants = {}
functions = {}

function printf(s,...)
  return print(s:format(...))
end

function integer(signed, width)
  local min = nil
  local max = nil
  local failure_value = nil
  local failure_message = nil

  local self = {}

  function self:declare_for_import(name)
    printf('%ssigned long long %s;', signed and "" or "un", name)
  end

  function self:declare_for_export(name)
    self:declare_for_import(name)
  end

  function self:into_c(dst, src, context)
    printf('if (!lua_isnumber(L, %d)) {', src)
    printf('  lua_pushstring(L, "Expected %ssigned %d-bit integer for %s");',
           signed and "" or "un", width, context)
    printf('  return lua_error(L);')
    printf('}')
    printf('%s = lua_tonumber(L, %d);', dst, src)
    self:bang_bits(dst)

    if min then
      printf('if (%s < %d) {', dst, min)
      printf('  lua_pushstring(L, "Value for %s is below minimum of %d");',
             context, min)
      printf('  return lua_error(L);')
      printf('}')
    end

    if max then
      printf('if (%s > %d) {', dst, max)
      printf('  lua_pushstring(L, "Value for %s is above maximum of %d");',
             context, max)
      printf('  return lua_error(L);')
      printf('}')
    end
  end

  function self:into_llua(src)
    self:bang_bits(src)
    if failure_value then
      printf('if (%s == %d) {', src, failure_value)
      printf('  lua_pushstring(L, "%s");', failure_message);
      printf('  return lua_error(L);')
      printf('}')
    end
    printf('lua_pushnumber(L, %s);', src)
  end

  function self:bang_bits(dst)
    printf('%s &= 0x%016XLL;', dst, 2^width - 1)
    if signed then
      printf('if (%s & 0x%016XLL) %s |= 0x%016XLL;',
             dst, 2^(width-1), dst, -(2^width))
    end
  end

  function self:min(m)
    min = m
    return self
  end

  function self:max(m)
    max = m
    return self
  end

  function self:fail_on(value, message)
    failure_value = value
    failure_message = message
    return self
  end

  return self
end

function uint(w)
  return integer(false, w)
end

function sint(w)
  return integer(true, w)
end

function bytestring(length, extra_bytes)
  local self = {}
  local nullable = false
  local mutable = false
  local max_length = nil

  function self:declare_for_import(name)
    if mutable then
      -- The value llua gives us is const, so we'll need to make a copy.
      printf('const void* %s_src; size_t %s_sz;', name, name)
      if length then
        -- Know the length already, the copy can live on the stack
        printf('char %s[(%s)+%d];', name, length, extra_bytes)
      else
        -- Don't know the length ahead of time, will need to make copy
        printf('void* %s;', name)
      end
    else
      printf('const void* %s, * %s_src; size_t %s_sz;', name, name, name)
    end
  end

  function self:declare_for_export(name)
    printf('const void* %s;', name)
  end

  function self:into_c(dst, src, context)
    printf('if (!lua_isnil(L, %d) && !lua_isstring(L, %d)) {', src, src)
    if length then
      printf('  lua_pushfstring(L, "Expected byte[%%d] for %s", (int)(%s));',
             context, length)
    else
      printf('  lua_pushstring(L, "Expected string for %s");', context)
    end
    printf('  return lua_error(L);')
    printf('}')
    printf('%s_src = (void*)lua_tolstring(L, %d, &%s_sz);', dst, src, dst)
    if not nullable then
      printf('if (!%s_src) {', dst)
      printf('  lua_pushstring(L, "%s must be non-nil");', context)
      printf('  return lua_error(L);')
      printf('}')
    end

    if length then
      printf('if (%s_sz != (%s)) {', dst, length)
      printf('  lua_pushfstring(L, "Expected byte[%%d] for %s", (int)(%s));',
             context, length)
      printf('  return lua_error(L);')
      printf('}')
    end

    if max_length then
      printf('if (%s_sz > %d) {', dst, max_length)
      printf('  lua_pushstring(L, "String for %s is too long (max %d bytes)");',
             context, max_length)
      printf('  return lua_error(L);')
      printf('}')
    end

    if mutable then
      if not length then
        printf('%s = alloca(%s_sz + %d);', dst, dst, extra_bytes)
      end
      printf('memcpy(%s, %s_src, %s_sz + %d);', dst, dst, dst, extra_bytes)
    else
      printf('%s = %s_src;', dst, dst);
    end
  end

  function self:echo_into_llua(src)
    self:into_llua(src)
  end

  function self:into_llua(src)
    if length then
      printf('lua_pushlstring(L, (const char*)%s, %s);', src, length)
    else
      printf('lua_pushstring(L, (const char*)%s);', src)
    end
  end

  function self:nullable()
    nullable = true
    return self
  end

  function self:mutable()
    mutable = true
    return self
  end

  function self:max_length(m)
    max_length = m
    return self
  end

  function self:echo()
    self.echo_argument = true
    return self
  end

  return self
end

function bytes(n)
  return bytestring(n, 0)
end

function ntbs()
  return bytestring(nil, 1)
end

function pointer(t)
  local self = {}

  function self:declare_for_import(name)
    t:declare_for_import(name .. "_pointee")
    printf('void* %s;', name)
  end

  function self:into_c(dst, src, context)
    t:into_c(dst .. "_pointee", src, "pointee of " .. context)
    printf('%s = &%s_pointee;', dst, dst)
  end

  function self:echo_into_llua(src)
    t:into_llua(src .. "_pointee")
  end

  function self:echo()
    self.echo_argument = true
    return self
  end

  return self
end

function fun(return_type)
  return function(...)
    return {
      return_type = return_type,
      args = { ... }
    }
  end
end

bool = {}
function bool:declare_for_import(name)
  printf('int %s;', name)
end

function bool:declare_for_export(name)
  printf('int %s;', name)
end

function bool:into_c(dst, src, context)
  printf('%s = lua_toboolean(L, %d);', dst, src)
end

function bool:into_llua(src)
  printf('lua_pushboolean(L, !!%s);', src)
end

function generate_const_link(name, value, table, type)
  type:into_llua(value)
  printf('lua_setfield(L, %d, "%s");', table, name)
end

function generate_fun_code(name, fun)
  local num_echos = 0

  printf('static int invoke_%s(lua_State* L) {', name)

  for i = 1, #fun.args do
    fun.args[i]:declare_for_import("argument_" .. i)
  end
  if fun.return_type then
    fun.return_type:declare_for_export("ret")
  end

  for i = 1, #fun.args do
    fun.args[i]:into_c("argument_" .. i, i,
                       string.format("argument %d for function %s", i, name))
  end

  local call = ""
  if fun.return_type then
    call ..= "ret = "
  end
  call ..= name .. "("
  for i = 1, #fun.args do
    if i ~= 1 then
      call ..= ", "
    end
    call ..= "argument_" .. i
  end
  call ..= ");"
  print(call)

  if fun.return_type then
    fun.return_type:into_llua("ret")
  end

  for i = 1, #fun.args do
    if fun.args[i].echo_argument then
      num_echos++
      fun.args[i]:echo_into_llua("argument_" .. i)
    end
  end

  printf('return %d;', num_echos + (fun.return_type and 1 or 0))
  print('}')
end

function generate_top()
  printf('/* This file was generated by gen-bindings.lua.')
  printf('* Not intended for human consumption.')
  printf('*/')
  printf('#include <string.h>')
  printf('#include <stdlib.h>')
  printf('#include "../contrib/lua/lua.h"')
  printf('#include "../contrib/lua/lauxlib.h"')
  print(preamble)

  for name, fun in pairs(functions) do
    generate_fun_code(name, fun)
  end

  printf('static const luaL_Reg %s[] = {', module_name)
  for name in pairs(functions) do
    printf('{"%s", invoke_%s},', name, name)
  end

  printf('{NULL,NULL}')
  printf('};')

  printf('static int require_module_%s(lua_State* L) {', module_name)
  for name, type in pairs(constants) do
    type:declare_for_export(name .. "_value")
  end

  printf('luaL_newlib(L, %s);', module_name)
  -- newlib leaves the module table at the top of the stack, as required by
  -- module loading.
  for name, type in pairs(constants) do
    printf('%s_value = %s;', name, name)
    generate_const_link(name, name .. "_value", -2, type)
  end
  printf('return 1;')
  printf('}')

  printf('void open_module_%s(lua_State* L) {', module_name)
  printf('luaL_requiref(L, "%s", require_module_%s, 1);',
         module_name, module_name)
  printf('}')
end

require(arg[1])
generate_top()
