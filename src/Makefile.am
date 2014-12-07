AUTOMAKE_OPTIONS = subdir-objects

.MAKE.JOB.PREFIX=

AM_CFLAGS=-std=c99 -Wall -D_BSD_SOURCE \
          $(SDL_CFLAGS) $(SDLIMG_CFLAGS) $(GL_CFLAGS) $(GLEW_CFLAGS) \
          $(GLU_CFLAGS)
AM_LDFLAGS=$(SDL_LIBS) $(SDLIMG_LIBS) $(GL_LIBS) $(GLEW_LIBS) $(GLU_LIBS)

bin_PROGRAMS = mantigraphia
mantigraphia_SOURCES = \
top/main.c \
micromp.c \
math/coords.c \
math/trigtab.c \
math/rand.c \
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
graphics/glbrush.c \
graphics/glpencil.c \
gl/glinfo.c \
gl/shader_loader.c \
gl/shaders.c \
gl/marshal.c \
world/terrain-tilemap.c \
world/terrain.c \
world/props.c \
world/propped-world.c \
world/generate.c \
world/hspace.c \
render/turtle.c \
render/context.c \
render/terrabuff.c \
render/terrain-tilemap.c \
render/props.c \
render/lsystem.c \
render/grass-props.c \
render/tree-props.c \
render/propped-world.c \
render/colour-palettes.c \
control/mouselook.c \
top/cosine-world.c
