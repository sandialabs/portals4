# -*- Makefile -*-
#
# Copyright (c) 2010 Sandia Corporation
#

AM_CXXFLAGS = @RESTRICT_CXXFLAGS@

TESTS = test_init

buildall: $(TESTS)
.PHONY: buildall

check_PROGRAMS = $(TESTS)

INCLUDES = -I$(top_srcdir)/include
OUTPUTDIR = $(top_builddir)/src
PORTALSLIB = $(OUTPUTDIR)/libportals.la
LDADD = $(PORTALSLIB)

TESTS_ENVIRONMENT = $(top_builddir)/src/yod -c 1

test_init_SOURCES = test_init.c
test_init_DEPENDENCIES = $(PORTALSLIB)
