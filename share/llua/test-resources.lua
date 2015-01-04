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

-- Playground for testing the resource system. This will eventually need to be
-- refactored into proper modules.

resource.palette.test_palette = core.bind(core.new_palette) {
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

function resource.texture.test_texture()
  local rnd = 42
  return core.new_texture(
    core.gentable2d(64, 64, function (x,y)
                      local r, b
                      r, rnd = mg.lcgrand(rnd)
                      b = bit32.extract(r, 8, 8)
                      r = bit32.extract(r, 0, 8)
                      return bit32.bor(
                        bit32.lshift(r, 16),
                        0x00ff00, b)
    end), core.mipmap_nearest)
end

function resource.graphic_plane.test_graphic_plane()
  local g = mg.rl_graphic_plane_new()
  mg.rl_graphic_plane_set_texture(g, resource.texture.test_texture())
  mg.rl_graphic_plane_set_palette(g, resource.palette.test_palette())
  return g
end

function resource.voxel_graphic.test_voxel_graphic()
  local g = mg.rl_voxel_graphic_new()
  for p = 0, 2 do
    mg.rl_voxel_graphic_set_plane(
      g, p, resource.graphic_plane.test_graphic_plane())
  end
  return g
end

function resource.voxel.test_voxel()
  local v = mg.rl_voxel_type_new()
  mg.rl_voxel_set_voxel_graphic(
    v * (2^mg.ENV_VOXEL_CONTEXT_BITS),
    resource.voxel_graphic.test_voxel_graphic())
  return v
end

-- TODO: Centralise, make sane, etc
function populate_vmap()
  local nfa = mg.ntvp_new()

  mg.ntvp_put_voxel(nfa, 0, 0, resource.voxel.test_voxel())
  mg.ntvp_transition(nfa, 0, 0, 0, 1, 0)
  mg.ntvp_transition(nfa, 0, 255, 0, 0, 0)
  mg.ntvp_branch(nfa, 0, 1, 1)
  mg.ntvp_transition(nfa, 1, 255, 0, 0, 0)
  mg.ntvp_transition(nfa, 1, 255, 0, 0, 0)
  mg.ntvp_transition(nfa, 1, 255, 0, 0, 0)
  mg.ntvp_transition(nfa, 1, 255, 0, 0, 0)
  for s = 2, 5 do
    local dx = 2 == s and 1 or 3 == s and -1 or 0
    local dz = 4 == s and 1 or 5 == s and -1 or 0
    mg.ntvp_put_voxel(nfa, s, 0, resource.voxel.test_voxel())
    mg.ntvp_transition(nfa, 1, s, 0, 0, 0)
    mg.ntvp_transition(nfa, s, s, dx, 0, dz)
    mg.ntvp_transition(nfa, s, s, dx, 0, dz)
    mg.ntvp_transition(nfa, s, s, dx, 0, dz)
    mg.ntvp_transition(nfa, s, s, dx, 0, dz)
    mg.ntvp_transition(nfa, s, 0, dx, 0, dz)
  end

  for z = 0, 4000, 64 do
    for x = 0, 4000, 64 do
      mg.ntvp_paint(nfa, x, 0, z, x-16, z-16, 32, 32, 2048)
    end
  end
end
