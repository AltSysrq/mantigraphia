---
-- Copyright (c) 2015 Jason Lingle
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

-- core module: Common support functions needed by most modules, and
-- abstraction away from the very low-level interface provided by the resource
-- loader.

core = {}

--- Tables of resource exports.
--
-- The conventional way for modules to define resources is to define functions
-- within each sub-table which allocate the resource and return the resource's
-- index, eg,
--
--   function resource.graphic_plane.mymodule_myplane()
--     local v = mg.rl_graphic_plane_new()
--     -- configure v
--     return v
--   end
--
-- When resources are to be loaded, the functions in every sub-table are
-- memoised, and then the root types (eg, voxels) are forced.
resource = {
  graphic_plane = {},
  voxel_graphic = {},
  texture = {},
  palette = {},
  voxel = {},
  ntvp = {},
}

local bit_extract = bit32.extract
local string_chars = string.char

--- Converts an ARGB table to a RGBA string.
--
-- Converts a two-dimensional table of ARGB integers into a byte string in
-- RGBA format.
--
-- Example:
-- ```
--   local data = core.rgba_string_from_argb_table {
--     -- 2x2 pixels: red, green, blue, transparent
--     --  AARRGGBB    AARRGGBB
--     { 0xFFFF0000, 0xFF00FF00 },
--     { 0xFF0000FF, 0x00000000 },
--   }
-- ```
--
-- @param tab An assumed rectangular two-dimnensional table of integers in ARGB
-- format.
-- @return A string of length (rows*columns*4) in RGBA format.
function core.rgba_string_from_argb_table(tab)
  local rows = #tab
  local cols = #tab[1]
  local data = ""
  for y = 1, rows do
    local row = tab[y]
    for x = 1, cols do
      local argb = row[x]
      data ..= string_chars(bit_extract(argb, 16, 8),
                            bit_extract(argb,  8, 8),
                            bit_extract(argb,  0, 8),
                            bit_extract(argb, 24, 8))
    end
  end

  return data
end

--- Converts an ARGB table to an RGB string.
--
-- Converts a two-dimensional table of ARGB integers into a byte string in RGB
-- format.
--
-- Example:
-- ```
--   local data = core.rgb_string_from_argb_table {
--     -- 2x2 pixels: red, green, blue, black (because alpha is meaningless
--     -- in this call)
--     --  --RRGGBB    --RRGGBB
--     { 0xFFFF0000, 0xFF00FF00 },
--     { 0xFF0000FF, 0x00000000 },
--   }
-- ```
--
-- @param tab An assumed rectangular two-dimensional table of integers in ARGB
-- format.
-- @return A string of length (rows*columns*3) in RGB format.
function core.rgb_string_from_argb_table(tab)
  local rows = #tab
  local cols = #tab[1]
  local data = ""
  for y = 1, rows do
    local row = tab[y]
    for x = 1, cols do
      local argb = row[x]
      data ..= string_chars(bit_extract(argb, 16, 8),
                            bit_extract(argb,  8, 8),
                            bit_extract(argb,  0, 8))
    end
  end

  return data
end

--- Produces a two-dimensional table containing the given value in all cells.
--
-- @param rows The number of rows for the table (ie, first index)
-- @param cols The number of columns for the table (ie, second index)
-- @param value The value that will be placed into every cell of the table.
-- @return A rectangular two-dimensional table whose indices range from
-- output[1][1] to output[rows][cols], and contains value at every index.
function core.table2d(rows, cols, value)
  local result = {}
  for y = 1, rows do
    local row = {}
    for x = 1, cols do
      row[x] = value
    end
    result[y] = row
  end

  return result
end

--- Produces a two-dimensional table containing generated values.
--
-- @param rows The number of rows for the table (ie, first index)
-- @param cols The number of columns for the table (ie, second index)
-- @param fun A function which will be called for each cell, given its (x,y)
-- coordinates.
-- @return A rectangular two-dimensional table whose indices range from
-- output[1][1] to output[rows][cols], and contains value at every index.
function core.gentable2d(rows, cols, fun)
  local result = {}
  for y = 1, rows do
    local row = {}
    for x = 1, cols do
      row[x] = fun(x, y)
    end
    result[y] = row
  end

  return result
end

