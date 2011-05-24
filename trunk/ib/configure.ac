#                                               -*- Autoconf -*-
# Process this file with autoconf to produce a configure script.

AC_PREREQ(2.65)
AC_INIT([portals],[4.0])
AM_INIT_AUTOMAKE([1.7])
LT_PREREQ([2.2])
LT_INIT([dlopen])
AC_CONFIG_HEADERS([config.h])

AC_CONFIG_MACRO_DIR([m4])

# Use silent rules if supported.
m4_ifdef([AM_SILENT_RULES], [AM_SILENT_RULES([yes])])

# Checks for programs.
AC_PROG_CXX
AC_PROG_AWK
AC_PROG_CC
AC_PROG_CPP
AC_PROG_INSTALL
AC_PROG_LN_S
AC_PROG_MAKE_SET
AM_PROG_CC_C_O

#
AC_ARG_ENABLE([fast],
              [AS_HELP_STRING([--enable-fast],
                              [Sets the available configuration options to run as quickly as possible. This makes assumptions and CAN result in bad things happening.])])

AS_IF([test "x$enable_fast" == xyes],
      [CPPFLAGS="$CPPFLAGS -DNDEBUG"
       CFLAGS="-O3 $CFLAGS"
       enable_arg_checking=no
       enable_strict_uidjid=no
       enable_hard_polling=yes
       enable_register_on_bind=yes
       AS_IF([test "x$enable_debug_symbols" == xyes],
             [CFLAGS="-g $CFLAGS"])])
AC_ARG_ENABLE([arg-checking],
              [AS_HELP_STRING([--disable-arg-checking],
                              [turns off argument checking; does not strictly conform to Portals4 standard, but avoids unnecessary work in the common case])])
AS_IF([test "x$enable_arg_checking" == xno],
      [AC_DEFINE([NO_ARG_VALIDATION], [1], [Define to disable argument checking])])

# Checks for libraries.
AC_CHECK_LIB(ev, ev_loop_new,,AC_MSG_ERROR([libev not found.])) 

# Checks for header files.
AC_CHECK_HEADERS([arpa/inet.h limits.h netinet/in.h stddef.h stdint.h stdlib.h string.h sys/file.h sys/socket.h unistd.h ev.h])

# Checks for typedefs, structures, and compiler characteristics.
AC_C_INLINE
AC_TYPE_OFF_T
AC_TYPE_PID_T
AC_TYPE_SIZE_T
AC_TYPE_SSIZE_T
AC_TYPE_UINT16_T
AC_TYPE_UINT32_T
AC_TYPE_UINT64_T
AC_TYPE_UINT8_T

# Checks for library functions.
AC_FUNC_FORK
AC_FUNC_MALLOC
AC_FUNC_MMAP
AC_FUNC_REALLOC
AC_FUNC_STRERROR_R
AC_CHECK_FUNCS([ftruncate getpagesize inet_ntoa memset select socket strerror strtol strtoul])

# Only export the library symbols 
AC_CACHE_CHECK(whether ld accepts --version-script, ac_cv_version_script,
    [if test -n "`$LD --help < /dev/null 2>/dev/null | grep version-script`"; then
        LD_VERSION_SCRIPT='-Wl,--version-script=$(srcdir)/portals4.map'
    else
        LD_VERSION_SCRIPT=
    fi])
AC_SUBST(LD_VERSION_SCRIPT)

AC_CONFIG_FILES([Makefile
		 include/Makefile
                 src/Makefile
		 test/Makefile])
AC_OUTPUT
