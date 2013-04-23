# -*- Autoconf -*-
#
# Copyright (c)      2011  Sandia Corporation
#

# SANDIA_CHECK_EV4([action-if-found], [action-if-not-found])
# ------------------------------------------------------------------------------
AC_DEFUN([SANDIA_CHECK_EV4],[dnl
AC_CACHE_CHECK(
  [for libev 4 or newer],
  [sandia_cv_c_libev4],
  [AC_COMPILE_IFELSE([AC_LANG_SOURCE([[
#ifdef LIBEV_INC_PREFIXED
#include <libev/ev.h>
#else
#include <ev.h>
#endif

#if EV_VERSION_MAJOR < 4
#error "libev version too old"
#endif
]])],
     [sandia_cv_c_libev4=yes],
     [sandia_cv_c_libev4=no])])

AS_IF([test "$sandia_cv_c_libev4" = "yes"], [$1], [$2])
])


# SANDIA_CHECK_EV([action-if-found], [action-if-not-found])
# ------------------------------------------------------------------------------
AC_DEFUN([SANDIA_CHECK_EV], [
  AC_ARG_WITH([ev],
    [AS_HELP_STRING([--with-ev=[path]],
       [Location of EV library])])

  SANDIA_CHECK_PATH([$with_ev], [], [AC_MSG_ERROR([--with-ev option must be an absolute path])])

  saved_CPPFLAGS="$CPPFLAGS"
  saved_LDFLAGS="$LDFLAGS"
  saved_LIBS="$LIBS"

  LIBS="$LIBS -lm"

  AS_IF([test "$with_ev" != "no"],
    [AS_IF([test ! -z "$with_ev" -a "$with_ev" != "yes"],
       [check_ev_dir="$with_ev"])
     AS_IF([test ! -z "$with_ev_libdir" -a "$with_ev_libdir" != "yes"],
       [check_ev_libdir="$with_ev_libdir"])
     OMPI_CHECK_PACKAGE([ev],
                        [ev.h],
                        [ev],
                        [ev_loop_new],
                        [],
                        [$check_ev_dir],
                        [$check_ev_libdir],
                        [check_ev_happy="yes"],
                        [check_ev_happy="no"])
     AS_IF([test "x$check_ev_happy" = xno],
	       [OMPI_CHECK_PACKAGE([ev],
	       [libev/ev.h],
	       [ev],
	       [ev_loop_new],
	       [],
	       [$check_ev_dir],
	       [$check_ev_libdir],
	       [check_ev_happy="yes"
	       AC_DEFINE([LIBEV_INC_PREFIXED],[1],[libev headers in a weird spot])],
	                 [check_ev_happy="no"])])],
    [check_ev_happy="no"])

  CPPFLAGS="$CPPFLAGS $ev_CPPFLAGS"
  LDFLAGS="$LDFLAGS $ev_LDFLAGS"
  LIBS="$LIBS $ev_LIBS"

  SANDIA_CHECK_EV4([], [AC_MSG_ERROR([libev version too old; 4.0 or later required])])

  CPPFLAGS="$saved_CPPFLAGS"
  LDFLAGS="$saved_LDFLAGS"
  LIBS="$saved_LIBS"

  echo "ev_LIBS: $ev_LIBS"
  ev_LIBS="$ev_LIBS -lm"
  echo "ev_LIBS: $ev_LIBS"

  AC_SUBST(ev_CPPFLAGS)
  AC_SUBST(ev_LDFLAGS)
  AC_SUBST(ev_LIBS)

  AS_IF([test "$check_ev_happy" = "yes"],
    [$1],
    [AS_IF([test ! -z "$with_ev" -a "$with_ev" != "no"],
      [AC_MSG_ERROR([EV support requested but not found.  Aborting])])
     $2])
])
