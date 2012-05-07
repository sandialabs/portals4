dnl based on http://autoconf-archive.cryp.to/ax_check_page_aligned_malloc.html
dnl but different (obviously)
AC_DEFUN([SANDIA_CHECK_8ALIGNED_MALLOC],
[AC_CACHE_CHECK([if mallocs guarantee 8-byte alignment],
  [sandia_cv_func_malloc_8aligned],
  [AC_TRY_RUN([
#include <stdio.h>
#include <stdlib.h>
#if HAVE_UNISTD_H
# include <unistd.h>
#endif

int main()
{
  int i;

  for (i=0; i<100; i++)
    if ((unsigned long)malloc(9) & (7))
      exit (1);
  for (i=0; i<100; i++)
    if ((unsigned long)malloc(1) & (7))
      exit (1);
  exit (0);
}
              ],
     [sandia_cv_func_malloc_8aligned=yes],
     [sandia_cv_func_malloc_8aligned=no],
     [sandia_cv_func_malloc_8aligned=no])
  ])
if test "$sandia_cv_func_malloc_8aligned" = yes ; then
  AC_DEFINE([HAVE_8ALIGNED_MALLOC], [1],
    [Define if `malloc'ing more than 8 bytes always returns a 8-byte-aligned address (common practice on most libcs).])
fi
])
AC_DEFUN([SANDIA_CHECK_8ALIGNED_CALLOC],
[AC_CACHE_CHECK([if callocs guarantee 8-byte alignment],
  [sandia_cv_func_calloc_8aligned],
  [AC_TRY_RUN([
#include <stdio.h>
#include <stdlib.h>
#if HAVE_UNISTD_H
# include <unistd.h>
#endif

int main()
{
  int i;

  for (i=0; i<100; i++)
    if ((unsigned long)calloc(1,9) & (7))
      exit (1);
  for (i=0; i<100; i++)
    if ((unsigned long)calloc(1,1) & (7))
      exit (1);
  exit (0);
}
              ],
     [sandia_cv_func_calloc_8aligned=yes],
     [sandia_cv_func_calloc_8aligned=no],
     [sandia_cv_func_calloc_8aligned=no])
  ])
if test "$sandia_cv_func_calloc_8aligned" = yes ; then
  AC_DEFINE([HAVE_8ALIGNED_CALLOC], [1],
    [Define if `calloc'ing more than 8 bytes always returns a 8-byte-aligned address (common practice on most libcs).])
fi
])
