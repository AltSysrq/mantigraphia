.MAKE.JOB.PREFIX=
noinst_DATA = gen_vvoxel.glsl gen_vmanifold.glsl gen_vflower.glsl

gen_vvoxel.glsl: perspective.glsl vvoxel.glsl
	$(AM_V_GEN)cat perspective.glsl vvoxel.glsl >gen_vvoxel.glsl

gen_vmanifold.glsl: perspective.glsl vmanifold.glsl
	$(AM_V_GEN)cat perspective.glsl vmanifold.glsl >gen_vmanifold.glsl

gen_vflower.glsl: perspective.glsl vflower.glsl
	$(AM_V_GEN)cat perspective.glsl vflower.glsl >gen_vflower.glsl
