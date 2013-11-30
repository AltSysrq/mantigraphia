AUTOMAKE_OPTIONS = subdir-objects

AM_CFLAGS=-std=c99 -Wall -D_BSD_SOURCE \
          $(SDL_CFLAGS) $(SDLIMG_CFLAGS) $(GL_CFLAGS)
AM_LDFLAGS=$(SDL_LIBS) $(SDLIMG_LIBS) $(GL_LIBS)

noinst_PROGRAMS = gentrigtab
bin_PROGRAMS = mantigraphia
gentrigtab_SOURCES = gen_trigtab.c
gentrigtab_LDFLAGS = -lm
mantigraphia_SOURCES = \
main.c \
coords.c \
trigtab.c \
graphics/canvas.c \
graphics/parchment.c \
graphics/pencil.c \
graphics/brush.c \
graphics/tscan.c \
graphics/tiled-texture.c \
graphics/linear-paint-tile.c

trigtab.c: gentrigtab
	./gentrigtab >trigtab.c