--- Mipmaps a table using the "nearest" algorithm.
--
-- Produces a mipmapped version of the input table by simply dropping
-- intermediate values.
--
-- Example:
-- ```
-- local m = core.mipmap_nearest {
--   {  1,  2,  3,  4 },
--   {  5,  6,  7,  8 },
--   {  9, 10, 11, 12 },
--   { 13, 14, 15, 16 },
-- }
-- m == { { 1, 3 }, { 9, 11 } }
-- ```
--
-- @param An assumed rectangular two-dimensional table whose dimensions should
-- be even numbers.
-- @return A two-dimensional table whose dimensions are half those of the input
-- table's, where output[y][x] == input[y*2][x*2].
function core.mipmap_nearest(tab)
  local rows = #tab / 2
  local cols = #tab[1] / 2
  local result = {}
  for y = 1, rows do
    local inrow = tab[y*2]
    local outrow = {}
    for x = 1, cols do
      outrow[x] = inrow[x*2]
    end
    result[y] = outrow
  end

  return result
end

--- Creates and loads a palette from a table.
--
-- Convenience function for allocating a new palette and loading it with the
-- texture defined by the given two-dimensional ARGB table.
--
-- @param tab A table suitable for core.rgba_string_from_argb_table()
-- @return The index of the new palette object.
function core.new_palette(tab)
  local palette = mg.rl_palette_new()
  mg.rl_palette_loadNxMrgba(palette, #tab[1], #tab,
                            core.rgba_string_from_argb_table(tab))
  return palette
end

--- Creates and loads a mipmapped texture from a table.
--
-- Convenience function for allocating a new texture and loading it with the
-- data from the given two-dimensional RGB table.
--
-- @param tab An ARGB table with the exact dimensions of 64x64.
-- @param mipmap A function which takes a two-dimensional table and returns
-- a new two-dimensional table whose dimensions are half those of the input,
-- and still compatible with core.rgb_string_from_argb_table().
-- @return The index of the new texture object.
function core.new_texture(t64, mipmap)
  local texture = mg.rl_texture_new()
  local t32 = mipmap(t64)
  local t16 = mipmap(t32)
  local t8  = mipmap(t16)
  local t4  = mipmap(t8)
  local t2  = mipmap(t4)
  local t1  = mipmap(t2)

  mg.rl_texture_load64x64rgb(texture,
                             core.rgb_string_from_argb_table(t64),
                             core.rgb_string_from_argb_table(t32),
                             core.rgb_string_from_argb_table(t16),
                             core.rgb_string_from_argb_table(t8),
                             core.rgb_string_from_argb_table(t4),
                             core.rgb_string_from_argb_table(t2),
                             core.rgb_string_from_argb_table(t1))
  return texture
end

--- Creates a new graphic plane with the specified properties.
--
-- Parameters are passed in in a single table for readability. The relevant
-- parameters are:
--
-- - texture. Specifies a key to resource.texture from which to obtain the
--   texture to use with the graphic plane.
--
-- - palette. Specifies a key to resource.palette from which to obtain the
--   palette to use with the graphic plane.
--
-- @parm parms A table of parameters, as described above.
-- @return The new graphic plane.
function core.new_graphic_plane(parms)
  local g = mg.rl_graphic_plane_new()
  mg.rl_graphic_plane_set_texture(g, resource.texture[parms.texture]())
  mg.rl_graphic_plane_set_palette(g, resource.palette[parms.palette]())
  return g
end

--- Creates a new voxel graphic with the specified properties.
--
-- Parameters are passed in a single table for readability. The relevant
-- parameters are:
--
-- - x, y, z. Optional. If present, specify a key to resource.graphic_plane
--   from which to obtain the graphic plane to use with each plane of the
--   graphic.
--
-- @param parms A table of parameters, as described above.
-- @return The new voxel graphic.
function core.new_voxel_graphic(parms)
  local g = mg.rl_voxel_graphic_new()
  if parms.x then
    mg.rl_voxel_graphic_set_plane(g, 0, resource.graphic_plane[parms.x]())
  end
  if parms.y then
    mg.rl_voxel_graphic_set_plane(g, 1, resource.graphic_plane[parms.y]())
  end
  if parms.z then
    mg.rl_voxel_graphic_set_plane(g, 2, resource.graphic_plane[parms.z]())
  end
  return g
