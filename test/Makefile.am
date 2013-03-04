# -*- Makefile -*-
#
# Copyright (c) 2010 Sandia Corporation
#

DIST_SUBDIRS=sfw basic benchmarks
SUBDIRS=. basic benchmarks

if HAVE_LIBXML
SUBDIRS+=sfw
endif

AM_CPPFLAGS = -I$(top_srcdir)/src/runtime

noinst_LTLIBRARIES = libtestsupport.la
libtestsupport_la_SOURCES = support.h support.c pmi.c
libtestsupport_la_LIBADD = \
	$(top_builddir)/src/libportals.la \
	$(top_builddir)/src/runtime/libportals_runtime.la

EXTRA_DIST = numawrapper.sh
