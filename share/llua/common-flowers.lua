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

local function grad_rgb(r0, g0, b0, r1, g1, b1)
  return core.argb_gradient({ 255, r0, g0, b0 },
                            { 255, r1, g1, b1 }, 4)
end

resource.flower.wild_aprjune_white = core.bind(core.new_flower) {
  colours = grad_rgb(204, 204, 204, 140, 140, 150),
  appear = 1.0,
  disappear = 4.0,
  size = 0.25,
  date_stagger = 0.5,
}

resource.flower.wild_juneaug_pink = core.bind(core.new_flower) {
  colours = grad_rgb(255, 204, 187, 150, 125, 110),
  appear = 3.0,
  disappear = 6.0,
  size = 0.2,
  date_stagger = 0.5,
}

resource.flower.wild_augoct_blue = core.bind(core.new_flower) {
  colours = grad_rgb(170, 170, 204, 100, 100, 150),
  appear = 5.0,
  disappear = 8.0,
  size = 0.275,
  date_stagger = 0.5,
}

resource.flower.seasonal_apr_red = core.bind(core.new_flower) {
  colours = grad_rgb(255, 64, 32, 140, 32, 24),
  appear = 0.5,
  disappear = 2.0,
  size = 0.23,
  date_stagger = 0.3,
}

resource.flower.seasonal_may_yellow = core.bind(core.new_flower) {
  colours = grad_rgb(204, 200, 32, 150, 150, 24),
  appear = 1.0,
  disappear = 3.0,
  size = 0.2,
  date_stagger = 0.3,
}

resource.flower.seasonal_june_blue = core.bind(core.new_flower) {
  colours = grad_rgb(48, 48, 170, 24, 24, 140),
  appear = 2.0,
  disappear = 4.0,
  size = 0.25,
  date_stagger = 0.3,
}

resource.flower.seasonal_july_orange = core.bind(core.new_flower) {
  colours = grad_rgb(255, 150, 32, 170, 85, 24),
  appear = 3.0,
  disappear = 5.0,
  size = 0.225,
}

resource.flower.seasonal_aug_white = core.bind(core.new_flower) {
  colours = grad_rgb(224, 224, 200, 149, 149, 137),
  appear = 4.0,
  disappear = 6.0,
  size = 0.2,
  date_stagger = 0.3,
}

resource.flower.seasonal_sept_violet = core.bind(core.new_flower) {
  colours = grad_rgb(128, 48, 170, 85, 32, 120),
  appear = 5.0,
  disappear = 7.0,
  size = 0.25,
  date_stagger = 0.3,
}

--- Distributes the common flowers in a "standard" way
function common_flowers_distribute_default()
  -- Distribute wildflowers uniformly
  mg.wod_clear()
  mg.wod_permit_terrain_type(mg.terrain_type_grass)
  mg.wod_add_flower(resource.flower.wild_aprjune_white(), 0.1, 0.4)
  mg.wod_add_flower(resource.flower.wild_juneaug_pink(), 0.1, 0.4)
  mg.wod_add_flower(resource.flower.wild_augoct_blue(), 0.1, 0.4)
  mg.wod_distribute(100000, 0)

  local function put_seasonal_flower(flower_type, group_wavelen, h1, h2)
    mg.wod_clear()
    mg.wod_permit_terrain_type(mg.terrain_type_grass)
    mg.wod_add_perlin(64, 256)
    mg.wod_add_perlin(32, 96)
    mg.wod_add_perlin(group_wavelen, 64)
    mg.wod_add_flower(flower_type, h1, h2)
    mg.wod_distribute(1000000, 256)
  end

  put_seasonal_flower(resource.flower.seasonal_apr_red(), 16, 0.2, 0.4)
  put_seasonal_flower(resource.flower.seasonal_may_yellow(), 8, 0.1, 0.3)
  put_seasonal_flower(resource.flower.seasonal_june_blue(), 64, 0.2, 0.3)
  put_seasonal_flower(resource.flower.seasonal_july_orange(), 32, 0.2, 0.5)
  put_seasonal_flower(resource.flower.seasonal_aug_white(), 16, 0.2, 0.4)
  put_seasonal_flower(resource.flower.seasonal_sept_violet(), 32, 0.3, 0.4)
end
