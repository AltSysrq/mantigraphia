#                                               -*- Autoconf -*-
# Process this file with autoconf to produce a configure script.

AC_PREREQ([2.6])
AC_INIT([mantigraphia], [0.0.0], [jason@lin.gl])
AC_CONFIG_SRCDIR([src/micromp.c])
AM_INIT_AUTOMAKE([1.11])
AM_SILENT_RULES(yes)
AC_CONFIG_MACRO_DIR([m4])
LT_PREREQ([1.5])
LT_INIT([disable-shared])
AM_CONFIG_HEADER([config.h])
PKG_PROG_PKG_CONFIG()

# Disable extraneous praefectus stuff
AM_CONDITIONAL(BUILD_DEMOS, false)
AM_CONDITIONAL(BUILD_VERTEXD, false)

# Checks for programs.
AC_PROG_CC
AM_PROG_CC_C_O
AC_CHECK_PROG([ASN1C], [asn1c], [asn1c], [])
AS_IF([test "x$ASN1C" = "x"],
      [AC_MSG_ERROR([cannot find "asn1c" in the path])])

# Modifying these variables directly is typically considered a bad thing.
# But for whatever reason, Autotools doesn't bother adding the /usr/local paths
# to the search directories on systems (eg, FreeBSD) where *everything* lives
# there, so we'll do it ourselves here, making sure to preserve the flags
# already given.
CPPFLAGS="$CPPFLAGS -I/usr/local/include"
LDFLAGS="$LDFLAGS -L/usr/local/lib"

# Checks for libraries.
AC_SEARCH_LIBS([errc], [c bsd])
AC_SEARCH_LIBS([sin], [c m])
AC_SEARCH_LIBS([dlopen], [dl])
PKG_CHECK_MODULES([SDL], [sdl2])
PKG_CHECK_MODULES([SDLIMG], [SDL2_image])
PKG_CHECK_MODULES([GL], [gl])
PKG_CHECK_MODULES([GLU], [glu])
PKG_CHECK_MODULES([GLEW], [glew])
PKG_CHECK_MODULES([CHECK], [check], [], [:])
AC_SEARCH_LIBS(__gmpz_powm, [gmp mpir], [],
[AC_MSG_ERROR([cannot locate a gmp or mpir library])])
# In theory, this should work, but maybe it won't.
# Leave it here for now, and cross that bridge when we eventually port to this
# platform.
AC_SEARCH_LIBS([CryptAcquireContextW], [advapi32])
AC_SEARCH_LIBS([WSAStartup], [ws2_32 ws2_64 wsock32 wsock64])

# Headers
AC_CHECK_HEADERS([sys/types.h sys/socket.h netinet/in.h netdb.h])
AC_CHECK_HEADERS([fcntl.h unistd.h])
AC_CHECK_HEADERS([sys/queue.h bsd/sys/queue.h sys/tree.h bsd/sys/tree.h dnl
                  err.h bsd/err.h sysexits.h bsd/sysexits.h bsd/string.h dnl
                  sys/endian.h bsd/sys/endian.h])
AC_CHECK_HEADERS([sys/param.h])
AC_CHECK_HEADERS([sys/cpuset.h], [], [], [
#ifdef HAVE_SYS_PARAM_H
#include <sys/param.h>
#endif
])
AC_CHECK_HEADERS([winsock2.h ws2tcpip.h])
AC_CHECK_HEADERS([gmp.h mpir.h])
# For Windows-specific crypto functions, since that platform doesn't have
# /dev/urandom and such.
AC_CHECK_HEADERS([windows.h])

# Checks for typedefs, structures, and compiler characteristics.
AC_HEADER_STDBOOL
AC_C_INLINE
AC_C_CONST
AC_C_RESTRICT
AC_C_FLEXIBLE_ARRAY_MEMBER
AC_TYPE_SIZE_T
AC_TYPE_SSIZE_T
AC_TYPE_UINT8_T
AC_TYPE_UINT16_T
AC_TYPE_UINT32_T
AC_TYPE_UINT64_T

# Checks for library functions.
AC_CHECK_FUNCS([memmove memset pow setlocale sqrt dlfunc dlerror dnl
                cpuset_setaffinity])

AC_CONFIG_FILES([Makefile src/Makefile test/Makefile
                 share/glsl/Makefile
                 src/contrib/praefectus/Makefile
                 src/contrib/praefectus/src/Makefile
                 src/contrib/praefectus/src/asn1-support/Makefile
                 src/contrib/praefectus/src/udp-common/Makefile
                 src/contrib/praefectus/src/libpraefectus/messages/Makefile
                 src/contrib/praefectus/src/vertexd/config/Makefile
                 src/contrib/praefectus/src/praefectus/asn1/Makefile
                 src/contrib/praefectus/test/Makefile])
AC_OUTPUT
