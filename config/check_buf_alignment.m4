dnl If triggered me is used, a certain alignment of the members of the
dnl ptl_buf struct is required. This test checks for this alignment.

AC_DEFUN([CHECK_BUF_ALIGNMENT], [
  saved_CPPFLAGS="$CPPFLAGS"
  saved_LDLAGS="$LDFLAGS"
  saved_LIBS="$LIBS"
  CPPFLAGS="$CPPFLAGS -I${srcdir}/src/ib/ -I${srcdir}/include/ -I${top_srcdir}/include ${ev_CPPFLAGS} ${ofed_CPPFLAGS} ${XPMEM_CPPFLAGS}" 
  LDFLAGS="$LDFLAGS ${ev_LDFLAGS} ${ofed_LDFLAGS} ${XPMEM_LDFLAGS}"
  LIBS="$LIBS ${ev_LIBS} ${ofed_LIBS} ${XPMEM_LIBS}"
AC_CACHE_CHECK([if buf is aligned right for triggered me],
  [ptl_cv_buf_aligned_for_trig_me],
  [AC_TRY_RUN([
#include <stdio.h>
#include <stddef.h>
#include <assert.h>

#include "ptl_loc.h"
#include "ptl_buf.h"

#define member_size(type, member) sizeof(((type *)0)->member)
#define CHECK_ALIGNED(structname, m1, m2, m3, m4)                 {\
  if (                                                             \
   (offsetof(structname, m1) != offsetof(structname, m2)) ||       \
   (offsetof(structname, m1) != offsetof(structname, m3)) ||       \
   (offsetof(structname, m1) != offsetof(structname, m4))) {       \
	 printf("%s, %s, %s, %s in %s not lined up, adjust tme_pad1, tme_pad2\n", \
	         #m1 , #m2 , #m3 , #m4 , #structname );                \
	 exit(EXIT_FAILURE);                                           \
                                                             }     \
} /* CHECK_ALIGNED*/

int main(int argc, char** argv) { 
#ifdef WITH_TRIG_ME_OPS
 	CHECK_ALIGNED(buf_t, recv_buf, indir_sge, ct_event, me_init);
	CHECK_ALIGNED(buf_t, put_md, me, le, ptl_list);
#endif
	exit(EXIT_SUCCESS);
}
      
],
     [ptl_cv_buf_aligned_for_trig_me=yes],
     [ptl_cv_buf_aligned_for_trig_me=no],
     [ptl_cv_buf_aligned_for_trig_me=no])
  ])
if test "$ptl_cv_buf_aligned_for_trig_me" = yes ; then
  AC_MSG_RESULT([yes])
else
  AC_MSG_RESULT([no])
  AC_MSG_ERROR(["fix the padding in buf_t or build without triggered me ops"])
fi
CPPFLAGS="$saved_CPPFLAGS"
LDFLAGS="$saved_LDFLAGS"
LIBS="$saved_LIBS"
])

