# -*- Makefile -*-
#
# Copyright (c) 2011 Sandia Corporation

AM_CPPFLAGS = -I$(top_srcdir)/include $(XPMEM_CPPFLAGS)

EXTRA_DIST = \
	lib/ptmalloc/arena.c \
	lib/ptmalloc/hooks.c \
	lib/ptmalloc/ChangeLog \
	lib/ptmalloc/COPYRIGHT \
	lib/ptmalloc/README \
	lib/ptmalloc/tst-mallocstate.c \
	lib/ptmalloc/tst-mstats.c \
	lib/ptmalloc/t-test1.c \
	lib/ptmalloc/t-test2.c \
	lib/ptmalloc/t-test.h \
	lib/ptmalloc/sysdeps/sproc/thread-st.h \
	lib/ptmalloc/sysdeps/sproc/malloc-machine.h \
	lib/ptmalloc/sysdeps/solaris/thread-st.h \
	lib/ptmalloc/sysdeps/solaris/malloc-machine.h

bin_PROGRAMS = portals_engine
noinst_LTLIBRARIES = libportals_mc.la libportals_mc_alloc.la libportals_mc_shared.la

portals_engine_SOURCES = ppe/ppe.c
portals_engine_LDFLAGS = $(XPMEM_LDFLAGS)
portals_engine_LDADD = libportals_mc_shared.la $(XPMEM_LIBS)

libportals_mc_la_SOURCES = \
	lib/alloc.h \
	lib/alloc.c \
	lib/init.c
libportals_mc_la_LDFLAGS = $(XPMEM_LDFLAGS)
libportals_mc_la_LIBADD = libportals_mc_shared.la libportals_mc_alloc.la $(XPMEM_LIBS)

libportals_mc_alloc_la_SOURCES = \
	lib/ptmalloc/lran2.h \
	lib/ptmalloc/malloc.c \
	lib/ptmalloc/malloc.h \
	lib/ptmalloc/malloc-stats.h \
	lib/ptmalloc/sysdeps/pthread/thread-st.h \
	lib/ptmalloc/sysdeps/pthread/malloc-machine.h \
	lib/ptmalloc/sysdeps/generic/atomic.h \
	lib/ptmalloc/sysdeps/generic/thread-st.h \
	lib/ptmalloc/sysdeps/generic/malloc-machine.h
libportals_mc_alloc_la_CPPFLAGS = \
	$(AM_CPPFLAGS) \
	-I$(top_srcdir)/src/mc/lib/ptmalloc/sysdeps/pthread \
	-I$(top_srcdir)/src/mc/lib/ptmalloc/sysdeps/generic \
	-I$(top_srcdir)/src/mc/lib/ptmalloc \
	-DUSE_TSD_DATA_HACK

libportals_mc_shared_la_SOURCES = \
	command_queue.h \
	command_queue_xpmem.c
