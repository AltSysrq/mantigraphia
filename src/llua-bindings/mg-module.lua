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

module_name = "mg"
preamble = [[
#include "../math/coords.h"
#include "../math/frac.h"
#include "../math/rand.h"
]]

local coord = uint(32)
local coord_offset = sint(32)
local chronon = uint(32)
local velocity = coord_offset
local acceleration = coord_offset
local angle = sint(16)
local angular_velocity = angle
local zo_scaling_factor = sint(16)
local fraction = uint(32)
local precise_fraction = uint(64)

constants = {
  METRE = coord_offset,
  MILLIMETRE = coord_offset,
  SECOND = chronon,
  METRES_PER_SECOND = velocity,
  MM_PER_SECOND = velocity,
  METRES_PER_SS = acceleration,
  GRAVITY = acceleration,
  DEG_90 = angle,
  DEG_180 = angle,
  DEG_270 = angle,
  ZO_SCALING_FACTOR_BITS = uint(32),
  ZO_SCALING_FACTOR_MAX = zo_scaling_factor,
  ZO_COSINE_COUNT = uint(32),

  FRACTION_BITS = uint(32),
  FRACTION_BASE = fraction,
  PRECISE_FRACTION_BITS = uint(32),
  PRECISE_FRACTION_BASE = precise_fraction,
  PRECISE_FRACTION_INTERMEDIATE_TRAILING_BITS = uint(32),
}

functions = {
  torus_dist = fun (coord_offset) (coord_offset, coord),
  clampu = fun (uint(32)) (uint(32), uint(32), uint(32)),
  clamps = fun (sint(32)) (sint(32), sint(32), sint(32)),
  zo_scale = fun (sint(32)) (sint(32), zo_scaling_factor),
  zo_cos = fun (zo_scaling_factor) (angle),
  zo_sin = fun (zo_scaling_factor) (angle),
  zo_cosms = fun (sint(32)) (angle, sint(32)),
  zo_sinms = fun (uint(32)) (angle, uint(32)),
  isqrt = fun (uint(32)) (uint(64)),

  fraction_of = fun (fraction) (uint(32):min(1)),
  fraction_of2 = fun (fraction) (uint(32), uint(32):min(1)),
  fraction_umul = fun (uint(32)) (uint(32), fraction),
  fraction_smul = fun (sint(32)) (sint(32), fraction),
  precise_fraction_of = fun (precise_fraction) (uint(64):min(1)),
  precise_fraction_umul = fun (uint(32)) (uint(16), precise_fraction),
  precise_fraction_smul = fun (sint(32)) (sint(16), precise_fraction),
  precise_fraction_ured = fun (uint(32)) (uint(32)),
  precise_fraction_sred = fun (sint(32)) (sint(32)),
  precise_fraction_uexp = fun (uint(32)) (uint(32)),
  precise_fraction_sexp = fun (sint(32)) (sint(32)),
  precise_fraction_fmul = fun (precise_fraction) (precise_fraction, precise_fraction),

  lcgrand = fun (uint(16)) (pointer(uint(32)):echo()),
  chaos_accum = fun (uint(32)) (uint(32), uint(32)),
  chaos_of = fun (uint(32)) (uint(32))
}
