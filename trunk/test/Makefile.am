# -*- Makefile -*-
#
# Copyright (c) 2010 Sandia Corporation
#

DIST_SUBDIRS=sfw basic benchmarks
SUBDIRS = . basic benchmarks

if HAVE_LIBXML
SUBDIRS+=sfw
endif

noinst_LTLIBRARIES = libtestsupport.la
libtestsupport_la_SOURCES = support.h support.c pmi.c
libtestsupport_la_CPPFLAGS = $(pmi_CPPFLAGS) -I$(top_srcdir)/include
libtestsupport_la_LDFLAGS = $(pmi_LDFLAGS)
libtestsupport_la_LIBADD = \
	$(top_builddir)/src/libportals.la $(pmi_LIBS)

EXTRA_DIST = numawrapper.sh
