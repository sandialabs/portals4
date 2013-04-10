# -*- Makefile -*-
#
# Copyright (c) 2013 Sandia Corporation
#

AM_CPPFLAGS = -I$(top_srcdir)/src/runtime -I$(top_srcdir)/test
LDADD = $(top_builddir)/test/libtestsupport.la $(top_builddir)/src/libportals.la
AM_LDFLAGS = $(LIBTOOL_WRAPPER_LDFLAGS)

EXTRA_DIST = NetPipe/P4LEwithCT.c
EXTRA_PROGRAMS = 

include msg_rate/Makefile.inc
include rtt_latency/Makefile.inc

NPROCS ?= 2
TESTS_ENVIRONMENT = $(top_builddir)/src/runtime/hydra/yod.hydra -np $(NPROCS)