end

--- Memoises the given no-argument function.
--
-- Given a no-argument function, returns a new no-argument function. When
-- invoked, it invokes the original function and returns its value. Further
-- invocations return the same value without re-invoking the original function.
function core.memoise(fun)
  local value = nil
  return function()
    if fun then
      value = fun()
      fun = nil
    end
    return value
  end
end

--- Binds a function to its arguments without calling it.
--
-- Takes a function of any arity and returns a new function of the same arity.
-- Calling that function returns a zero-argument function. Calling that
-- function in turn will invoke the original function with the arguments passed
-- into the second.
--
-- This is mainly useful for declaring resources that can be produced with a
-- single function call, eg
--
--   resource.palette.mymodule_mypalette = core.bind(core.new_palette) { ... }
function core.bind(fun)
  return function(...)
    --- ... isn't really lexically scoped, apparently; it's not visible in the
    --- inner function, so we need to pack it into a variable, sadly.
    local args = table.pack(...)
    return function()
      return fun(table.unpack(args))
    end
  end
end


--- Produces a chaotic value from the given integer sequence.
--
-- Uses mg.chaos_of() and mg.chaos_accum() to produce an essentially random
-- 32-bit value determined from the given inputs.
function core.chaos(...)
  local args = table.pack(...)
  local accum = 0
  for i = 1, #args do
    accum = mg.chaos_accum(accum, args[i])
  end

  return mg.chaos_of(accum)
end

--- Generates an NTVP by observing the behaviour of a set of functions.
--
-- Idiomatic usage is
--
--   local _ENV = core.ntvp_compiler()
--   local nfa = {}
--   -- Define functions in nfa
--   return compile(nfa, nfa.<initial-state>, <some-arguments>)
--
-- Each state in the NFA is inferred by the identity of one of the functions in
-- the nfa map and its (numeric) arguments. Transitions are added by calling
--
--   go(dx, dy, dz, self.<new-state>, <arguments>)
--
-- within one of the functions. Branches of size 1 are similarly added with
--
--   branch(self.<new-state>, <arguments>)
--
-- Voxel replacement is specified with
--
--   put_voxel(from_type, to_type)
--
-- Note that it is important to keep the entropy of arguments reasonably small,
-- since there are only 256 states to go around. All arguments to functions (if
-- they take any) must be numeric.
function core.ntvp_compiler()
  local nfa
  local states = {}
  local compiler = {}
  local that
  local next_state = 0
  local current_state

  setmetatable(compiler, { __index = _ENV })

  local function get_state(fun, ...)
    local strkey = table.concat(table.pack(...), ',')
    if not states[fun] or not states[fun][strkey] then
      if 256 == next_state then
        error("Exceeded limit of 256 states for nfa")
      end

      if not states[fun] then
        states[fun] = {}
      end
      states[fun][strkey] = next_state
      next_state++

      local prev_state = current_state
      current_state = next_state-1
      fun(that, ...)
      current_state = prev_state
    end

    return states[fun][strkey]
  end

  function compiler.go(dx, dy, dz, fun, ...)
    mg.ntvp_transition(nfa, current_state, get_state(fun, ...),
                       dx, dy, dz)
  end

  function compiler.fork(fun, ...)
    mg.ntvp_branch(nfa, current_state, get_state(fun, ...), 1)
  end

  function compiler.compile(t, fun, ...)
    nfa = mg.ntvp_new()
    that = t
    get_state(fun, ...)
    return nfa
  end

  function compiler.put_voxel(from_type, to_type)
    mg.ntvp_put_voxel(nfa, current_state, from_type, to_type)
  end

  return compiler
end

--- The global resoure loading trigger.
--
-- Called by the application when it wishes to load resources. This call is the
-- only time during with the mg.rl_* funcctions will work.
--
-- The default implementation is documented with the `resource` global.
function load_resources()
  -- Memoise all the subtables
  for group, members in pairs(resource) do
    for name, fun in pairs(members) do
      members[name] = core.memoise(fun)
    end
  end

  -- Force the roots
  local function force_roots(members)
    for name, fun in pairs(members) do
      fun()
    end
  end
  force_roots(resource.voxel)
end
