# -*- Autoconf -*-
#
# Copyright (c)      2011  Sandia Corporation
#

# SANDIA_CHECK_OFED([action-if-found], [action-if-not-found])
# ------------------------------------------------------------------------------
AC_DEFUN([SANDIA_CHECK_OFED], [
  AC_ARG_WITH([ofed],
    [AS_HELP_STRING([--with-ofed=[path]],
       [Location of OFED library])])

  AS_IF([test "$with_ofed" = "no"], [happy=no], [happy=yes])

  OFED_CPPFLAGS=
  OFED_LDFLAGS=
  OFED_LIBS="-libverbs"

  saved_CPPFLAGS="$CPPFLAGS"
  saved_LDFLAGS="$LDFLAGS"
  saved_LIBS="$LIBS"
  AS_IF([test ! -z "$with_ofed" -a "$with_ofed" != "yes"],
    [CPPFLAGS="$CPPFLAGS -I$with_ofed/include"
     LDFLAGS="$LDFLAGS -L$with_ofed/lib -L$with_ofed/lib64"
     OFED_CPPFLAGS="-I$with_ofed/include"
     OFED_LDFLAGS="-L$with_ofed/lib -L$with_ofed/lib64"])

  AS_IF([test "$happy" = "yes"], 
    [AC_CHECK_HEADERS([infiniband/ib.h], [], [happy=no])])
  AS_IF([test "$happy" = "yes"],
    [AC_CHECK_LIB([ibverbs], [ibv_open_device], [], [happy=no])])

  CPPFLAGS="$saved_CPPFLAGS"
  LDFLAGS="$saved_LDFLAGS"
  LIBS="$saved_LIBS"

  AC_SUBST(OFED_CPPFLAGS)
  AC_SUBST(OFED_LDFLAGS)
  AC_SUBST(OFED_LIBS) 

  AS_IF([test "$happy" = "yes"], [$1], [$2])
])
