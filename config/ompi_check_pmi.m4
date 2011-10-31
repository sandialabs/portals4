# -*- shell-script -*-
#
# Copyright (c) 2004-2005 The Trustees of Indiana University and Indiana
#                         University Research and Technology
#                         Corporation.  All rights reserved.
# Copyright (c) 2004-2005 The University of Tennessee and The University
#                         of Tennessee Research Foundation.  All rights
#                         reserved.
# Copyright (c) 2004-2005 High Performance Computing Center Stuttgart, 
#                         University of Stuttgart.  All rights reserved.
# Copyright (c) 2004-2005 The Regents of the University of California.
#                         All rights reserved.
# Copyright (c) 2009-2011 Cisco Systems, Inc.  All rights reserved.
# Copyright (c) 2011      Los Alamos National Security, LLC. All rights
#                         reserved.
# $COPYRIGHT$
# 
# Additional copyrights may follow
# 
# $HEADER$
#

# OMPI_CHECK_PMI([action-if-found], [action-if-not-found])
# --------------------------------------------------------
AC_DEFUN([OMPI_CHECK_PMI],[
    AC_ARG_WITH([pmi],
                [AC_HELP_STRING([--with-pmi@<:@=DIR@:>@],
                                [Build PMI support, if found])])
    AC_ARG_WITH([pmi-libdir],
        [AC_HELP_STRING([--with-pmi-libdir=DIR],
             [Search for PMI libraries in DIR])])
    OMPI_CHECK_WITHDIR([pmi-libdir], [$with_pmi_libdir], [libpmi.*])

    check_pmi_save_CPPFLAGS="$CPPFLAGS"
    check_pmi_save_LDFLAGS="$LDFLAGS"
    check_pmi_save_LIBS="$LIBS"

    AS_IF([test "$with_pmi" != "no"],
          [AS_IF([test ! -z "$with_pmi" -a "$with_pmi" != "yes"],
                 [check_pmi_dir="$with_pmi"])
           AS_IF([test ! -z "$with_pmi_libdir" -a "$with_pmi_libdir" != "yes"],
                 [check_pmi_libdir="$with_pmi_libdir"])

           OMPI_CHECK_PACKAGE([pmi],
                              [pmi.h],
                              [pmi],
                              [PMI_Init],
                              [],
                              [$check_pmi_dir],
                              [$check_pmi_libdir],
                              [check_pmi_happy="yes"],
                              [check_pmi_happy="no"])
           AS_IF([test "$check_pmi_happy" = "no"], 
             [OMPI_CHECK_PACKAGE([pmi],
                                 [slurm/pmi.h],
                                 [pmi],
                                 [PMI_Init],
                                 [],
                                 [$check_pmi_dir],
                                 [$check_pmi_libdir],
                                 [AC_DEFINE([PMI_SLURM], [1],
                                    [Defined to 1 if PMI implementation is SLURM.])
                                  check_pmi_happy="yes"],
                                 [check_pmi_happy="no"])])],
          [check_pmi_happy="no"])

    CPPFLAGS="$check_pmi_save_CPPFLAGS"
    LDFLAGS="$check_pmi_save_LDFLAGS"
    LIBS="$check_pmi_save_LIBS"

    AC_SUBST(pmi_CPPFLAGS)
    AC_SUBST(pmi_LDFLAGS)
    AC_SUBST(pmi_LIBS)

    AS_IF([test "$check_pmi_happy" = "yes"],
          [$1],
          [AS_IF([test ! -z "$with_pmi" -a "$with_pmi" != "no"],
                 [AC_MSG_ERROR([PMI support requested but not found.  Aborting])])
           $2])
])
