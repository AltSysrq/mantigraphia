.MAKE.JOB.PREFIX=
noinst_DATA = gen_vvoxel.glsl

gen_vvoxel.glsl: perspective.glsl vvoxel.glsl
	$(AM_V_GEN)cat perspective.glsl vvoxel.glsl >gen_vvoxel.glsl
