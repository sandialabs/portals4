# -*- Autoconf -*-
#
# Copyright (c)      2011  Sandia Corporation
#

# SANDIA_DETECT_KNEM([action-if-found], [action-if-not-found])
# ------------------------------------------------------------------------------
AC_DEFUN([SANDIA_DETECT_KNEM], [
AC_ARG_WITH([knem],
			[AS_HELP_STRING([--with-knem=[path]],
							[Use KNEM for bulk message transfer, and optionally specify a path])],
			[knem_softfail=no],
			[with_knem=yes
			 knem_softfail=yes])
saved_CPPFLAGS="$CPPFLAGS"
knem_happy=yes

AS_IF([test "x$with_knem" != xyes -a "x$with_knem" != xno],
      [CPPFLAGS="$CPPFLAGS -I$with_knem/include"])
AC_CHECK_HEADERS([knem_io.h],
                 [knem_happy=yes],
				 [knem_happy=no])
AS_IF([test "$knem_happy" == yes],
	  [AC_PREPROC_IFELSE([AC_LANG_SOURCE([[#include <knem_io.h>
#if KNEM_ABI_VERSION < 0xd
# error "KNEM ABI >= 0xd is required"
#endif]])],
	                     [knem_happy=yes],
						 [knem_happy=no])])
AS_IF([test "$knem_happy" == no],
      [$2
	   AS_IF([test "$knem_softfail" == no],
	         [AS_IF([test "x$with_knem" == xyes],
			 	    [AC_ERROR([KNEM enabled, but cannot find it.])],
					[AC_ERROR([KNEM location specified, but cannot find it.])])],
			 [CPPFLAGS="$saved_CPPFLAGS"])],
	  [$1
	   AC_DEFINE([USE_KNEM],[1],[Define to use KNEM])])
])
