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
  graphic_blob = {},
  voxel_graphic = {},
  texdata = {},
  texpalette = {},
  palette = {},
  texture = {},
  voxel = {},
  ntvp = {},
}

local bit_extract = bit32.extract
local string_chars = string.char

--- Converts an alpha, red, green, blue triple into an ARGB integer.
function core.argb(a, r, g, b)
  return (a * 2^24) + (r * 2^16) + (g * 2^8) + b;
end

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

--- Converts an array of integers into a byte string.
--
-- The upper 56 bits of each integer are discarded.
function core.byte_array(tab)
  return string_chars(table.unpack(tab))
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

--- Front-end to mg.tg_mipmap_maximum
--
-- Passes through to mg.tg_mipmap_maximum after automatically determining the
-- current size.
function core.binary_mipmap_maximum(data)
  return mg.tg_mipmap_maximum(mg.isqrt(#data/3), data)
end

--- Creates a texpalette description from a table.
--
-- Convenience function for allocating a new palette and loading it with the
-- texture defined by the given two-dimensional ARGB table.
--
-- @param tab A table suitable for core.rgba_string_from_argb_table()
-- @return A table describing the converted palette.
function core.new_texpalette(tab)
  return {
    data = core.rgba_string_from_argb_table(tab),
    ncolours = #tab[1],
    ntimes = #tab,
  }
end

--- Creates mipmapped texture data from a table.
--
-- Convenience function for allocating a new texture and loading it with the
-- data from the given two-dimensional RGB table.
--
-- @param tab An ARGB table with the exact dimensions of 64x64.
-- @param mipmap A function which takes a two-dimensional table and returns
-- a new two-dimensional table whose dimensions are half those of the input,
-- and still compatible with core.rgb_string_from_argb_table().
-- @param is_raw If true, input textures are assumed to already be byte arrays.
-- @return An array containing the mipmapped texture data.
function core.new_texdata(t64, mipmap, is_raw)
  local t32 = mipmap(t64)
  local t16 = mipmap(t32)
  local t8  = mipmap(t16)
  local t4  = mipmap(t8)
  local t2  = mipmap(t4)
  local t1  = mipmap(t2)

  local cvt
  if is_raw then
    cvt = function(x) return x end
  else
    cvt = core.rgb_string_from_argb_table
  end

  return { cvt(t64), cvt(t32), cvt(t16), cvt(t8), cvt(t4), cvt(t2), cvt(t1) }
end

--- Constructs and loads a new texture object.
--
-- Parameters are passed in a single table for readability. The relevant
-- parameters are:
--
-- - texdata. Specifies a key to resources.texdata from which to obtain the
--   texture data (see core.new_texdata()).
-- - palette. Specifies a key to resources.texpalettes from which to obain the
--   palette data (see core.new_texpalette()).
function core.new_texture(parms)
  local texture = mg.rl_texture_new()
  local td = resource.texdata[parms.texdata]()
  local pd = resource.texpalette[parms.palette]()

  mg.rl_texture_load64x64rgbmm_NxMrgba(
    texture,
    td[1], td[2], td[3],
    td[4], td[5], td[6],
    td[7],
    pd.ncolours, pd.ntimes, pd.data)

  return texture
end

--- Constructs and loads a new palette object.
--
-- @param data A rectangular table containing RGBA pixel values.
-- @return The new palette object.
function core.new_palette(data)
  local p = mg.rl_palette_new()
  mg.rl_palette_loadMxNrgba(p, #data[1], #data,
                            core.rgba_string_from_argb_table(data))
  return p
end

--- Creates a new graphic plane with the specified properties.
--
-- Parameters are passed in in a single table for readability. The relevant
-- parameters are:
--
-- - texture. Specifies a key to resource.texture from which to obtain the
--   texture to use with the graphic plane.
--
-- @param parms A table of parameters, as described above.
-- @return The new graphic plane.
function core.new_graphic_plane(parms)
  local g = mg.rl_graphic_plane_new()
  mg.rl_graphic_plane_set_texture(g, resource.texture[parms.texture]())
  return g
end

--- Creates a new graphic plane with the specified properties.
--
-- Parameters are passed in a single table for readability The relevant
-- parameters are:
--
-- - palette. Specifies a key to resource.palette from which to obtain the
--   palette for use with this blob.
--
-- - noise. Optional. Table containing keys bias, amp, xfreq, yfreq, indicating
--   the noise configuration for this blob.
--
-- - perturbation. Optional. Integer indicating perturbation to set on the blob.
--
-- @param parms A table of parameters, as described above.
-- @return The new graphic blob.
function core.new_graphic_blob(parms)
  local b = mg.rl_graphic_blob_new()
  mg.rl_graphic_blob_set_palette(b, resource.palette[parms.palette]())
  if parms.noise then
    mg.rl_graphic_blob_set_noise(b, parms.noise.bias, parms.noise.amp,
                                 parms.noise.xfreq, parms.noise.yfreq)
  end
  if parms.perturbation then
    mg.rl_graphic_blob_set_perturbation(b, parms.perturbation)
  end
  return b
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
-- - blob. Optional. If present, specifies a key to resource.graphic_blob from
--   which to obtain the graphic blob to use with this graphic.
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
  if parms.blob then
    mg.rl_voxel_graphic_set_blob(g, resource.graphic_blob[parms.blob]())
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

--- Modulo function for Llua tables.
--
-- Since Llua tables are 1-indexed, normal modulus doesn't fully correctly
-- handle wrapping. This function returns (n%m), except that it returns m if
-- the result would be zero.
function core.mod1(n, m)
  n = n % m
  return 0 ~= n and n or m
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

--- Generates a two-dimensional table by mapping lanes trough a function.
--
-- Input is a table with three keys:
-- x: A table mapping names to parallel lists of values.
-- y: A table mapping names to parallel lists of values.
-- fun: The mapping function.
--
-- The resulting table has an X dimension of the number of items in the lists
-- in parms.x and a Y dimension of the number of items in the lists in parms.y.
-- Each element is generated by calling parms.fun with a table for each
-- coordinate; this table has the contents of parms.x and parms.y flattened
-- together, with an element from each list chosen according to the position in
-- the table. Additionally, fields named x and y are added to this table,
-- indicating the position of the particular pixel being generated.
function core.gen_2d_table_from_lanes(parms)
  local nx, ny = 0, 0

  for _, l in pairs(parms.x) do
    nx = #l
    break
  end
  for _, l in pairs(parms.y) do
    ny = #l
    break
  end

  local result = {}
  for y = 1, ny do
    local row = {}
    for x = 1, nx do
      local p = {}
      for k, l in pairs(parms.y) do
        p[k] = l[y]
      end
      for k, l in pairs(parms.x) do
        p[k] = l[x]
      end

      p.x = x
      p.y = y

      row[x] = parms.fun(p)
    end

    result[y] = row
  end

  return result
end

--- Generates a 2d table and creates a palette from it.
--
-- The given input is passed to gen_2d_table_from_lanes(), and the result into
-- new_palette().
function core.gen_palette_from_lanes(parms)
  return core.new_palette(core.gen_2d_table_from_lanes(parms))
end

--- Generates a 2d table and creates a texpalette from it.
--
-- The given input is passed to gen_2d_table_from_lanes(), and the result into
-- new_texpalette().
function core.gen_texpalette_from_lanes(parms)
  return core.new_texpalette(core.gen_2d_table_from_lanes(parms))
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
