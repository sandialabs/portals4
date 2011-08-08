dnl -*- Autoconf -*-
dnl
dnl Copyright (c)      2011  Sandia Corporation
dnl
dnl
dnl SANDIA_CHECK_SHMGET
dnl ------------------------------------------------------------------------------
AC_DEFUN([SANDIA_CHECK_SHMGET], [
AC_CACHE_CHECK([shmctl IPC_RMID behavior],
    [sandia_cv_rmid_behavior],
    [AC_RUN_IFELSE(
	   [AC_LANG_PROGRAM([[
#include <stdint.h>
#include <stddef.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/shm.h>
#include <assert.h>
int ret;
struct shmid_ds buf;
int shmid;
void *attached = NULL;]],
[[
shmid = shmget(999, 4096, IPC_CREAT | IPC_EXCL | S_IRWXU);
assert(shmid != -1);
attached = shmat(shmid, NULL, 0);
ret = shmctl(shmid, IPC_RMID, NULL);
assert(ret == 0);
ret = shmctl(shmid, IPC_STAT, &buf);
if (ret == 0) {
  shmdt(attached);
  return 0; // Linux
} else {
  return 1; // MacOS X
}
]])],
	[sandia_cv_rmid_behavior="linux"],
	[sandia_cv_rmid_behavior="bsd"],
	[sandia_cv_rmid_behavior="bsd"])])
AS_IF([test "x$sandia_cv_rmid_behavior" == xlinux],
	  [AC_DEFINE([IPC_RMID_IS_CLEANUP], [1], [Define if bitfields are in forward order])])
])
