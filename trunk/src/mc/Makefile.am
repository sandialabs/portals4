# -*- Makefile -*-
#
# Copyright (c) 2011 Sandia Corporation

AM_CPPFLAGS = -I$(top_srcdir)/include $(XPMEM_CPPFLAGS)

bin_PROGRAMS = portals_engine
noinst_LTLIBRARIES = libportals_mc.la

portals_engine_SOURCES = ppe/ppe.c
portals_engine_LDFLAGS = $(XPMEM_LDFLAGS)
portals_engine_LDADD = $(XPMEM_LIBS)

libportals_mc_la_SOURCES = lib/init.c
libportals_mc_la_LDFLAGS = $(XPMEM_LDFLAGS)
libportals_mc_la_LIBADD = $(XPMEM_LIBS)
