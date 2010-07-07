# -*- Autoconf -*-
#
# Copyright (c) 2010 Sandia Corporation
#

## --------------------- ##
## Autoconf Requirements ##
## --------------------- ##

AC_PREREQ(2.60)

## ---------------------------------- ##
## Autoconf / Automake Initialization ##
## ---------------------------------- ##
AC_INIT([portals], [4], [kbwheel@sandia.gov])
AC_CONFIG_AUX_DIR([config])
AC_CONFIG_MACRO_DIR([config])
AC_CONFIG_SRCDIR([src/shmem/init.c])

dnl Require at least AM 1.7.  Prior versions require AM_CONFIG_HEADER
dnl and have no equivalent to AC_CONFIG_HEADERS (the plural version),
dnl which is a pain.  AM 1.7 also added support for putting target
dnl information in AM_CONDITIONAL blocks and using the += modifier.
dnl ... removed "check-news" because of automatic version generation
AM_INIT_AUTOMAKE([foreign dist-bzip2 no-define 1.7])
# If Automake supports silent rules, enable them
m4_ifdef([AM_SILENT_RULES], [AM_SILENT_RULES([yes])])

# clean some extra things...
CLEANFILES="*~ .\#* .gdb_history"
AC_SUBST(CLEANFILES)

## -------------------------- ##
## Information on the package ##
## -------------------------- ##
AC_CANONICAL_HOST

## ----------------- ##
## Configure Options ##
## ----------------- ##
AC_ARG_ENABLE([picky],
			  [AS_HELP_STRING([--enable-picky],
							  [turns on extra compiler warnings (for developers of portals)])])
AS_IF([test "x$enable_picky" == x],
	  [AS_IF([test -d "${srcdir}/.svn"],
			 [echo "--> developer override: enable picky compiler by default"
			  enable_picky=yes])])

## ------------------- ##
## Checks for programs ##
## ------------------- ##
AC_PROG_CC
dnl We use system extensions.  This includes setting _GNU_SOURCE
AC_USE_SYSTEM_EXTENSIONS
AC_PROG_CC_C99
AC_PROG_CPP
AC_PROG_INSTALL
AC_PROG_LN_S
AC_PROG_MAKE_SET
AC_PROG_LIBTOOL

AC_CACHE_SAVE

## ----------------------- ##
## Checks for header files ##
## ----------------------- ##
AC_HEADER_STDC
AC_SYS_LARGEFILE

## --------------- ##
## Check for types ##
## --------------- ##

## ---------------------------------- ##
## Check for compiler characteristics ##
## ---------------------------------- ##
SANDIA_DETECT_COMPILER_TYPE
AC_C_CONST
AC_C_INLINE
AC_C_VOLATILE
# Find out if we need the -restrict flag
RESTRICT_CXXFLAGS=""
AS_IF([test "x$sandia_cv_cxx_compiler_type" = "xIntel"],
	  [CXXFLAGS="-restrict $CXXFLAGS"])

AX_C_RESTRICT
AS_IF([test "x$enable_picky" == xyes],
  [case "$sandia_cv_c_compiler_type" in
   GNU)
   CFLAGS="-Wall -Wno-strict-aliasing -Wmissing-prototypes -Wstrict-prototypes $CFLAGS"
   ;;
   Intel)
   CFLAGS="-Wall $CFLAGS"
   ;;
   SunStudio)
   # This compiler defaults to enabling all warnings
   ;;
   PortlandGroup)
   CFLAGS="-Minform=inform $CFLAGS"
   ;;
   *)
   CFLAGS="-Wall $CFLAGS"
   ;;
   esac
   case "$sandia_cv_cxx_compiler_type" in
   GNU)
   CXXFLAGS="-Wall $CXXFLAGS"
   ;;
   Intel)
   CXXFLAGS="-Wall -wd981 $CXXFLAGS"
   ;;
   SunStudio)
   # This compiler defaults to enabling all warnings
   ;;
   PortlandGroup)
   CXXFLAGS="-Minform=inform $CXXFLAGS"
   ;;
   *)
   CXXFLAGS="-Wall $CXXFLAGS"
   ;;
   esac])

SANDIA_CHECK_ATOMICS([],[AC_MSG_ERROR([atomics are no implemented portably])])

AC_CACHE_SAVE

## --------------------------- ##
## Check for library functions ##
## --------------------------- ##

AC_FUNC_MMAP

AC_CACHE_SAVE

## --------------- ##
## Output and done ##
## --------------- ##

AM_CONDITIONAL([BACKEND_IMPL_SHMEM], [true])

AC_CONFIG_HEADERS([include/config.h])
AC_CONFIG_FILES([Makefile
				 src/Makefile
				 doc/Makefile
				 include/Makefile
				 test/Makefile])
AC_OUTPUT
