# vim:ft=automake

noinst_LTLIBRARIES = libtestsupport.la

libtestsupport_la_SOURCES = support/support.h support/support.c
libtestsupport_la_LIBADD = $(top_builddir)/src/libportals.la

if TEST_RUNTIME_SHMEMRT
AM_CPPFLAGS += -I$(top_srcdir)/src/shmem
libtestsupport_la_SOURCES += support/shmemrt.c
endif

if TEST_RUNTIME_MPI
libtestsupport_la_SOURCES += support/mpi.c
endif

if TEST_RUNTIME_SINGLE
libtestsupport_la_SOURCES += support/single.c
endif
