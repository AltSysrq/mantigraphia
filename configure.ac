#                                               -*- Autoconf -*-
# Process this file with autoconf to produce a configure script.

AC_PREREQ([2.6])
AC_INIT([mantigraphia], [0.0.0], [jason@lin.gl])
AC_CONFIG_SRCDIR([src/main.c])
AM_INIT_AUTOMAKE([1.11])
AM_SILENT_RULES(yes)
AM_CONFIG_HEADER([config.h])
PKG_PROG_PKG_CONFIG()

# Checks for programs.
AC_PROG_CC

# Checks for libraries.
AC_SEARCH_LIBS([errc], [c bsd])
AC_SEARCH_LIBS([sin], [c m])
PKG_CHECK_MODULES([SDL], [sdl2])
PKG_CHECK_MODULES([SDLIMG], [SDL2_image])

# Headers
AC_CHECK_HEADERS([arpa/inet.h inttypes.h malloc.h netinet/in.h stdint.h dnl
                  stdlib.h string.h sys/socket.h sys/time.h unistd.h])
AC_CHECK_HEADERS([sys/queue.h bsd/sys/queue.h sys/tree.h bsd/sys/tree.h dnl
                  err.h bsd/err.h sysexits.h bsd/sysexits.h])

# Checks for typedefs, structures, and compiler characteristics.
AC_HEADER_STDBOOL
AC_C_INLINE
AC_C_CONST
AC_TYPE_SIZE_T
AC_TYPE_SSIZE_T
AC_TYPE_UINT8_T
AC_TYPE_UINT16_T
AC_TYPE_UINT32_T
AC_TYPE_UINT64_T

# Checks for library functions.
AC_CHECK_FUNCS([memmove memset pow setlocale sqrt])

AC_CONFIG_FILES([Makefile src/Makefile])
AC_OUTPUT
