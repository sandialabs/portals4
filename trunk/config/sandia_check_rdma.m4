# -*- Autoconf -*-
#
# Copyright (c)      2011  Sandia Corporation
#

# SANDIA_CHECK_EV([action-if-found], [action-if-not-found])
# ------------------------------------------------------------------------------
AC_DEFUN([SANDIA_CHECK_RDMA], [
  AC_ARG_WITH([rdma],
    [AS_HELP_STRING([--with-rdma=[path]],
       [Location of RDMA-CM library])])

  saved_CPPFLAGS="$CPPFLAGS"
  saved_LDFLAGS="$LDFLAGS"
  saved_LIBS="$LIBS"

  AS_IF([test "$with_rdma" != "no"],
    [AS_IF([test ! -z "$with_rdma" -a "$with_rdma" != "yes"],
       [check_rdma_dir="$with_rdma"])
     AS_IF([test ! -z "$with_rdma_libdir" -a "$with_rdma_libdir" != "yes"],
       [check_rdma_libdir="$with_rdma_libdir"])
     OMPI_CHECK_PACKAGE([rdma],
                        [rdma/rdma_cma.h],
                        [rdmacm],
                        [rdma_create_event_channel],
                        [],
                        [$check_rdma_dir],
                        [$check_rdma_libdir],
                        [check_rdma_happy="yes"],
                        [check_rdma_happy="no"])],
    [check_rdma_happy="no"])

  CPPFLAGS="$saved_CPPFLAGS"
  LDFLAGS="$saved_LDFLAGS"
  LIBS="$saved_LIBS"

  AC_SUBST(rdma_CPPFLAGS)
  AC_SUBST(rdma_LDFLAGS)
  AC_SUBST(rdma_LIBS)

  AS_IF([test "$check_rdma_happy" = "yes"],
    [$1],
    [AS_IF([test ! -z "$with_rdma" -a "$with_rdma" != "no"],
      [AC_MSG_ERROR([RDMA library not found.  Aborting])])
     $2])
])
