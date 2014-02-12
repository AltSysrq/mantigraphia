AUTOMAKE_OPTIONS = subdir-objects

.MAKE.JOB.PREFIX=

AM_CFLAGS=-std=c99 -Wall -D_BSD_SOURCE \
          $(SDL_CFLAGS) $(SDLIMG_CFLAGS) $(GL_CFLAGS) $(GLEW_CFLAGS) \
          $(GLU_CFLAGS)
AM_LDFLAGS=$(SDL_LIBS) $(SDLIMG_LIBS) $(GL_LIBS) $(GLEW_LIBS) $(GLU_LIBS)

noinst_PROGRAMS = gentrigtab
bin_PROGRAMS = mantigraphia
gentrigtab_SOURCES = gen_trigtab.c
gentrigtab_LDFLAGS = -lm
mantigraphia_SOURCES = \
main.c \
coords.c \
trigtab.c \
rand.c \
micromp.c \
graphics/canvas.c \
graphics/parchment.c \
graphics/pencil.c \
graphics/brush.c \
graphics/tscan.c \
graphics/tiled-texture.c \
graphics/linear-paint-tile.c \
graphics/perspective.c \
graphics/hull.c \
graphics/dm-proj.c \
graphics/fast-brush.c \
gl/shader_loader.c \
gl/shaders.c \
world/basic-world.c \
world/terrain.c \
world/props.c \
world/propped-world.c \
world/generate.c \
render/turtle.c \
render/context.c \
render/shared-fast-brush.c \
render/terrabuff.c \
render/basic-world.c \
render/props.c \
render/lsystem.c \
render/draw-queue.c \
render/grass-props.c \
render/tree-props.c \
render/propped-world.c \
control/mouselook.c \
flower-pot.c \
cosine-world.c

trigtab.c: gentrigtab
	./gentrigtab >trigtab.c
