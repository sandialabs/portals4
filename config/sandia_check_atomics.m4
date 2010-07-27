# -*- Autoconf -*-
#
# Copyright (c)      2010  Sandia Corporation
#
# Largely stolen from Qthreads
#

# SANDIA_CHECK_ATOMICS([action-if-found], [action-if-not-found])
# ------------------------------------------------------------------------------
AC_DEFUN([SANDIA_CHECK_ATOMICS], [
AC_ARG_ENABLE([builtin-atomics],
     [AS_HELP_STRING([--disable-builtin-atomics],
	                 [force the use of inline-assembly (if possible) rather than compiler-builtins for atomics. This is useful for working around some compiler bugs; normally, it's preferable to use compiler builtins.])])
AS_IF([test "x$enable_builtin_atomics" != xno], [
AC_CHECK_HEADERS([ia64intrin.h ia32intrin.h])
AC_CACHE_CHECK([whether compiler supports builtin atomic CAS-32],
  [sandia_cv_atomic_CAS32],
  [AC_LINK_IFELSE([AC_LANG_SOURCE([[
#ifdef HAVE_IA64INTRIN_H
# include <ia64intrin.h>
#elif HAVE_IA32INTRIN_H
# include <ia32intrin.h>
#endif
#include <stdlib.h>
#include <stdint.h> /* for uint32_t */

int main()
{
uint32_t bar=1, old=1, new=2;
uint32_t foo = __sync_val_compare_and_swap(&bar, old, new);
return (int)foo;
}]])],
		[sandia_cv_atomic_CAS32="yes"],
		[sandia_cv_atomic_CAS32="no"])])
AC_CACHE_CHECK([whether compiler supports builtin atomic CAS-64],
  [sandia_cv_atomic_CAS64],
  [AC_LINK_IFELSE([AC_LANG_SOURCE([[
#ifdef HAVE_IA64INTRIN_H
# include <ia64intrin.h>
#elif HAVE_IA32INTRIN_H
# include <ia32intrin.h>
#endif
#include <stdlib.h>
#include <stdint.h> /* for uint64_t */

int main()
{
uint64_t bar=1, old=1, new=2;
uint64_t foo = __sync_val_compare_and_swap(&bar, old, new);
return foo;
}]])],
		[sandia_cv_atomic_CAS64="yes"],
		[sandia_cv_atomic_CAS64="no"])])
AC_CACHE_CHECK([whether compiler supports builtin atomic CAS-ptr],
  [sandia_cv_atomic_CASptr],
  [AC_LINK_IFELSE([AC_LANG_SOURCE([[
#ifdef HAVE_IA64INTRIN_H
# include <ia64intrin.h>
#elif HAVE_IA32INTRIN_H
# include <ia32intrin.h>
#endif
#include <stdlib.h>

int main()
{
void *bar=(void*)1, *old=(void*)1, *new=(void*)2;
void *foo = __sync_val_compare_and_swap(&bar, old, new);
return (int)(long)foo;
}]])],
		[sandia_cv_atomic_CASptr="yes"],
		[sandia_cv_atomic_CASptr="no"])])
AS_IF([test "x$sandia_cv_atomic_CAS32" = "xyes" && test "x$sandia_cv_atomic_CAS64" = "xyes" && test "x$sandia_cv_atomic_CASptr" = "xyes"],
	  [sandia_cv_atomic_CAS=yes],
	  [sandia_cv_atomic_CAS=no])
AC_CACHE_CHECK([whether compiler supports builtin atomic incr],
  [sandia_cv_atomic_incr],
  [AC_LINK_IFELSE([AC_LANG_SOURCE([[
#ifdef HAVE_IA64INTRIN_H
# include <ia64intrin.h>
#elif HAVE_IA32INTRIN_H
# include <ia32intrin.h>
#endif
#include <stdlib.h>

int main()
{
long bar=1;
long foo = __sync_fetch_and_add(&bar, 1);
return foo;
}]])],
		[sandia_cv_atomic_incr="yes"],
		[sandia_cv_atomic_incr="no"])
   ])
AS_IF([test "$sandia_cv_atomic_CAS" = "yes"],
	  [AC_CACHE_CHECK([whether ia64intrin.h is required],
	    [sandia_cv_require_ia64intrin_h],
		[AC_LINK_IFELSE([AC_LANG_SOURCE([[
#include <stdlib.h>

int main()
{
long bar=1, old=1, new=2;
long foo = __sync_val_compare_and_swap(&bar, old, new);
return foo;
}]])],
		[sandia_cv_require_ia64intrin_h="no"],
		[sandia_cv_require_ia64intrin_h="yes"])])])
])
AC_CACHE_CHECK([whether the compiler supports CMPXCHG16B],
  [sandia_cv_cmpxchg16b],
  [AC_LINK_IFELSE([AC_LANG_SOURCE([[
#include <stdint.h> /* for uint64_t */
struct m128 {
uint64_t a,b;
} one, two, tmp;
int main(void)
{
one.a = 1;
one.b = 2;
two.a = 3;
two.b = 4;
tmp.a = 5;
tmp.b = 6;
__asm__ __volatile__ ("lock cmpxchg16b (%2)"
:"=a"(tmp.a),"=d"(tmp.b)
:"r"(&two),"a"(two.a),"d"(two.b),"b"(one.a),"c"(one.b)
:"memory");
return tmp.a;
}]])],
    [sandia_cv_cmpxchg16b="yes"],
	[sandia_cv_cmpxchg16b="no"])])
AS_IF([test "x$sandia_cv_cmpxchg16b" = "xyes"],
[AC_CACHE_CHECK([whether the CPU supports CMPXCHG16B],
  [sandia_cv_cpu_cmpxchg16b],
  [AC_ARG_ENABLE([cross-cmpxchg16b],
     [AS_HELP_STRING([--enable-cross-cmpxchg16b],
	     [when cross compiling, ordinarily we asume that cmpxchg16b is not available, however this option forces us to assume that cmpxchg16b IS available])])
   AC_RUN_IFELSE([AC_LANG_SOURCE([[
#include <stdint.h> /* for uint64_t */
struct m128 {
uint64_t a,b;
} one, two, tmp;
int main(void)
{
one.a = 1;
one.b = 2;
two.a = 3;
two.b = 4;
tmp.a = 5;
tmp.b = 6;
__asm__ __volatile__ ("lock cmpxchg16b (%2)"
:"=a"(tmp.a),"=d"(tmp.b)
:"r"(&two),"a"(two.a),"d"(two.b),"b"(one.a),"c"(one.b)
:"memory");
return 0;
}]])],
    [sandia_cv_cpu_cmpxchg16b="yes"],
	[sandia_cv_cpu_cmpxchg16b="no"],
	[AS_IF([test "x$enable_cross_cmpxchg16b" == "xyes"],
	       [sandia_cv_cpu_cmpxchg16b="assuming yes"],
	       [sandia_cv_cpu_cmpxchg16b="assuming no"])])])
		   ])
AS_IF([test "x$sandia_cv_cpu_cmpxchg16b" == "xassuming yes"],[sandia_cv_cpu_cmpxchg16b="yes"])
AS_IF([test "x$sandia_cv_cpu_cmpxchg16b" == "xyes"],
      [AC_DEFINE([HAVE_CMPXCHG16B],[1],[if the compiler and cpu can both handle the 128-bit CMPXCHG16B instruction])])
AS_IF([test "$sandia_cv_require_ia64intrin_h" = "yes"],
	  [AC_DEFINE([SANDIA_NEEDS_INTEL_INTRIN],[1],[if this header is necessary for builtin atomics])])
AS_IF([test "x$sandia_cv_atomic_CASptr" = "xyes"],
      [AC_DEFINE([SANDIA_BUILTIN_CAS_PTR],[1],
	  	[if the compiler supports __sync_val_compare_and_swap on pointers])])
AS_IF([test "x$sandia_cv_atomic_CAS32" = "xyes"],
      [AC_DEFINE([SANDIA_BUILTIN_CAS32],[1],
	  	[if the compiler supports __sync_val_compare_and_swap on 32-bit ints])])
AS_IF([test "x$sandia_cv_atomic_CAS64" = "xyes"],
      [AC_DEFINE([SANDIA_BUILTIN_CAS64],[1],
	  	[if the compiler supports __sync_val_compare_and_swap on 64-bit ints])])
AS_IF([test "x$sandia_cv_atomic_CAS" = "xyes"],
	[AC_DEFINE([SANDIA_BUILTIN_CAS],[1],[if the compiler supports __sync_val_compare_and_swap])])
AS_IF([test "$sandia_cv_atomic_incr" = "yes"],
	[AC_DEFINE([SANDIA_BUILTIN_INCR],[1],[if the compiler supports __sync_fetch_and_add])])
AS_IF([test "$sandia_cv_atomic_CAS" = "yes" -a "$sandia_cv_atomic_incr" = "yes"],
  		[AC_DEFINE([SANDIA_ATOMIC_BUILTINS],[1],[if the compiler supports __sync_val_compare_and_swap])
		 $1],
		[$2])
])
