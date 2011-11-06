# -*- Makefile -*-
#
# Copyright (c) 2011 Sandia Corporation

AM_CPPFLAGS = \
	$(XPMEM_CPPFLAGS) \
	-I$(top_srcdir)/include

EXTRA_DIST =
SUBDIRS = . test

bin_PROGRAMS = portals_engine
noinst_LTLIBRARIES = libportals_mc.la libportals_mc_shared.la

portals_engine_SOURCES = ppe/ppe.c
portals_engine_LDFLAGS = $(XPMEM_LDFLAGS)
portals_engine_LDADD = libportals_mc_shared.la $(XPMEM_LIBS) -ldl -lpthread

libportals_mc_la_SOURCES = \
	lib/include/ptl_internal_pid.h \
	lib/include/ptl_internal_EQ.h \
	lib/include/ptl_internal_handles.h \
	lib/include/ptl_internal_LE.h \
	lib/include/ptl_internal_nit.h \
	lib/include/ptl_internal_MD.h \
	lib/include/ptl_visibility.h \
	lib/include/ptl_internal_CT.h \
	lib/include/ptl_internal_error.h \
	lib/include/ptl_internal_PT.h \
	lib/bundle.c \
	lib/md.c \
	lib/map.c \
	lib/pt.c \
	lib/triggered.c \
	lib/init.c \
	lib/ni.c \
	lib/data_movment.c \
	lib/me.c \
	lib/atomic.c \
	lib/handle.c \
	lib/ppe_if.c \
	lib/eq.c \
	lib/le.c \
	lib/id.c \
	lib/ct.c \
	lib/ppe_if_md.h \
	lib/func_call.h \
	lib/ppe_if.h \
	lib/ppe_if_ni.h
libportals_mc_la_CPPFLAGS = $(AM_CPPFLAGS) -I$(top_srcdir)/src/mc/lib/include
libportals_mc_la_LDFLAGS = $(XPMEM_LDFLAGS)
libportals_mc_la_LIBADD = libportals_mc_shared.la $(XPMEM_LIBS)

libportals_mc_shared_la_SOURCES = \
	shared/ptl_command_queue.h \
	shared/ptl_command_queue_entry.h \
	shared/ptl_command_queue_xpmem.c \
	shared/ptl_connection_manager.h \
	shared/ptl_connection_manager.c

