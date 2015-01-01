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
