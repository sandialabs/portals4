dnl -*- Autoconf -*-
dnl
dnl Copyright (c)      2010  Sandia Corporation
dnl
dnl
dnl SANDIA_CHECK_BITFIELDS
dnl ------------------------------------------------------------------------------
AC_DEFUN([SANDIA_CHECK_BITFIELDS], [
AC_CACHE_CHECK([bitfield ordering],
    [sandia_cv_bitfield_order],
    [AC_RUN_IFELSE(
	   [AC_LANG_PROGRAM([[
union foo {
	unsigned int w;
	struct bar {
		unsigned a : 28;
		unsigned b : 3;
		unsigned c : 1;
	} s;
} fb;]],
[[
fb.w = 0;
fb.s.c = 1;
if (fb.w == 1) { return 0; } else { return 1; }]])],
	[sandia_cv_bitfield_order="forward"],
	[sandia_cv_bitfield_order="reverse"],
	[sandia_cv_ucstack_ssflags="assuming reverse"])])
AS_IF([test "$sandia_cv_bitfield_order" == forward],
	  [AC_DEFINE([BITFIELD_ORDER_FORWARD], [1], [Define if bitfields are in forward order])],
	  [AC_DEFINE([BITFIELD_ORDER_REVERSE], [1], [Define if bitfields are in reverse order])])
])
