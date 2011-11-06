# -*- Makefile -*-
#
# Copyright (c) 2011 Sandia Corporation

AM_CPPFLAGS = -I$(top_srcdir)/include $(XPMEM_CPPFLAGS) -I$(top_srcdir)/src/mc/include

EXTRA_DIST =
SUBDIRS = . test

bin_PROGRAMS = portals_engine
noinst_LTLIBRARIES = libportals_mc.la libportals_mc_shared.la

portals_engine_SOURCES = ppe/ppe.c
portals_engine_LDFLAGS = $(XPMEM_LDFLAGS)
portals_engine_LDADD = libportals_mc_shared.la $(XPMEM_LIBS) -ldl -lpthread

libportals_mc_la_SOURCES =

libportals_mc_la_LDFLAGS = $(XPMEM_LDFLAGS)
libportals_mc_la_LIBADD = libportals_mc_shared.la $(XPMEM_LIBS)

libportals_mc_shared_la_SOURCES = \
	ptl_command_queue.h \
	ptl_command_queue_entry.h \
	ptl_command_queue_xpmem.c \
	ptl_connection_manager.h \
	ptl_connection_manager.c

