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
AC_ARG_ENABLE([arg-checking],
			  [AS_HELP_STRING([--disable-arg-checking],
							  [turns off argument checking; does not strictly conform to Portals4 standard, but avoids unnecessary work in the common case])])
AC_ARG_ENABLE([hard-polling],
			  [AS_HELP_STRING([--enable-hard-polling],
							  [Forces hard polls on inter-process communication queues rather than blocking polls. This can reduce the message latency, especially when the message rate is low])])
AC_ARG_ENABLE([fast],
			  [AS_HELP_STRING([--enable-fast],
							  [Sets the available configuration options to run as quickly as possible. This makes the assumption and CAN result in bad things happening.])])
AS_IF([test "x$enable_hard_polling" == xyes],
	  [AC_DEFINE([USE_HARD_POLLING], [1], [Define to force the use of hard-polling for message queues.])])
AS_IF([test "x$enable_fast" == xyes],
	  [CFLAGS="-O3"
	   CPPFLAGS="-DNDEBUG"
	   enable_arg_checking=no])
AS_IF([test "x$enable_arg_checking" == xno],
	  [AC_DEFINE([NO_ARG_CHECKING], [1], [Define to disable argument checking])])


## ------------------- ##
## Checks for programs ##
## ------------------- ##
AC_PROG_AWK
AC_PROG_CC
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
AC_CHECK_HEADERS([fcntl.h stddef.h stdint.h malloc.h sys/time.h limits.h])
AC_CHECK_HEADERS([sys/posix_shm.h], [], [],
[#ifdef HAVE_SYS_TIME_H
# include <sys/time.h>
#endif
])

## --------------- ##
## Check for types ##
## --------------- ##
AC_C_INLINE
AC_TYPE_SIZE_T
AC_TYPE_UINT32_T
AC_TYPE_UINT64_T
AC_TYPE_PID_T

## ---------------------------------- ##
## Check for compiler characteristics ##
## ---------------------------------- ##
SANDIA_DETECT_COMPILER_TYPE
AC_C_CONST
AC_C_INLINE
AC_C_VOLATILE
SANDIA_UNUSED_ATTRIBUTE
SANDIA_NORETURN_ATTRIBUTE
SANDIA_BUILTIN_UNREACHABLE
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
   CFLAGS="-Wall -wd981 $CFLAGS"
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

SANDIA_CHECK_ATOMICS([],[AC_MSG_ERROR([Atomics are not implemented portably])])
SANDIA_CHECK_BITFIELDS

AC_CACHE_SAVE

## --------------------------- ##
## Check for library functions ##
## --------------------------- ##

AC_FUNC_FORK
AC_FUNC_MALLOC
AC_FUNC_MMAP
AC_CHECK_FUNCS([munmap]) # how absurd is this?
AC_CHECK_FUNCS([memalign posix_memalign], [break]) # first win
AC_CHECK_FUNCS([getpagesize]) # not mandatory
AC_CHECK_FUNCS([ftruncate memset setenv strtol], [],
			   [AC_MSG_ERROR([required function not found])])
AC_CHECK_LIB([bsd-compat],[main]) # potentially important because we use BSD functions like ftruncate
AC_SEARCH_LIBS([shm_open],
	       [rt], [],
	       [AC_MSG_ERROR([Kick the lazy developer and make him support normal file mmaping])])
AC_SEARCH_LIBS([hwloc_topology_init], [hwloc],
			   [AC_DEFINE([HAVE_HWLOC],[1],[Define if hwloc is available])])
SANDIA_CHOOSE_TIMER

## -------------------------- ##
## Check for library behavior ##
## -------------------------- ##
SANDIA_CHECK_8ALIGNED_MALLOC
SANDIA_CHECK_8ALIGNED_CALLOC
SANDIA_CHECK_16ALIGNED_MALLOC
SANDIA_CHECK_16ALIGNED_CALLOC
SANDIA_CHECK_WORKING_VALLOC
AS_IF([test "x$enable_hard_polling" != xyes],
	  [SANDIA_CHECK_PTHREAD_PROCESS_SHARED([AC_DEFINE([HAVE_PTHREAD_SHMEM_LOCKS],
										   			  [1],
													  [Define if PTHREAD_PROCESS_SHARED attribute on mutexes and cond variables works])])])

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
