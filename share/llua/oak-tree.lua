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
  { 0x00ffffff, 0xffffffff, 0xff003000, 0xffffffff,
    0xff144010, 0xff144010, 0xff103408, 0xff103408, },
  -- A
  { 0x00ffffff, 0xffffffff, 0xff003000, 0xff003000,
    0xff144010, 0xff144010, 0xff103408, 0xff103408, },
  -- M
  { 0x00003000, 0xff002000, 0xff004800, 0xff003000,
    0xff1e6018, 0xff144010, 0xff184e0c, 0xff103408, },
  -- J
  { 0x00003000, 0xff003000, 0xff004800, 0xff004800,
    0xff1e6018, 0xff1e6018, 0xff184e0c, 0xff184e0c, },
  -- J
  { 0x00003000, 0xff002000, 0xff004800, 0xff003000,
    0xff1e6018, 0xff144010, 0xff184e0c, 0xff103408, },
  -- A
  { 0x00002000, 0xff002000, 0xff003000, 0xff003000,
    0xff144010, 0xff144010, 0xff103408, 0xff103408, },
  -- S
  { 0x00002000, 0xff002000, 0xff003000, 0xff003000,
    0xff144010, 0xff144010, 0xff103408, 0xff103408, },
  -- O
  { 0x00a00000, 0xffa00000, 0xffa00000, 0xffa02000,
    0xffa04000, 0xffa06000, 0xffa08000, 0xffa0a000, },
  -- N
  { 0x00ffffff, 0xffffffff, 0xffffffff, 0xffffffff,
    0xff704020, 0xff705030, 0xff706040, 0xff707050, },
  -- D
  { 0x00ffffff, 0xffffffff, 0xffffffff, 0xffffffff,
    0xffffffff, 0xffffffff, 0xffffffff, 0xffffffff, },
}

resource.palette.oaktree_trunk = core.bind(core.new_palette) {
  -- M
  { 0x00ffffff, 0xffffffff, 0xffffffff, 0xff302020, 0xff636357,
    0xff303020, 0xff302800, 0xff303020, 0xff302000, 0xff303020, },
  -- A
  { 0x00000000, 0xff000000, 0xff201c00, 0xff303020, 0xff302000,
    0xff303020, 0xff302800, 0xff303020, 0xff302000, 0xff303020, },
  -- M
  { 0x00000000, 0xff000000, 0xff201c00, 0xff303020, 0xff302000,
    0xff303020, 0xff302800, 0xff303020, 0xff302000, 0xff303020, },
  -- J
  { 0x00000000, 0xff000000, 0xff201c00, 0xff303020, 0xff302000,
    0xff303020, 0xff302800, 0xff303020, 0xff302000, 0xff303020, },
  -- J
  { 0x00000000, 0xff000000, 0xff201c00, 0xff303020, 0xff302000,
    0xff303020, 0xff302800, 0xff303020, 0xff302000, 0xff303020, },
  -- A
  { 0x00000000, 0xff000000, 0xff201c00, 0xff303020, 0xff302000,
    0xff303020, 0xff302800, 0xff303020, 0xff302000, 0xff303020, },
  -- S
  { 0x00000000, 0xff000000, 0xff201c00, 0xff303020, 0xff302000,
    0xff303020, 0xff302800, 0xff303020, 0xff302000, 0xff303020, },
  -- O
  { 0x00000000, 0xff000000, 0xff201c00, 0xff303020, 0xff302000,
    0xff303020, 0xff302800, 0xff303020, 0xff302000, 0xff303020, },
  -- N
  { 0x00ffffff, 0xffffffff, 0xffffffff, 0xff302020, 0xff636357,
    0xff303020, 0xff302800, 0xff303020, 0xff302000, 0xff303020, },
  -- D
  { 0x00ffffff, 0xffffffff, 0xffffffff, 0xff302020, 0xffffffff,
    0xff303020, 0xff302800, 0xff303020, 0xff302000, 0xff303020, },
}

