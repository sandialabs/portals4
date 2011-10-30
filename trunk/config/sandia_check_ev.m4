# -*- Autoconf -*-
#
# Copyright (c)      2011  Sandia Corporation
#

# SANDIA_CHECK_EV([action-if-found], [action-if-not-found])
# ------------------------------------------------------------------------------
AC_DEFUN([SANDIA_CHECK_EV], [
  AC_ARG_WITH([ev],
    [AS_HELP_STRING([--with-ev=[path]],
       [Location of EV library])])

  AS_IF([test "$with_ev" = "no"], [happy=no], [happy=yes])

  EV_CPPFLAGS=
  EV_LDFLAGS=
  EV_LIBS="-lev"

  saved_CPPFLAGS="$CPPFLAGS"
  saved_LDFLAGS="$LDFLAGS"
  saved_LIBS="$LIBS"
  AS_IF([test ! -z "$with_ev" -a "$with_ev" != "yes"],
    [CPPFLAGS="$CPPFLAGS -I$with_ev/include"
     LDFLAGS="$LDFLAGS -L$with_ev/lib"
     EV_CPPFLAGS="-I$with_ev/include"
     EV_LDFLAGS="-L$with_ev/lib"])

  AS_IF([test "$happy" = "yes"], 
    [AC_CHECK_HEADERS([ev.h], [], [happy=no])])
  AS_IF([test "$happy" = "yes"],
    [AC_CHECK_LIB([ev], [ev_loop_new], [], [happy=no])])

  CPPFLAGS="$saved_CPPFLAGS"
  LDFLAGS="$saved_LDFLAGS"
  LIBS="$saved_LIBS"

  AC_SUBST(EV_CPPFLAGS)
  AC_SUBST(EV_LDFLAGS)
  AC_SUBST(EV_LIBS) 

  AS_IF([test "$happy" = "yes"], [$1], [$2])
])
