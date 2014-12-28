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

module_name = "rfb"
preamble = [[
#include "../resource/resource-file-builder.h"
]]

functions = {
  init = fun (nil) (ntbs(), ntbs(), ntbs(), ntbs(), ntbs(), ntbs(),
                    ntbs():nullable()),
  write = fun (nil) (ntbs()),
  add_note = fun (nil) (ntbs()),
  add_texture = fun (nil) (ntbs(), bytes(64*64*3), bytes(32*32*3),
                           bytes(16*16*3), bytes(8*8*3), bytes(4*4*3),
                           bytes(2*2*3), bytes(1*1*3)),
  add_palette = fun (nil) (ntbs(), uint(8):min(1), uint(8):min(12):max(36),
                           -- Trust the llua script to provide the correct
                           -- amount of memory.
                           bytestring(nil,0)),
  add_plane = fun (nil) (ntbs(), ntbs(), ntbs(), sint(32), sint(32)),
  add_voxel_graphic = fun (nil) (ntbs(), ntbs():nullable(),
                                 ntbs():nullable(), ntbs():nullable()),
  add_voxel = fun (nil) (ntbs(), uint(6), ntbs()),
  voxel_add_family = fun (nil) (ntbs()),
  voxel_add_context = fun (nil) (uint(6), ntbs()),
  add_script = fun (nil) (ntbs())
}

-- Shorten the C names since they're already qualified by the rfb table
for short_name in pairs(functions) do
  preamble ..= string.format('#define %s resource_file_builder_%s\n',
                             short_name, short_name)
end