-- TODO: Real textures instead of noise
function resource.texture.oaktree_trunk()
  local rnd = 42
  return core.new_texture(
    mg.tg_zip(mg.tg_uniform_noise(core.byte_array { 64, 128, 224 }, 1),
              mg.tg_uniform_noise(core.byte_array { 255 }, 3),
              mg.tg_uniform_noise(nil, 2)),
    core.binary_mipmap_maximum, true)
end

function oaktree_generate_leaf_texture(bias)
  local rnd = 751
  local zero = mg.tg_fill(0)
  local worley = zero
  for i = 1, 32 do
    local sx, sy
    sx, rnd = mg.lcgrand(rnd)
    sy, rnd = mg.lcgrand(rnd)
    worley = mg.tg_max(
      worley, mg.tg_similarity(sx % 64, sy % 64, zero, 0))
  end

  worley = mg.tg_normalise(worley, 255-32, 255-16)

  local sphere = mg.tg_similarity(32, 32, zero, 0)
  local colour = mg.tg_normalise(worley, bias, bias + 32)
  local clipped = mg.tg_stencil(zero, colour, worley,
                                mg.tg_fill(255-32), sphere)

  return core.new_texture(
    mg.tg_zip(
      clipped,
      mg.tg_uniform_noise(core.byte_array { 255, 220, 200, 197 }, rnd),
      mg.tg_uniform_noise(nil, rnd+1)),
    core.binary_mipmap_maximum, true)
end

resource.graphic_plane.oaktree_trunk = core.bind(core.new_graphic_plane) {
  texture = "oaktree_trunk",
  palette = "oaktree_trunk",
}

resource.voxel_graphic.oaktree_trunk = core.bind(core.new_voxel_graphic) {
  x = "oaktree_trunk",
  z = "oaktree_trunk",
}

for i = 1, 4 do
  resource.texture["oaktree_leaf"..i] =
    core.bind(oaktree_generate_leaf_texture)(32 + i*32)

  resource.graphic_plane["oaktree_leaf"..i] = core.bind(core.new_graphic_plane) {
    texture = "oaktree_leaf" .. i,
    palette = "oaktree_leaf",
  }

  resource.voxel_graphic["oaktree_leaf"..i] = core.bind(core.new_voxel_graphic) {
    x = "oaktree_leaf" .. i,
    y = "oaktree_leaf" .. i,
    z = "oaktree_leaf" .. i,
  }

  resource.voxel["oaktree_leaf"..i] = function()
    local v = mg.rl_voxel_type_new()
    mg.rl_voxel_set_voxel_graphic(
      v * (2^mg.ENV_VOXEL_CONTEXT_BITS),
      resource.voxel_graphic["oaktree_leaf"..i]())
    return v
  end
end

function resource.voxel.oaktree_trunk()
  local v = mg.rl_voxel_type_new()
  mg.rl_voxel_set_voxel_graphic(
    v * (2^mg.ENV_VOXEL_CONTEXT_BITS),
    resource.voxel_graphic.oaktree_trunk())
  return v
end

