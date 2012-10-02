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
AC_INIT([portals4], [1.0a1], [portals4-devel@googlegroups.com])
AC_CONFIG_AUX_DIR([config])
AC_CONFIG_MACRO_DIR([config])

dnl Automake's silent rules were implemented in the same version that
dnl color-tests was implemented, so we can use one to detect the other.
dnl This nasty, dirty, unreliable trick is strongly discouraged by its author:
dnl http://blog.flameeyes.eu/trackbacks?article_id=5155
m4_ifdef([AM_SILENT_RULES],
           [m4_define([ptl_color_tests], [color-tests])],
           [m4_define([ptl_color_tests], [])])
AM_INIT_AUTOMAKE([foreign check-news subdir-objects dist-bzip2 no-define tar-ustar 1.7 -Wall -Werror ]ptl_color_tests)
dnl If Automake supports silent rules, enable them
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
  [Enable extra compiler warnings (for developers of portals)])])
AS_IF([test "x$enable_picky" == x],
  [AS_IF([test -d "${srcdir}/.svn"],
     [echo "--> developer override: enable picky compiler by default"
      enable_picky=yes])])

AC_ARG_ENABLE([fast],
  [AS_HELP_STRING([--enable-fast],
     [Sets the available configuration options to run as quickly as possible. This makes assumptions and CAN result in bad things happening.])])
AS_IF([test "x$enable_fast" == xyes],
  [CPPFLAGS="$CPPFLAGS -DNDEBUG"
   CFLAGS="-O3 $CFLAGS"
   enable_arg_checking=no
   enable_strict_uid=no
   enable_hard_polling=yes
   enable_register_on_bind=yes])

AC_ARG_ENABLE([arg-checking],
  [AS_HELP_STRING([--disable-arg-checking],
    [Disable argument checking in Portals library.])])
AS_IF([test "x$enable_arg_checking" == xno],
  [AC_DEFINE([NO_ARG_VALIDATION], [1], [Define to disable argument checking])])

AC_ARG_ENABLE([transfer-engine],
  [AS_HELP_STRING([--enable-transfer-engine],
    [Enables KNEM and other direct SHMEM support])])

AC_ARG_WITH([cacheline-width],
  [AS_HELP_STRING([--with-cacheline-width=bytes],
    [Specify the cacheline width for the target machine. Defaults to 64.])],
  [],
  [with_cacheline_width=64])
AC_MSG_CHECKING([cacheline width])
AC_MSG_RESULT([$with_cacheline_width])
AC_DEFINE_UNQUOTED([CACHELINE_WIDTH], [$with_cacheline_width], [The cacheline width])

AS_IF([test "x$enable_register_on_bind" == xyes],
	  [AC_DEFINE([REGISTER_ON_BIND], [1], [Define that makes XFE memory registration happen at MDBind time, rather than at data movement time.])])

dnl fix me
with_implementation=ib

AC_ARG_ENABLE([ib-shmem],
  [AS_HELP_STRING([--enable-ib-shmem],
    [Use Shared memory for local communication with IB implementation.])])
AS_IF([test "x$with_implementation" = "xib" -a "$enable_ib_shmem" == "yes"],
  [AC_DEFINE([WITH_TRANSPORT_SHMEM], [1], [Define to enable Shared memory / KNEM support])])
AM_CONDITIONAL(WITH_TRANSPORT_SHMEM, test "x$enable_ib_shmem" == xyes)

AC_ARG_ENABLE([ib-udp],
  [AS_HELP_STRING([--enable-ib-udp],
    [Use UDP for remote communication with IB implementation.])])
AS_IF([test "x$with_implementation" = "xib" -a "$enable_ib_udp" == "yes"],
  [AC_DEFINE([WITH_TRANSPORT_UDP], [1], [Define to enable UDP support])])
AM_CONDITIONAL(WITH_TRANSPORT_UDP, test "x$enable_ib_udp" == xyes)

AC_ARG_ENABLE([ib-ib],
  [AS_HELP_STRING([--disable-ib-ib],
    [Disable IB for communication.])])
AS_IF([test "x$with_implementation" = "xib" -a x"$enable_ib_ib" != xno],
  [AC_DEFINE([WITH_TRANSPORT_IB], [1], [Define to enable IB support])])
AM_CONDITIONAL(WITH_TRANSPORT_IB, test "x$enable_ib_ib" != xno)

AC_ARG_ENABLE([ib-ppe],
  [AS_HELP_STRING([--enable-ib-ppe],
    [Enable IB/PPE for communication.])])
AS_IF([test "x$with_implementation" = "xib" -a "$enable_ib_ppe" == "yes"],
  [AC_DEFINE([WITH_PPE], [1], [Define to enable PPE / XPMEM])])
AM_CONDITIONAL(WITH_PPE, test "x$enable_ib_ppe" == xyes)


