AC_DEFUN([SANDIA_CHECK_PTHREAD_PROCESS_SHARED],
[AC_CACHE_CHECK([if mutexes may be shared between processes],
  [sandia_cv_working_pthread_process_shared],
  [AC_TRY_RUN([
#include <pthread.h>

int main()
{
	pthread_mutexattr_t ma;
	pthread_condattr_t ca;
	if (pthread_mutexattr_init(&ma) != 0) return 1;
	if (pthread_mutexattr_setpshared(&ma, PTHREAD_PROCESS_SHARED) != 0) return 1;
	if (pthread_mutexattr_destroy(&ma) != 0) return 1;
        if (pthread_condattr_init(&ca) != 0) return 1;
        if (pthread_condattr_setpshared(&ca, PTHREAD_PROCESS_SHARED) != 0) return 1;
	if (pthread_condattr_destroy(&ma) != 0) return 1;
	return 0;
}
              ],
     [sandia_cv_working_pthread_process_shared=yes],
     [sandia_cv_working_pthread_process_shared=no],
     [sandia_cv_working_pthread_process_shared=no])
  ])
AS_IF([test "$sandia_cv_working_pthread_process_shared" = yes],
	  [AC_DEFINE([HAVE_WORKING_PTHREAD_PROCESS_SHARED], [1],
		  [Define if pthread_*attr_setpshared(attr, PTHREAD_PROCESS_SHARED) works.])
	   $1],
	  [$2])
])