function oaktree_gen_ntvp(leafkey)
  local nfa = {}
  local _ENV = core.ntvp_compiler()

  local directions = {
    { 1, 0 }, { -1, 0 },
    { 0, 1 }, { 0, -1 },
    { 1, 1 }, { -1, 1 },
    { 1, -1 }, { -1, -1 },
  }

  local half_directions = {
    { 0, 0 }, { 0, 1 }, { 1, 0 }, { 1, 1 },
  }

  function nfa:terminate()
  end

  function nfa:init_state()
    -- 7/8 of trees are normal size...
    for i = 1, 7 do
      go(0, 0, 0, self.init_state, 1)
    end

    -- 1/8 of trees might be larger
    go(0, 0, 0, self.larger_init_state)
  end

  function nfa:larger_init_state()
    -- 63/64 trees are double size
    for i = 1, 7 do
      go(0, 0, 0, self.start, 1)
    end

    -- 1/64 trees are double size
    go(0, 0, 0, self.double_init_state, 1)
  end

  function nfa:double_init_state(direction)
    if direction < 4 then
      fork(self.double_init_state, direction + 1)
    end

    go(half_directions[direction][1], 0, half_directions[direction][2],
       self.start, 2)
  end

  function nfa:start(m)
    go(0, 0, 0, self.base_trunk, 3*m, 3*m)
    go(0, 0, 0, self.base_trunk, 4*m, 3*m)
    go(0, 0, 0, self.base_trunk, 4*m, 4*m)
    go(0, 0, 0, self.base_trunk, 5*m, 4*m)
    go(0, 0, 0, self.base_trunk, 6*m, 5*m)
  end

  function nfa:base_trunk(segments_left, radius)
    put_voxel(0, resource.voxel.oaktree_trunk())

    if segments_left > 1 then
      for i = 1, 7 do
        go(0, 1, 0, self.base_trunk, segments_left - 1, radius)
      end
      go(0, 0, 0, self.crooked_trunk, segments_left, radius)
    else
      go(0, 1, 0, self.branching_trunk, radius/2 + 1, 1, radius)
    end
  end

  function nfa:crooked_trunk(segments_left, radius)
    go( 1, 0,  0, self.base_trunk, segments_left, radius)
    go(-1, 0,  0, self.base_trunk, segments_left, radius)
    go( 0, 0,  1, self.base_trunk, segments_left, radius)
    go( 0, 0, -1, self.base_trunk, segments_left, radius)
  end

  function nfa:branching_trunk(segments_left, may_branch, radius)
    put_voxel(0, resource.voxel.oaktree_trunk())
    if 1 == may_branch then
      fork(self.spawn_branches, radius, 1)
    end

    if segments_left > 1 then
      go(0, 1, 0, self.branching_trunk, segments_left - 1,
            0 == may_branch and 1 or 0, radius - 1)
    else
      go(0, 1, 0, self.cluster_centre, 1)
    end
  end

  function nfa:cluster_centre(direction)
    put_voxel(0, resource.voxel[leafkey]())
    if direction < #directions then
      fork(self.cluster_centre, direction + 1)
    end

    go(directions[direction][1], 0, directions[direction][2],
       self.leaf)
  end

  function nfa:leaf(direction)
    put_voxel(0, resource.voxel[leafkey]())
  end

  function nfa:spawn_branches(radius, direction)
    if direction < 4 then
      fork(self.spawn_branches, radius, direction + 1)
    end

    go(directions[direction][1], 0, directions[direction][2],
       self.branch, radius, direction)
  end

  function nfa:branch(segments_left, direction)
    put_voxel(0, resource.voxel.oaktree_trunk())

    if segments_left > 1 then
      fork(self.continue_branch, segments_left - 1, direction)
    else
      fork(self.cluster_centre, 1)
    end

    go(0, 1, 0, self.cluster_centre, 1)
    go(0, 1, 0, self.cluster_centre, 1)
    go(0, -1, 0, self.leaf)
    if segments_left > 1 then
      local d2 = core.mod1(direction+1, 4)
      go(directions[d2][1], 0, directions[d2][2],
         self.branch, segments_left - 1, d2)
      d2 = core.mod1(direction-1, 4)
      go(directions[d2][1], 0, directions[d2][2],
         self.branch, segments_left - 1, d2)
    end
  end

  function nfa:continue_branch(segments_left, direction)
    fork(self.cluster_centre, 1)
    go(directions[direction][1], 0, directions[direction][2],
       self.branch, segments_left, direction)
  end

  local n = compile(nfa, nfa.init_state)
  mg.ntvp_visibility(n, resource.voxel.oaktree_trunk(), 1, 1, 1, 1)
  mg.ntvp_visibility(n, resource.voxel[leafkey](), 1, 1, 2, 3)
  return n
end

for i = 1, 4 do
  resource.ntvp["oaktree"..i] =
    core.bind(oaktree_gen_ntvp)("oaktree_leaf"..i)
end
