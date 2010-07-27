# -*- Makefile -*-
#
# Copyright (c) 2010 Sandia Corporation
#

AM_CXXFLAGS = @RESTRICT_CXXFLAGS@

TESTS = test_init \
	test_NIInit \
	test_bootstrap

#EXTRA_PROGRAMS = test_bootstrap

buildall: $(TESTS)
.PHONY: buildall

check_PROGRAMS = $(TESTS)

INCLUDES = -I$(top_srcdir)/include
OUTPUTDIR = $(top_builddir)/src
PORTALSLIB = $(OUTPUTDIR)/libportals.la
LDADD = $(PORTALSLIB)

THREADS ?= 1

TESTS_ENVIRONMENT = $(top_builddir)/src/yod -c $(THREADS)

test_init_SOURCES = test_init.c
test_init_DEPENDENCIES = $(PORTALSLIB)

test_NIInit_SOURCES = test_NIInit.c
test_NIInit_DEPENDENCIES = $(PORTALSLIB)

test_bootstrap_SOURCES = test_bootstrap.c
test_bootstrap_DEPENDENCIES = $(PORTALSLIB)
