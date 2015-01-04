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

-- TODO: These palettes need a lot of work, since they originally passed
-- through a rendering system that distorted the colours a lot.
resource.palette.oaktree_leaf = core.bind(core.new_palette) {
  -- M
  { 0xffffffff, 0xffffffff, 0xff003000, 0xffffffff,
    0xff144010, 0xff144010, 0xff103408, 0xff103408, },
  -- A
  { 0xffffffff, 0xffffffff, 0xff003000, 0xff003000,
    0xff144010, 0xff144010, 0xff103408, 0xff103408, },
  -- M
  { 0xff003000, 0xff002000, 0xff004800, 0xff003000,
    0xff1e6018, 0xff144010, 0xff184e0c, 0xff103408, },
  -- J
  { 0xff003000, 0xff003000, 0xff004800, 0xff004800,
    0xff1e6018, 0xff1e6018, 0xff184e0c, 0xff184e0c, },
  -- J
  { 0xff003000, 0xff002000, 0xff004800, 0xff003000,
    0xff1e6018, 0xff144010, 0xff184e0c, 0xff103408, },
  -- A
  { 0xff002000, 0xff002000, 0xff003000, 0xff003000,
    0xff144010, 0xff144010, 0xff103408, 0xff103408, },
  -- S
  { 0xff002000, 0xff002000, 0xff003000, 0xff003000,
    0xff144010, 0xff144010, 0xff103408, 0xff103408, },
  -- O
  { 0xffa00000, 0xffa00000, 0xffa00000, 0xffa02000,
    0xffa04000, 0xffa06000, 0xffa08000, 0xffa0a000, },
  -- N
  { 0xffffffff, 0xffffffff, 0xffffffff, 0xffffffff,
    0xff704020, 0xff705030, 0xff706040, 0xff707050, },
  -- D
  { 0xffffffff, 0xffffffff, 0xffffffff, 0xffffffff,
    0xffffffff, 0xffffffff, 0xffffffff, 0xffffffff, },
}

resource.palette.oaktree_trunk = core.bind(core.new_palette) {
  -- M
  { 0xffffffff, 0xffffffff, 0xffffffff, 0xff302020, 0xff636357,
    0xff303020, 0xff302800, 0xff303020, 0xff302000, 0xff303020, },
  -- A
  { 0xff000000, 0xff000000, 0xff201c00, 0xff303020, 0xff302000,
    0xff303020, 0xff302800, 0xff303020, 0xff302000, 0xff303020, },
  -- M
  { 0xff000000, 0xff000000, 0xff201c00, 0xff303020, 0xff302000,
    0xff303020, 0xff302800, 0xff303020, 0xff302000, 0xff303020, },
  -- J
  { 0xff000000, 0xff000000, 0xff201c00, 0xff303020, 0xff302000,
    0xff303020, 0xff302800, 0xff303020, 0xff302000, 0xff303020, },
  -- J
  { 0xff000000, 0xff000000, 0xff201c00, 0xff303020, 0xff302000,
    0xff303020, 0xff302800, 0xff303020, 0xff302000, 0xff303020, },
  -- A
  { 0xff000000, 0xff000000, 0xff201c00, 0xff303020, 0xff302000,
    0xff303020, 0xff302800, 0xff303020, 0xff302000, 0xff303020, },
  -- S
  { 0xff000000, 0xff000000, 0xff201c00, 0xff303020, 0xff302000,
    0xff303020, 0xff302800, 0xff303020, 0xff302000, 0xff303020, },
  -- O
  { 0xff000000, 0xff000000, 0xff201c00, 0xff303020, 0xff302000,
    0xff303020, 0xff302800, 0xff303020, 0xff302000, 0xff303020, },
  -- N
  { 0xffffffff, 0xffffffff, 0xffffffff, 0xff302020, 0xff636357,
    0xff303020, 0xff302800, 0xff303020, 0xff302000, 0xff303020, },
  -- D
  { 0xffffffff, 0xffffffff, 0xffffffff, 0xff302020, 0xffffffff,
    0xff303020, 0xff302800, 0xff303020, 0xff302000, 0xff303020, },
}

-- TODO: Real textures instead of noise
function resource.texture.oaktree_temp()
  local rnd = 42
  return core.new_texture(
    core.gentable2d(64, 64, function (x,y)
                      local r, b
                      r = core.chaos(x/2, y/2)
                      b = bit32.extract(r, 8, 8)
                      r = bit32.extract(r, 0, 8)
                      return bit32.bor(
                        bit32.lshift(r, 16),
                        0x00ff00, b)
    end), core.mipmap_nearest)
end

resource.graphic_plane.oaktree_trunk = core.bind(core.new_graphic_plane) {
  texture = "oaktree_temp",
  palette = "oaktree_trunk",
}

resource.graphic_plane.oaktree_leaf = core.bind(core.new_graphic_plane) {
  texture = "oaktree_temp",
  palette = "oaktree_leaf",
}

resource.voxel_graphic.oaktree_trunk = core.bind(core.new_voxel_graphic) {
  x = "oaktree_trunk",
  z = "oaktree_trunk",
}

resource.voxel_graphic.oaktree_leaf = core.bind(core.new_voxel_graphic) {
  x = "oaktree_leaf",
  y = "oaktree_leaf",
  z = "oaktree_leaf",
}

function resource.voxel.oaktree_trunk()
  local v = mg.rl_voxel_type_new()
  mg.rl_voxel_set_voxel_graphic(
    v * (2^mg.ENV_VOXEL_CONTEXT_BITS),
    resource.voxel_graphic.oaktree_trunk())
  return v
end

