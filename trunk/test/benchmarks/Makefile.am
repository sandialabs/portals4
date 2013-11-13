# -*- Makefile -*-
#
# Copyright (c) 2013 Sandia Corporation
#

AM_CPPFLAGS = -I$(top_srcdir)/src/runtime -I$(top_srcdir)/test -I$(top_srcdir)/include
LDADD = $(top_builddir)/test/libtestsupport.la $(top_builddir)/src/libportals.la
AM_LDFLAGS = $(LIBTOOL_WRAPPER_LDFLAGS)

EXTRA_DIST = NetPIPE/P4LEwithCT.c
check_PROGRAMS =    

include msg_rate/Makefile.inc
include rtt_latency/Makefile.inc

NPROCS ?= 2
LOG_COMPILER = $(TEST_RUNNER)

