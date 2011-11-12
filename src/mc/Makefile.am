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

portals_engine_SOURCES = \
	ppe/atomic.c \
	ppe/ct.c \
	ppe/data_movement.c \
	ppe/eq.c \
	ppe/le.c \
	ppe/md.c \
	ppe/me.c \
	ppe/ni.c \
	ppe/ppe.c \
	ppe/ppe.h \
	ppe/pt.c

portals_engine_LDFLAGS = $(XPMEM_LDFLAGS)
portals_engine_LDADD = libportals_mc_shared.la $(XPMEM_LIBS) -ldl -lpthread

libportals_mc_la_SOURCES = \
	lib/include/ptl_internal_pid.h \
	lib/include/ptl_internal_EQ.h \
	lib/include/ptl_internal_handles.h \
	lib/include/ptl_internal_LE.h \
	lib/include/ptl_internal_nit.h \
	lib/include/ptl_internal_MD.h \
	lib/include/ptl_internal_CT.h \
	lib/include/ptl_internal_error.h \
	lib/include/ptl_internal_PT.h \
	lib/include/ptl_internal_global.h \
	lib/include/ptl_visibility.h \
	lib/atomic.c \
	lib/bundle.c \
	lib/ct.c \
	lib/data_movment.c \
	lib/eq.c \
	lib/handle.c \
	lib/id.c \
	lib/init.c \
	lib/le.c \
	lib/map.c \
	lib/md.c \
	lib/me.c \
	lib/ni.c \
	lib/pt.c \
	lib/startup.c \
	lib/triggered.c
libportals_mc_la_CPPFLAGS = $(AM_CPPFLAGS) -I$(top_srcdir)/src/mc/lib/include
libportals_mc_la_LDFLAGS = $(XPMEM_LDFLAGS)
libportals_mc_la_LIBADD = libportals_mc_shared.la $(XPMEM_LIBS)

libportals_mc_shared_la_SOURCES = \
	shared/ptl_circular_buffer.h \
	shared/ptl_circular_buffer.c \
	shared/ptl_command_queue.h \
	shared/ptl_command_queue_entry.h \
	shared/ptl_command_queue_xpmem.c \
	shared/ptl_connection_manager.h \
	shared/ptl_connection_manager.c \
	shared/ptl_util.h

