AUTOMAKE_OPTIONS = subdir-objects
.MAKE.JOB.PREFIX=

TESTS = math/evaluator.t world/env-vmap.t
AM_CFLAGS = $(CHECK_CFLAGS) -I$(top_builddir)/src \
  -Wall -Wextra \
  -Wno-unused-parameter -Wno-long-long -Wno-missing-field-initializers \
  -std=gnu99

AM_LDFLAGS = libtestcore.la $(CHECK_LIBS) ../src/libmantigraphia.la

check_PROGRAMS = $(TESTS)
check_LTLIBRARIES = libtestcore.la
libtestcore_la_SOURCES = test.c
libtestcore_la_LDFLAGS = $(CHECK_LIBS)
math_evaluator_t_SOURCES = math/evaluator.c
world_env_vmap_t_SOURCES = world/env-vmap.c
