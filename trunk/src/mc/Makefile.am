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
	ppe/connection.c \
	ppe/data_movement.c \
	ppe/dispatch.h \
	ppe/eq.c \
	ppe/le.c \
	ppe/map.c \
	ppe/md.c \
	ppe/me.c \
	ppe/ni.c \
	ppe/ppe.c \
	ppe/ppe.h \
	ppe/ppe_xpmem.h \
	ppe/ppe_xpmem.c \
	ppe/pt.c \
	ppe/nal_callback.c \
	ppe/nal_init.c \
	ppe/nal/p3.3/include/p3-config.h \
	ppe/nal/p3.3/include/p3nal_utcp.h \
	ppe/nal/p3.3/include/p3/debug.h \
	ppe/nal/p3.3/include/p3/errno.h \
	ppe/nal/p3.3/include/p3/handle.h \
	ppe/nal/p3.3/include/p3/lock.h \
	ppe/nal/p3.3/include/p3/nal_types.h \
	ppe/nal/p3.3/include/p3api/debug.h \
	ppe/nal/p3.3/include/p3api/misc.h \
	ppe/nal/p3.3/include/p3api/types.h \
	ppe/nal/p3.3/include/p3lib/debug.h \
	ppe/nal/p3.3/include/p3lib/nal.h \
	ppe/nal/p3.3/include/p3lib/p3lib.h \
	ppe/nal/p3.3/include/p3lib/p3lib_support.h \
	ppe/nal/p3.3/include/p3lib/p3validate.h \
	ppe/nal/p3.3/include/p3lib/types.h \
	ppe/nal/p3.3/linux/include/linux/list.h \
	ppe/nal/p3.3/linux/include/linux/prefetch.h \
	ppe/nal/p3.3/linux/include/linux/stddef.h \
	ppe/nal/p3.3/nal/tcp/hash_init.h \
	ppe/nal/p3.3/nal/tcp/lib-tcpnal.c \
	ppe/nal/p3.3/nal/tcp/lib-tcpnal.h \
	ppe/nal/p3.3/p3lib/init.c \
	ppe/nal/p3.3/p3lib/nal.c \
	ppe/nal/p3.3/user/p3lib_support.c \
	ppe/nal/p3.3/user/p3utils.c \
	ppe/nal/p3.3/user/p3utils.h 

portals_engine_CPPFLAGS = $(AM_CPPFLAGS) \
    -I$(top_srcdir)/src/mc/ppe/nal/p3.3/include \
    -I$(top_srcdir)/src/mc/ppe/nal/p3.3/linux/include \
    -I$(top_srcdir)/src/mc/ppe/nal/p3.3/nal/tcp \
    -I$(top_srcdir)/src/mc/ppe/nal/p3.3/user

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
	shared/ptl_util.h \
	shared/ptl_double_list.c \
	shared/ptl_double_list.h \
	shared/ptl_free_list.c \
	shared/ptl_free_list.h \
	shared/ptl_stack.c \
	shared/ptl_stack.h

