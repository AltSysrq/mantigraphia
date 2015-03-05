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

function cherrytree_gen_leaf_valtex()
  local a, b
  a = mg.tg_perlin_noise(8, 100, 29541)
  b = mg.tg_perlin_noise(32, 75, 29542)

  return mg.tg_normalise(mg.tg_sum(a, b), 0, 255)
end

resource.valtex.cherrytree_leaf = core.compose(
  core.new_valtex, cherrytree_gen_leaf_valtex)

resource.palette.cherrytree_leaf = core.bind(core.gen_palette_from_lanes) {
  x = {
    heat = {    0,      1,      2,      1,      0,      1,      2,      0,      1,      2       },
    r = {       0.8,    0.5,    0.6,    0.7,    0.8,    0.9,    1.0,    0.5,    0.6,    0.7     },
    g = {       0.8,    0.6,    0.7,    0.9,    1.0,    0.8,    0.9,    0.7,    1.0,    0.6     },
    agm = {     0.2,    0.3,    0.1,    0.25,   0.35,   0.45,   0.15,   0.2,    0.3,    0.4     },
  },
  --            M       A       M       J       J       A       S       O       N       D
  y = {
    snow = {    2,      1,      0,      0,      0,      0,      0,      0,      1,      3       },
    rsat = {    0.1,    0.1,    1.0,    0.1,    0.1,    0.1,    0.2,    0.7,    0.1,    0.1     },
    gsat = {    0.4,    0.5,    0.7,    0.7,    0.7,    0.5,    0.4,    0.4,    0.2,    0.1     },
    bsat = {    0.2,    0.2,    0.9,    0.2,    0.15,   0.15,   0.1,    0.05,   0.05,   0.0     },
    autumn = {  false,  false,  false,  false,  false,  false,  false,  true,   false,  false   },
    bloom = {   false,  false,  true,   false,  false,  false,  false,  false,  false,  false   },
  },
  fun = function(p)
    local r, g, b

    if p.snow > p.heat then
      r = 255
      g = 255
      b = 255
    elseif p.autumn then
      r = 255 * p.r * p.rsat / 1.0 / 1.0
      g = r - 255 * p.agm / 1.0
      b = 255 * p.bsat / 1.0
    else
      r = 255 * p.r * p.rsat / 1.0 / 1.0
      g = 255 * p.g * p.gsat / 1.0 / 1.0
      b = 255 * p.r * p.bsat / 1.0 / 1.0
    end

    if p.bloom then
      if g > b then
        g = b
      end
    end

    local a = core.chaos(p.x, p.y) % 64 + 191
    return core.argb(a, r, g, b)
  end
}

for i = 1, 4 do
  resource.graphic_blob["cherrytree_leaf"..i] = core.bind(core.new_graphic_blob) {
    valtex = "cherrytree_leaf",
    palette = "cherrytree_leaf",
    noise = {
      bias = 0.2 * (i-1),
      amp = 0.3,
      xfreq = 0.75,
      yfreq = 0.75,
    },
    perturbation = 0.4
  }

  resource.voxel_graphic["cherrytree_leaf"..i] = core.bind(core.new_voxel_graphic) {
    blob = "cherrytree_leaf"..i
  }

  resource.voxel["cherrytree_leaf"..i] = core.bind(core.new_voxel_type) {
    graphic = "cherrytree_leaf"..i
  }
end

function cherrytree_gen_trunk_valtex()
  return mg.tg_normalise(mg.tg_perlin_noise(16, 256, 29543), 0, 255)
end
resource.valtex.cherrytree_trunk = core.compose(
  core.new_valtex, cherrytree_gen_trunk_valtex)

