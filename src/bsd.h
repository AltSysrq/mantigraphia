/* Since GNU's "libbsd" package for some reason puts the headers in a different
 * location, despite its sole purpose being to provide compatibility with
 * applications using the BSD extensions, we need this bit of logic to figure
 * out where the includes are.
 *
 * Also includes functions very commonly needed (ie, for splay trees).
 */

#ifndef L3_BSD_H_
#define L3_BSD_H_

#if HAVE_BSD_ERR_H
#include <bsd/err.h>
#elif HAVE_ERR_H
#include <err.h>
#else
#error "No BSD err.h could be found on your system. (See libbsd-dev on GNU.)"
#endif

#if HAVE_BSD_SYS_QUEUE_H
#include <bsd/sys/queue.h>
#elif HAVE_SYS_QUEUE_H
#include <sys/queue.h>
#else
#error "No BSD sys/queue.h could be found on your system. (See libbsd-dev on GNU.)"
#endif

#if HAVE_BSD_SYS_TREE_H
#include <bsd/sys/tree.h>
#elif HAVE_SYS_TREE_H
#include <sys/tree.h>
#else
#error "No BSD sys/tree.h could be found on your system. (See libbsd-dev on GNU.)"
#endif

#if HAVE_BSD_SYSEXITS_H
#include <bsd/sysexits.h>
#elif HAVE_SYSEXITS_H
#include <sysexits.h>
#else
#error "No BSD sysexits.h could be found on your system. (See libbsd-dev on GNU.)"
#endif

static inline int l3_splay_compare_leading_unsigned(
  const void* va, const void* vb
) {
  const unsigned* a = va, * b = vb;
  if (*a == *b) return 0;
  if (*a <  *b) return -1;
  else          return +1;
}

#endif /* L3_BSD_H_ */
