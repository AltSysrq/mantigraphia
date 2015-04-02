AUTOMAKE_OPTIONS = subdir-objects
SUBDIRS = contrib/praefectus .

.MAKE.JOB.PREFIX=

# We don't plan to have Lua stuff directly interact with the OS anyway, so just
# use the most conservative option. Autotools takes care of all of the other
# platform stuff for us (at least the platforms we care about --- nobody's
# going to be building this on AIX).
AM_CFLAGS=-std=c99 -Wall -D_BSD_SOURCE -DLUA_ANSI \
          $(SDL_CFLAGS) $(SDLIMG_CFLAGS) $(GL_CFLAGS) $(GLEW_CFLAGS) \
          $(GLU_CFLAGS) -Icontrib/praefectus/src/libpraefectus \
          -Icontrib/praefectus/src/asn1-support
AM_LDFLAGS=$(SDL_LIBS) $(SDLIMG_LIBS) $(GL_LIBS) $(GLEW_LIBS) $(GLU_LIBS)

MANTIGRAPHIA_LDCOMMON = libmantigraphia.la libllua.la \
contrib/praefectus/src/libpraefectus.la \
contrib/praefectus/src/libpraefectus-umb.la

bin_PROGRAMS = mantigraphia
noinst_PROGRAMS = llua mgllua
noinst_LTLIBRARIES = libmantigraphia.la libllua.la
mantigraphia_SOURCES = \
top/main.c
mantigraphia_LDADD = $(MANTIGRAPHIA_LDCOMMON)

libmantigraphia_la_SOURCES = \
micromp.c \
math/coords.c \
math/trigtab.c \
math/rand.c \
math/evaluator.c \
math/poisson-disc.c \
graphics/canvas.c \
graphics/parchment.c \
graphics/linear-paint-tile.c \
graphics/perspective.c \
gl/glinfo.c \
gl/shader_loader.c \
gl/shaders.c \
gl/marshal.c \
gl/auxbuff.c \
world/terrain-tilemap.c \
world/terrain.c \
world/generate.c \
world/hspace.c \
world/env-vmap.c \
world/vmap-painter.c \
world/nfa-turtle-vmap-painter.c \
world/world-object-distributor.c \
world/flower-map.c \
render/context.c \
render/terrabuff.c \
render/terrain-tilemap-rnd.c \
render/colour-palettes.c \
render/paint-overlay.c \
render/env-vmap-manifold-renderer.c \
render/skybox.c \
render/flower-map-renderer.c \
control/mouselook.c \
resource/resource-loader.c \
resource/texgen.c \
llua-bindings/lluas.c \
llua-bindings/mg-module.c \
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

mgllua_CFLAGS = $(AM_CFLAGS) -DLUA_WITH_MANTIGRAPHIA_MODULES
mgllua_SOURCES = contrib/lua/lua.c
mgllua_LDADD = $(MANTIGRAPHIA_LDCOMMON)

llua-bindings/mg-module.c: llua-bindings/mg-module.lua \
llua-bindings/gen-bindings.lua llua
	$(AM_V_GEN)./llua llua-bindings/gen-bindings.lua \
	    llua-bindings/mg-module >llua-bindings/mg-module.c