## ------------------- ##
## Checks for programs ##
## ------------------- ##
AC_PROG_AWK
AC_PROG_CC
AC_USE_SYSTEM_EXTENSIONS
m4_ifdef([AM_PROG_AR],[AM_PROG_AR])
AC_PROG_CC_C99
AC_PROG_CPP
AC_PROG_INSTALL
AC_PROG_LN_S
AC_PROG_MAKE_SET
AC_PROG_LIBTOOL
AM_PROG_CC_C_O

LT_PREREQ([2.2])
LT_INIT([])

AC_CACHE_SAVE

## ----------------------- ##
## Checks for header files ##
## ----------------------- ##
AC_HEADER_STDC
AC_SYS_LARGEFILE
AC_CHECK_HEADERS([fcntl.h stddef.h stdint.h malloc.h sys/time.h limits.h endian.h])
AC_CHECK_HEADERS([sys/posix_shm.h], [], [],
[#ifdef HAVE_SYS_TIME_H
# include <sys/time.h>
#endif
])
AC_CHECK_HEADERS([arpa/inet.h limits.h netinet/in.h stddef.h \
        stdint.h stdlib.h string.h sys/file.h sys/socket.h \
        unistd.h syscall.h])

AM_PATH_XML2([2.6.0], [have_libxml=1], [have_libxml=0])
AM_CONDITIONAL([HAVE_LIBXML], [test "$have_libxml" = "1"])

## --------------- ##
## Check for types ##
## --------------- ##
AC_C_INLINE
AC_TYPE_OFF_T
AC_TYPE_SIZE_T
AC_TYPE_SSIZE_T
AC_TYPE_UINT8_T
AC_TYPE_UINT16_T
AC_TYPE_UINT32_T
AC_TYPE_UINT64_T
AC_TYPE_PID_T
AC_CHECK_TYPES([uint_fast8_t, uint_fast32_t, uint_fast64_t])

## ---------------------------------- ##
## Check for compiler characteristics ##
## ---------------------------------- ##
SANDIA_DETECT_COMPILER_TYPE
AC_C_CONST
AC_C_INLINE
AC_C_VOLATILE
SANDIA_ALIGNED_ATTRIBUTE
SANDIA_UNUSED_ATTRIBUTE
SANDIA_NORETURN_ATTRIBUTE
SANDIA_BUILTIN_UNREACHABLE
# Find out if we need the -restrict flag
RESTRICT_CXXFLAGS=""
AS_IF([test "x$sandia_cv_cxx_compiler_type" = "xIntel"],
	  [CXXFLAGS="-restrict $CXXFLAGS"])

AX_C_RESTRICT
AS_IF([test "x$enable_picky" == xyes],
  [AS_CASE([$sandia_cv_c_compiler_type],
		   [GNU], [CFLAGS="-Wall -Wno-strict-aliasing -Wmissing-prototypes -Wstrict-prototypes $CFLAGS"],
		   [Intel], [CFLAGS="-Wall -wd981 $CFLAGS"],
		   [SunStudio], [], dnl This compiler defaults to enabling all warnings
		   [PortlandGroup], [CFLAGS="-Minform=inform $CFLAGS"],
		   [CFLAGS="-Wall $CFLAGS"])
   AS_CASE([$sandia_cv_cxx_compiler_type],
		   [GNU], [CXXFLAGS="-Wall $CXXFLAGS"],
		   [Intel], [CXXFLAGS="-Wall -wd981 $CXXFLAGS"],
		   [SunStudio], [], dnl This compiler defaults to enabling all warnings
		   [PortlandGroup], [CXXFLAGS="-Minform=inform $CXXFLAGS"],
		   [CXXFLAGS="-Wall $CXXFLAGS"])
   ])

SANDIA_CHECK_ATOMICS([],[AC_MSG_ERROR([Atomics are not implemented portably])])
SANDIA_CHECK_BITFIELDS

AC_CACHE_SAVE

## --------------------------- ##
## Check for library functions ##
## --------------------------- ##

AC_FUNC_FORK
AC_FUNC_STRERROR_R
AC_FUNC_MMAP
AC_CHECK_FUNCS([syscall __munmap __mmap])
AC_CHECK_FUNCS([munmap]) # how absurd is this?
AC_CHECK_FUNCS([memalign posix_memalign], [break]) # first win
AC_CHECK_FUNCS([getpagesize tdestroy linux/ioctl.h]) # not mandatory
AC_CHECK_FUNCS([ftruncate getpagesize inet_ntoa memset select socket strerror strtol strtoul])
AC_CHECK_LIB([dl], [dlsym])
AC_CHECK_FUNCS([dlsym])