resource.palette.cherrytree_trunk = core.bind(core.new_palette) {
  -- M
  { 0x90ffffff, 0x90ffffff, 0x90ffffff, 0x90645c5c, 0x907e7e78,
    0x9064645c, 0x90302800, 0x9064645c, 0x90302000, 0x9064645c, },
  -- A
  { 0x9064645c, 0x90302800, 0x90201c00, 0x9064645c, 0x90302000,
    0x9064645c, 0x90302800, 0x9064645c, 0x90302000, 0x9064645c, },
  -- M
  { 0x9064645c, 0x90302800, 0x90201c00, 0x9064645c, 0x90302000,
    0x9064645c, 0x90302800, 0x9064645c, 0x90302000, 0x9064645c, },
  -- J
  { 0x9064645c, 0x90302800, 0x90201c00, 0x9064645c, 0x90302000,
    0x9064645c, 0x90302800, 0x9064645c, 0x90302000, 0x9064645c, },
  -- J
  { 0x9064645c, 0x90302800, 0x90201c00, 0x9064645c, 0x90302000,
    0x9064645c, 0x90302800, 0x9064645c, 0x90302000, 0x9064645c, },
  -- A
  { 0x9064645c, 0x90302800, 0x90201c00, 0x9064645c, 0x90302000,
    0x9064645c, 0x90302800, 0x9064645c, 0x90302000, 0x9064645c, },
  -- S
  { 0x9064645c, 0x90302800, 0x90201c00, 0x9064645c, 0x90302000,
    0x9064645c, 0x90302800, 0x9064645c, 0x90302000, 0x9064645c, },
  -- O
  { 0x9064645c, 0x90302800, 0x90201c00, 0x9064645c, 0x90302000,
    0x9064645c, 0x90302800, 0x9064645c, 0x90302000, 0x9064645c, },
  -- N
  { 0x90ffffff, 0x90ffffff, 0x90ffffff, 0x90645c5c, 0x907e7e78,
    0x9064645c, 0x90302800, 0x9064645c, 0x90302000, 0x9064645c, },
  -- D
  { 0x90ffffff, 0x90ffffff, 0x90ffffff, 0x90645c5c, 0x90ffffff,
    0x9064645c, 0x90302800, 0x9064645c, 0x90302000, 0x9064645c, },
}

resource.graphic_blob.cherrytree_trunk = core.bind(core.new_graphic_blob) {
  valtex = "cherrytree_trunk",
  palette = "cherrytree_trunk",
  noise = {
    bias = 0.0,
    amp = 1.0,
    xfreq = 0.5,
    yfreq = 0.5,
  },
  perturbation = 0.075
}

resource.voxel_graphic.cherrytree_trunk = core.bind(core.new_voxel_graphic) {
  blob = "cherrytree_trunk"
}

resource.voxel.cherrytree_trunk = core.bind(core.new_voxel_type) {
  graphic = "cherrytree_trunk"
}

