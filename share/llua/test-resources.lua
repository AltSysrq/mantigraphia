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


-- TODO: Centralise, make sane, etc
function populate_vmap()
  local nfa = { resource.ntvp.oaktree1(), resource.ntvp.oaktree2(),
                resource.ntvp.oaktree3(), resource.ntvp.oaktree4() }

  mg.wod_clear()
  mg.wod_permit_terrain_type(mg.terrain_type_snow)
  mg.wod_permit_terrain_type(mg.terrain_type_grass)
  mg.wod_permit_terrain_type(mg.terrain_type_bare_grass)
  mg.wod_add_perlin(16, 1024)
  mg.wod_add_perlin(32, 2048)
  mg.wod_add_perlin(256, 4096)
  mg.wod_add_perlin(512, 8192)
  for i = 1, #nfa do
    mg.wod_add_ntvp(nfa[i], 32, 32, 65535)
  end
  mg.wod_distribute(65536, 6000)
end