function resource.voxel.oaktree_leaf()
  local v = mg.rl_voxel_type_new()
  mg.rl_voxel_set_voxel_graphic(
    v * (2^mg.ENV_VOXEL_CONTEXT_BITS),
    resource.voxel_graphic.oaktree_leaf())
  return v
end

function resource.ntvp.oaktree()
  local nfa = {}
  local max_height = 8
  local directions = {
    { 1, 0, 0 }, { -1, 0, 0 },
    { 0, 0, 1 }, { 0, 0, -1 },
    { 0, 1, 0 }, { 0, -1, 0 },
  }
  local branch_lengths = {
    0, 0, 2, 4, 4,
    4, 4, 3, 1, 0,
  }

  local _ENV = core.ntvp_compiler()

  function nfa:terminate()
  end

  function nfa:trunk(y)
    put_voxel(0, resource.voxel.oaktree_trunk())

    if branch_lengths[y] > 0 then
      fork(self.spawn_branches, 1, branch_lengths[y])
    end

    if y < max_height - 1 then
      go(0, 1, 0, self.trunk, y + 1)
      go(0, 1, 0, self.trunk, y + 2)
    elseif y == max_height - 1 then
      go(0, 1, 0, self.trunk, y + 1)
      go(0, 0, 0, self.explode_leaves, 1)
    else
      go(0, 0, 0, self.explode_leaves, 1)
    end
  end

  function nfa:spawn_branches(direction, branch_length)
    if direction < 4 then
      fork(self.spawn_branches, direction+1, branch_length)
    end

    local dx, dy, dz = table.unpack(directions[direction])
    go(dx, dy, dz,
       self.branch, 0, branch_length,
       dx, dy, dz)
    if branch_length > 1 then
      go(dx, dy, dz,
         self.branch, 0, branch_length/2,
         dx, dy, dz)
    end
    go(0, 0, 0, self.terminate)
    go(0, 0, 0, self.terminate)
  end

  function nfa:branch(length, off, ...)
    put_voxel(0, resource.voxel.oaktree_trunk())

    local dx, dy, dz = ...
    if length > 2 then
      go(dx, dy, dz, self.branch, length - 1,
         off == 2 and 2 or off + 1,
         dx, dy, dz)
      go(dx, dy, dz, self.branch, length - 1,
         off == 2 and 2 or off + 1,
         dx, dy, dz)
    elseif 1 == length then
      go(dx, dy, dz, self.branch, length - 1,
         off == 2 and 2 or off + 1,
         dx, dy, dz)
      go(dx, dy, dz, self.explode_leaves, 1)
    else
      go(dx, dy, dz, self.explode_leaves, 1)
    end

    if 2 == off and length > 1 then
      fork(self.maybe_spawn_side_branch, ...)
    end
  end

  function nfa:maybe_spawn_side_branch(bx, by, bz)
    local nbx, nbz
    if 0 ~= bx then
      nbx = 0
      nbz = 1
    else
      nbx = 1
      nbz = 0
    end

    go(0,  1, 0, self.branch, 1, 0, 0,  1, 0)
    go(0, -1, 0, self.branch, 1, 0, 0, -1, 0)
    go(1*nbx, 0, 1*nbz, self.branch, 1, 0, 1*nbx, 0, 1*nbz)
    go(-1*nbx, 0, -1*nbz, self.branch, 1, 0, -1*nbx, 0, -1*nbz)
    for i = 1, 4 do
      go(0, 0, 0, self.terminate)
    end
  end

  function nfa:explode_leaves(direction)
    if direction < #directions then
      fork(self.explode_leaves, direction + 1)
    end

    local dx, dy, dz = table.unpack(directions[direction])
    go(dx, dy, dz,
       self.leaf_explosion_centre, (direction+1)/2, 1)
    go(0, 0, 0, self.terminate)
  end

  local edge_offsets = {
    { { 0, 1, 0 }, { 0, -1, 0 }, { 0, 0, 1 }, { 0, 0, -1 } },
    { { 1, 0, 0 }, { -1, 0, 0 }, { 0, 1, 0 }, { 0, -1, 0 } },
    { { 1, 0, 0 }, { -1, 0, 0 }, { 0, 0, 1 }, { 0, 0, -1 } },
  }
  local edge_axes = {
    { 3, 3, 2, 2 },
    { 2, 2, 1, 1 },
    { 3, 3, 1, 1 },
  }
  function nfa:leaf_explosion_centre(axis, off)
    put_voxel(0, resource.voxel.oaktree_leaf())
    if off < 4 then
      fork(self.leaf_explosion_centre, axis, off+1)
    end

    local dx, dy, dz = table.unpack(edge_offsets[axis][off])
    go(dx, dy, dz,
       self.leaf_explosion_edge,
       edge_axes[axis][off], 1)
    go(0, 0, 0, self.terminate)
    go(0, 0, 0, self.terminate)
  end

  local corner_offsets = {
    { { 1, 0, 0 }, { -1, 0, 0 } },
    { { 0, 1, 0 }, { 0, -1, 0 } },
    { { 0, 0, 1 }, { 0, 0, -1 } },
  }
  function nfa:leaf_explosion_edge(edge, off)
    put_voxel(0, resource.voxel.oaktree_leaf())
    if off < 2 then
      fork(self.leaf_explosion_edge, edge, off+1)
    end

    local dx, dy, dz = table.unpack(corner_offsets[edge][off])
    go(dx, dy, dz,
       self.leaf_explosion_corner)
    go(0, 0, 0, self.terminate)
    go(0, 0, 0, self.terminate)
    go(0, 0, 0, self.terminate)
  end

  function nfa:leaf_explosion_corner()
    put_voxel(0, resource.voxel.oaktree_leaf())
  end

  return compile(nfa, nfa.trunk, 1)
end