function cherrytree_gen_ntvp(leafkey)
  local nfa = {}
  local _ENV = core.ntvp_compiler()

  local half_directions = {
    { 0, 0 }, { 0, 1 }, { 1, 0 }, { 1, 1 },
  }

  local branch_directions = {
    { 1, 0 }, { 1, 1 }, { 0, 1 },
    {-1, 0 }, {-1,-1 }, { 0,-1 },
    {-1, 1 }, { 1,-1 }, { 0, 0 },
  }

  local directions = {
    { 1, 0, 0 }, { -1,  0,  0 },
    { 0, 1, 0 }, {  0, -1,  0 },
    { 0, 0, 1 }, {  0,  0, -1 },
  }

  local leaf_directions = {
    { 1, 0, 0 }, {-1, 0, 0 },
    { 0, 1, 0 }, { 0,-1, 0 },
    { 0, 0, 1 }, { 0, 0,-1 },
    { 1, 1, 0 }, { 1,-1, 0 },
    { 1, 0, 1 }, { 1, 0,-1 },
    {-1, 1, 0 }, {-1,-1, 0 },
    {-1, 0, 1 }, {-1, 0, 1 },
    { 0, 1, 1 }, { 0, 1,-1 },
    { 0,-1, 1 }, { 0,-1,-1 },
    { 1, 1, 1 }, { 1, 1,-1 },
    { 1,-1, 1 }, { 1,-1,-1 },
    {-1, 1, 1 }, {-1, 1,-1 },
    {-1,-1, 1 }, {-1,-1,-1 },
  }

  function nfa:terminate()
  end

  function nfa:init_state()
    go(0, 0, 0, self.init_small)
    go(0, 0, 0, self.init_small)
    go(0, 0, 0, self.init_small)
    go(0, 0, 0, self.init_small)
    go(0, 0, 0, self.init_small)
    go(0, 0, 0, self.init_small)
    go(0, 0, 0, self.init_small)
    go(0, 0, 0, self.init_large_or_huge)
  end

  function nfa:init_large_or_huge()
    go(0, 0, 0, self.init_large)
    go(0, 0, 0, self.init_large)
    go(0, 0, 0, self.init_large)
    go(0, 0, 0, self.init_large)
    go(0, 0, 0, self.init_large)
    go(0, 0, 0, self.init_large)
    go(0, 0, 0, self.init_large)
    go(0, 0, 0, self.init_huge)
  end

  function nfa:init_small()
    go(0, 0, 0, self.start, 1, 3)
    go(0, 0, 0, self.start, 1, 4)
    go(0, 0, 0, self.start, 1, 5)
    go(0, 0, 0, self.start, 1, 6)
    go(0, 0, 0, self.start, 1, 6)
    go(0, 0, 0, self.start, 1, 6)
    go(0, 0, 0, self.start, 1, 7)
    go(0, 0, 0, self.start, 1, 7)
  end

  function nfa:init_large()
    go(0, 0, 0, self.start, 1, 8)
    go(0, 0, 0, self.start, 1, 8)
    go(0, 0, 0, self.start, 1, 9)
    go(0, 0, 0, self.start, 1, 9)
    go(0, 0, 0, self.start, 1, 10)
  end

  function nfa:init_huge()
    go(0, 0, 0, self.start, 1, 11)
    go(0, 0, 0, self.start, 1, 12)
    go(0, 0, 0, self.start, 1, 13)
    go(0, 0, 0, self.start, 1, 14)
    go(0, 0, 0, self.start, 1, 15)
    go(0, 0, 0, self.start, 1, 16)
  end

  function nfa:start(direction, height)
    if direction < #half_directions then
      fork(self.start, direction + 1, height)
    end

    go(half_directions[direction][1], 0, half_directions[direction][2],
       self.build_vertical_trunk, height)
  end

  function nfa:build_vertical_trunk(height)
    put_voxel(0, resource.voxel.cherrytree_trunk())
    if height > 0 then
      go(0, 1, 0, self.build_vertical_trunk, height-1)
    else
      go(0, 0, 0, self.start_branches, 1)
    end
  end

  function nfa:start_branches(direction)
    local base_length
    if direction == #branch_directions then
      base_length = 6
    else
      base_length = 3
    end
    go(0, 0, 0, self.terminate)
    go(0, 0, 0, self.branch, direction, base_length, 0)
    go(0, 0, 0, self.terminate)
    go(0, 0, 0, self.branch, direction, base_length+1, 0)

    if direction < #branch_directions then
      fork(self.start_branches, direction + 1)
    end
  end

  function nfa:branch(direction, length, odd)
    put_voxel(0, resource.voxel.cherrytree_trunk())
    if length > 0 then
      go(branch_directions[direction][1] * (1-odd), odd,
         branch_directions[direction][2] * (1-odd),
         self.branch, direction, length - odd, odd == 1 and 0 or 1)
    else
      go(0, 0, 0, self.spawn_leaves, 1)
    end
  end

  function nfa:spawn_leaves(count)
    if count < #directions then
      fork(self.spawn_leaves, count + 1)
    end

    go(directions[count][1]*2, 2+directions[count][2], directions[count][3]*2,
       self.create_leaves, 1)
    go(0, 0, 0, self.terminate)
  end

  function nfa:create_leaves(iter)
    if iter < #leaf_directions then
      fork(self.create_leaves, iter + 1)
    end

    go(leaf_directions[iter][1], leaf_directions[iter][2],
       leaf_directions[iter][3],
       self.create_leaf)
  end

  function nfa:create_leaf()
    put_voxel(0, resource.voxel[leafkey]())
  end

  local n = compile(nfa, nfa.init_state)
  mg.ntvp_visibility(n, resource.voxel.cherrytree_trunk(), 1, 1, 1, 1)
  mg.ntvp_visibility(n, resource.voxel[leafkey](), 1, 1, 2, 3)
  return n
end

for i = 1, 4 do
  resource.ntvp["cherrytree"..i] =
    core.bind(cherrytree_gen_ntvp)("cherrytree_leaf"..i)
end
