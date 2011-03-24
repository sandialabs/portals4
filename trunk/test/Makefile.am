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
	test_LE_atomic \
	test_ME_atomic \
	test_LE_fetchatomic \
	test_ME_fetchatomic \
	test_LE_swap \
	test_ME_swap \
	test_event \
	test_LE_put_truncate \
	test_ME_put_truncate \
	test_LE_get_truncate \
	test_ME_get_truncate \
	test_triggered_ctinc \
	test_triggered_ctinc_unordered \
	test_LE_oversize_get \
	test_ME_oversize_get \
	test_LE_oversize_put \
	test_ME_oversize_put \
	test_ME_unexpected_put
BENCHMARKS =

INCLUDES = -I$(top_srcdir)/include
OUTPUTDIR = $(top_builddir)/src
PORTALSLIB = $(OUTPUTDIR)/libportals.la
P4SUPPORTLIB = $(OUTPUTDIR)/libP4support.la
LDADD = $(PORTALSLIB)

include benchmarks/rtt_latency/Makefile.inc
include benchmarks/msg_rate/Makefile.inc

EXTRA_DIST = numawrapper.sh testing.h
EXTRA_PROGRAMS = $(BENCHMARKS)

all: $(TESTS) $(BENCHMARKS)

check_PROGRAMS = $(TESTS)

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

test_LE_atomic_SOURCES = test_atomic.c
test_LE_atomic_CPPFLAGS = -DINTERFACE=0
test_LE_atomic_DEPENDENCIES = $(PORTALSLIB)

test_ME_atomic_SOURCES = test_atomic.c
test_ME_atomic_CPPFLAGS = -DINTERFACE=1
test_ME_atomic_DEPENDENCIES = $(PORTALSLIB)

test_LE_fetchatomic_SOURCES = test_fetchatomic.c
test_LE_fetchatomic_CPPFLAGS = -DINTERFACE=0
test_LE_fetchatomic_DEPENDENCIES = $(PORTALSLIB)

test_ME_fetchatomic_SOURCES = test_fetchatomic.c
test_ME_fetchatomic_CPPFLAGS = -DINTERFACE=1
test_ME_fetchatomic_DEPENDENCIES = $(PORTALSLIB)

test_LE_swap_SOURCES = test_swap.c
test_LE_swap_CPPFLAGS = -DINTERFACE=0
test_LE_swap_DEPENDENCIES = $(PORTALSLIB)

test_ME_swap_SOURCES = test_swap.c
test_ME_swap_CPPFLAGS = -DINTERFACE=1
test_ME_swap_DEPENDENCIES = $(PORTALSLIB)

test_event_SOURCES = test_event.c
test_event_DEPENDENCIES = $(PORTALSLIB)

test_LE_oversize_get_SOURCES = test_oversize_get.c
test_LE_oversize_get_CPPFLAGS = -DINTERFACE=0
test_LE_oversize_get_DEPENDENCIES = $(PORTALSLIB)

test_ME_oversize_get_SOURCES = test_oversize_get.c
test_ME_oversize_get_CPPFLAGS = -DINTERFACE=1
test_ME_oversize_get_DEPENDENCIES = $(PORTALSLIB)

test_LE_oversize_put_SOURCES = test_oversize_put.c
test_LE_oversize_put_CPPFLAGS = -DINTERFACE=0
test_LE_oversize_put_DEPENDENCIES = $(PORTALSLIB)

test_ME_oversize_put_SOURCES = test_oversize_put.c
test_ME_oversize_put_CPPFLAGS = -DINTERFACE=1
test_ME_oversize_put_DEPENDENCIES = $(PORTALSLIB)

test_LE_put_truncate_SOURCES = test_put_truncate.c
test_LE_put_truncate_CPPFLAGS = -DINTERFACE=0
test_LE_put_truncate_DEPENDENCIES = $(PORTALSLIB)

test_ME_put_truncate_SOURCES = test_put_truncate.c
test_ME_put_truncate_CPPFLAGS = -DINTERFACE=1
test_ME_put_truncate_DEPENDENCIES = $(PORTALSLIB)

test_LE_get_truncate_SOURCES = test_get_truncate.c
test_LE_get_truncate_CPPFLAGS = -DINTERFACE=0
test_LE_get_truncate_DEPENDENCIES = $(PORTALSLIB)

test_ME_get_truncate_SOURCES = test_get_truncate.c
test_ME_get_truncate_CPPFLAGS = -DINTERFACE=1
test_ME_get_truncate_DEPENDENCIES = $(PORTALSLIB)

test_triggered_ctinc_SOURCES = test_triggered_ctinc.c
test_triggered_ctinc_CPPFLAGS = -DORDERED
test_triggered_ctinc_DEPENDENCIES = $(PORTALSLIB)

test_triggered_ctinc_unordered_SOURCES = test_triggered_ctinc.c
test_triggered_ctinc_unordered_DEPENDENCIES = $(PORTALSLIB)

test_ME_unexpected_put_SOURCES = test_unexpected_put.c
test_ME_unexpected_put_CPPFLAGS = -DINTERFACE=1
test_ME_unexpected_put_DEPENDENCIES = $(PORTALSLIB)
