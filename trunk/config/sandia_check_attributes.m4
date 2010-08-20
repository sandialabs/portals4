# -*- Autoconf -*-
#
# Copyright (c)      2008  Sandia Corporation
#

# SANDIA_ALIGNED_ATTRIBUTE([action-if-found], [action-if-not-found])
# -------------------------------------------------------------------------
AC_DEFUN([SANDIA_ALIGNED_ATTRIBUTE],[dnl
AC_CACHE_CHECK(
 [support for aligned data declarations],
 [sandia_cv_aligned_attr],
 [AC_COMPILE_IFELSE([AC_LANG_SOURCE([[
int foo __attribute__((aligned(64)));
int f(int i) { foo = 1; return foo; }]])],
 [sandia_cv_aligned_attr=yes],
 [sandia_cv_aligned_attr=no])])
 AS_IF([test "x$sandia_cv_aligned_attr" = xyes],
 	   [AC_DEFINE([SANDIA_ALIGNEDDATA_ALLOWED], [1],
		   [specifying data alignment is allowed])])
 AS_IF([test "x$sandia_cv_aligned_attr" = xyes], [$1], [$2])
])

AC_DEFUN([SANDIA_MALLOC_ATTRIBUTE],[dnl
AC_CACHE_CHECK(
 [support for __attribute__((malloc))],
 [sandia_cv_malloc_attr],
 [AC_COMPILE_IFELSE([AC_LANG_SOURCE([[
__attribute__((malloc))
void * f(int i) { return malloc(i); }]])],
 [sandia_cv_malloc_attr=yes],
 [sandia_cv_malloc_attr=no])])
 AS_IF([test "x$sandia_cv_malloc_attr" = xyes],
 	   [defstr="__attribute__((malloc))"],
	   [defstr=""])
 AC_DEFINE_UNQUOTED([Q_MALLOC], [$defstr],
		   [if the compiler supports __attribute__((malloc))])
 AS_IF([test "x$sandia_cv_malloc_attr" = xyes], [$1], [$2])
])

AC_DEFUN([SANDIA_UNUSED_ATTRIBUTE],[dnl
AC_CACHE_CHECK(
 [support for __attribute__((unused))],
 [sandia_cv_unused_attr],
 [AC_COMPILE_IFELSE([AC_LANG_SOURCE([[
__attribute__((unused))
int f(int i) { return i; }]])],
 [sandia_cv_unused_attr=yes],
 [sandia_cv_unused_attr=no])])
 AS_IF([test "x$sandia_cv_unused_attr" = xyes],
 	   [unusedstr="__attribute__((unused))"],
	   [unusedstr=""])
 AC_DEFINE_UNQUOTED([Q_UNUSED], [$unusedstr],
		   [most gcc compilers know a function __attribute__((unused))])
 AS_IF([test "x$sandia_cv_unused_attr" = xyes], [$1], [$2])
])

AC_DEFUN([SANDIA_NOINLINE_ATTRIBUTE],[dnl
AC_CACHE_CHECK(
 [support for __attribute__((noinline))],
 [sandia_cv_noinline_attr],
 [AC_COMPILE_IFELSE([AC_LANG_SOURCE([[
__attribute__((noinline))
void * f(int i) { return malloc(i); }]])],
 [sandia_cv_noinline_attr=yes],
 [sandia_cv_noinline_attr=no])])
 AS_IF([test "x$sandia_cv_noinline_attr" = xyes],
 	   [defstr="__attribute__((noinline))"],
	   [defstr=""])
 AC_DEFINE_UNQUOTED([Q_NOINLINE], [$defstr],
		   [if the compiler supports __attribute__((NOINLINE))])
 AS_IF([test "x$sandia_cv_noinline_attr" = xyes], [$1], [$2])
])

AC_DEFUN([SANDIA_NORETURN_ATTRIBUTE],[dnl
AC_CACHE_CHECK(
 [support for __attribute__((noreturn))],
 [sandia_cv_noreturn_attr],
 [AC_COMPILE_IFELSE([AC_LANG_SOURCE([[
void * f(int i) __attribute__((noreturn)) { while (1) ; }]])],
 [sandia_cv_noreturn_attr=yes],
 [sandia_cv_noreturn_attr=no])])
 AS_IF([test "x$sandia_cv_noreturn_attr" = xyes],
 	   [defstr="__attribute__((noreturn))"],
	   [defstr=""])
 AC_DEFINE_UNQUOTED([Q_NORETURN], [$defstr],
		   [if the compiler supports __attribute__((noreturn))])
 AS_IF([test "x$sandia_cv_noreturn_attr" = xyes], [$1], [$2])
])

AC_DEFUN([SANDIA_BUILTIN_UNREACHABLE],[dnl
AC_CACHE_CHECK(
 [support for __builtin_unreachable()],
 [sandia_cv_builtin_unreachable],
 [AC_LINK_IFELSE([AC_LANG_SOURCE([[
void * f(int i) { while (1) ; __builtin_unreachable(); }]])],
 [sandia_cv_builtin_unreachable=yes],
 [sandia_cv_builtin_unreachable=no])])
 AS_IF([test "x$sandia_cv_builtin_unreachable" = xyes],
 	   [defstr="__builtin_unreachable()"],
	   [defstr=""])
 AC_DEFINE_UNQUOTED([UNREACHABLE], [$defstr],
		   [if the compiler supports __builtin_unreachable()))])
 AS_IF([test "x$sandia_cv_builtin_unreachable" = xyes], [$1], [$2])
])