AC_SEARCH_LIBS([pthread_spin_init],[pthread],
			   [AC_DEFINE([HAVE_PTHREAD_SPIN_INIT], [1], [Define if pthread supports spinlocks])]) # not mandatory either, just faster
AC_CHECK_FUNCS([ftruncate memset setenv strtol strtoul select inet_ntoa socket strerror], [],
			   [AC_MSG_ERROR([required function not found])])
AC_CHECK_LIB([bsd-compat],[main]) # potentially important because we use BSD functions like ftruncate
AS_IF([test "x$enable_papi" != x],
	  [AC_CHECK_LIB([papi], [PAPI_library_init], [],
					[AC_MSG_ERROR([Cannot find libpapi])])])
AC_SEARCH_LIBS([shm_open],
	       [rt], [],
	       [AC_MSG_ERROR([Kick the lazy developer and make him support normal file mmaping])])
AC_SEARCH_LIBS([hwloc_topology_init], [hwloc],
			   [AC_DEFINE([HAVE_HWLOC],[1],[Define if hwloc is available])])
SANDIA_CHOOSE_TIMER
AS_IF([test "x$enable_transfer_engine" != xno],
	  [SANDIA_DETECT_KNEM([enable_transfer_engine=yes],
						  [AS_IF([test "x$enable_transfer_engine" == xyes],
								 [AC_MSG_ERROR([No transfer engine detected.])],
								 [enable_transfer_engine=no])])])
AS_IF([test "x$enable_transfer_engine" == xyes],
	  [AC_DEFINE([USE_TRANSFER_ENGINE], [1], [Define to enable the use of a transfer engine (e.g. KNEM)])])

AS_IF([test "$with_implementation" = "ib"],
	  [SANDIA_CHECK_EV([], [AC_MSG_ERROR([libev not found.])])])

AS_IF([test "$with_implementation" = "ib" -a x"$enable_ib_ib" != xno],
	  [SANDIA_CHECK_OFED([], [AC_MSG_ERROR([OFED not found.])])])

AS_IF([test "$with_implementation" = "mc"],
  [SANDIA_CHECK_XPMEM([], [AC_MSG_ERROR([libxpmem not found.])])])

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

# Only export the library symbols 
AS_IF([test "$with_implementation" = "ib"], [
AC_CACHE_CHECK(whether ld accepts --version-script, ac_cv_version_script,
    [AS_IF([test -n "`$LD --help < /dev/null 2>/dev/null | grep version-script`"],
           [LD_VERSION_SCRIPT='-Wl,--version-script=$(srcdir)/ib/portals4.map'],
		   [LD_VERSION_SCRIPT=''])])
AC_SUBST(LD_VERSION_SCRIPT)
])

## --------------- ##
## Output and done ##
## --------------- ##

AM_CONDITIONAL([BACKEND_IMPL_IB], [test "$with_implementation" = "ib"])
AM_CONDITIONAL([BACKEND_IMPL_MC], [test "$with_implementation" = "mc"])

DISTCHECK_CONFIGURE_FLAGS=
AS_IF([test -n "$with_implementation"],
  [DISTCHECK_CONFIGURE_FLAGS="$DISTCHECK_CONFIGURE_FLAGS --with-implementation=${with_implementation}"])
AS_IF([test -n "$enable_ib_shmem"],
  [DISTCHECK_CONFIGURE_FLAGS="$DISTCHECK_CONFIGURE_FLAGS --enable-ib-shmem=${enable_ib_shmem}"])
AS_IF([test -n "$enable_ib_ib"],
  [DISTCHECK_CONFIGURE_FLAGS="$DISTCHECK_CONFIGURE_FLAGS --enable-ib-ib=${enable_ib_ib}"])
AS_IF([test -n "$with_xpmem"],
  [DISTCHECK_CONFIGURE_FLAGS="$DISTCHECK_CONFIGURE_FLAGS --with-xpmem=${with_xpmem}"])
AS_IF([test -n "$with_ev"],
  [DISTCHECK_CONFIGURE_FLAGS="$DISTCHECK_CONFIGURE_FLAGS --with-ev=${with_ev}"])
AS_IF([test -n "$with_ofed"],
  [DISTCHECK_CONFIGURE_FLAGS="$DISTCHECK_CONFIGURE_FLAGS --with-ofed=${with_ofed}"])
AC_SUBST(DISTCHECK_CONFIGURE_FLAGS)

AC_CONFIG_SUBDIRS([src/runtime/hydra])

AC_CONFIG_HEADERS([include/config.h])
AC_CONFIG_FILES([Makefile
                 doc/Makefile
                 include/Makefile
                 src/Makefile
                 src/ib/Makefile
		 src/runtime/Makefile
                 test/Makefile
                 test/sfw/Makefile
                 test/sfw/test_n1/Makefile
                 test/sfw/test_n2/Makefile])
AC_OUTPUT
