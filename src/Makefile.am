AUTOMAKE_OPTIONS = subdir-objects

.MAKE.JOB.PREFIX=

# We don't plan to have Lua stuff directly interact with the OS anyway, so just
# use the most conservative option. Autotools takes care of all of the other
# platform stuff for us (at least the platforms we care about --- nobody's
# going to be building this on AIX).
AM_CFLAGS=-std=c99 -Wall -D_BSD_SOURCE -DLUA_ANSI \
          $(SDL_CFLAGS) $(SDLIMG_CFLAGS) $(GL_CFLAGS) $(GLEW_CFLAGS) \
          $(GLU_CFLAGS)
AM_LDFLAGS=$(SDL_LIBS) $(SDLIMG_LIBS) $(GL_LIBS) $(GLEW_LIBS) $(GLU_LIBS)

bin_PROGRAMS = mantigraphia
noinst_PROGRAMS = llua
noinst_LTLIBRARIES = libmantigraphia.la libllua.la
mantigraphia_SOURCES = \
top/main.c
mantigraphia_LDADD = libmantigraphia.la libllua.la

libmantigraphia_la_SOURCES = \
micromp.c \
math/coords.c \
math/trigtab.c \
math/rand.c \
math/evaluator.c \
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
world/generate.c \
world/hspace.c \
render/turtle.c \
render/context.c \
render/terrabuff.c \
render/terrain-tilemap-rnd.c \
render/lsystem.c \
render/colour-palettes.c \
control/mouselook.c \
top/cosine-world.c

libllua_la_SOURCES = \
contrib/lua/lapi.c \
contrib/lua/lauxlib.c \
contrib/lua/lbaselib.c \
contrib/lua/lbitlib.c \
contrib/lua/lcode.c \
contrib/lua/lcorolib.c \
contrib/lua/lctype.c \
contrib/lua/ldblib.c \
contrib/lua/ldebug.c \
contrib/lua/ldo.c \
contrib/lua/ldump.c \
contrib/lua/lfunc.c \
contrib/lua/lgc.c \
contrib/lua/linit.c \
contrib/lua/llex.c \
contrib/lua/lmem.c \
contrib/lua/loadlib.c \
contrib/lua/lobject.c \
contrib/lua/lopcodes.c \
contrib/lua/lparser.c \
contrib/lua/lstate.c \
contrib/lua/lstring.c \
contrib/lua/lstrlib.c \
contrib/lua/ltable.c \
contrib/lua/ltablib.c \
contrib/lua/ltm.c \
contrib/lua/lundump.c \
contrib/lua/lvm.c \
contrib/lua/lzio.c

llua_SOURCES = contrib/lua/lua.c
llua_LDADD = libllua.la
