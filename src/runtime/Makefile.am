# -*- Makefile -*-
#
# Copyright (c) 2012 Sandia Corporation

SUBDIRS = hydra

AM_CPPFLAGS = -I$(top_srcdir)/src/runtime/portals4
lib_LTLIBRARIES = libportals_runtime.la

portals4_HEADERS = portals4/pmi.h
libportals_runtime_la_LIBADD =
libportals_runtime_la_LDFLAGS = -version-info 1:0:0 -no-undefined
libportals_runtime_la_SOURCES = \
	pmi-simple/simple_pmi.c \
	pmi-simple/simple_pmiutil.c \
	pmi-simple/simple_pmiutil.h

portals4dir = $(includedir)/portals4
