# -*- Makefile -*-
#
# Copyright (c) 2010 Sandia Corporation
#

AM_CXXFLAGS = @RESTRICT_CXXFLAGS@

TESTS = test_init \
	test_NIInit \
	test_bootstrap \
	test_LE_get \
	test_ME_get \
	test_atomic \
	test_fetchatomic \
	test_swap \
	test_LE_hotpotato \
	test_ME_hotpotato \
	test_event

#EXTRA_PROGRAMS = test_bootstrap

all: $(TESTS)
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

test_LE_get_SOURCES = test_get.c
test_LE_get_CPPFLAGS = -DINTERFACE=0
test_LE_get_DEPENDENCIES = $(PORTALSLIB)

test_ME_get_SOURCES = test_get.c
test_ME_get_CPPFLAGS = -DINTERFACE=1
test_ME_get_DEPENDENCIES = $(PORTALSLIB)

test_atomic_SOURCES = test_atomic.c
test_atomic_DEPENDENCIES = $(PORTALSLIB)

test_fetchatomic_SOURCES = test_fetchatomic.c
test_fetchatomic_DEPENDENCIES = $(PORTALSLIB)

test_swap_SOURCES = test_swap.c
test_swap_DEPENDENCIES = $(PORTALSLIB)

test_LE_hotpotato_SOURCES = test_LE_hotpotato.c
test_LE_hotpotato_DEPENDENCIES = $(PORTALSLIB)

test_ME_hotpotato_SOURCES = test_ME_hotpotato.c
test_ME_hotpotato_DEPENDENCIES = $(PORTALSLIB)

test_event_SOURCES = test_event.c
test_event_DEPENDENCIES = $(PORTALSLIB)
