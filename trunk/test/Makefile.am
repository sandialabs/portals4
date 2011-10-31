# -*- Makefile -*-
#
# Copyright (c) 2010 Sandia Corporation
#

CC=$(TEST_CC)
AM_CPPFLAGS=
AM_CXXFLAGS = @RESTRICT_CXXFLAGS@

DIST_SUBDIRS=sfw
SUBDIRS=. 

if TEST_RUNTIME_MPI
SUBDIRS+=sfw
endif

include support/Makefile.am

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
	test_triggered_LE_put \
	test_triggered_ME_put \
	test_triggered_LE_get \
	test_triggered_ME_get \
	test_triggered_ctinc \
	test_triggered_ctinc_unordered \
	test_triggered_ctset \
	test_triggered_ctset_unordered \
	test_LE_oversize_get \
	test_ME_oversize_get \
	test_LE_oversize_put \
	test_ME_oversize_put \
	test_ME_unexpected_put

large_atomic_tests = \
	test_LE_oversize_atomic \
	test_ME_oversize_atomic \
	test_LE_oversize_fetchatomic \
	test_ME_oversize_fetchatomic \
	test_LE_oversize_swap \
	test_ME_oversize_swap

if BACKEND_IMPL_SHMEM
if USE_TRANSFER_ENGINE
TESTS += $(large_atomic_tests)
endif
endif

if BACKEND_IMPL_IB
# These tests require atomic limits that cannot be supported by IB
#TESTS += $(large_atomic_tests)
endif

BENCHMARKS =

AM_CPPFLAGS += -I$(top_srcdir)/include
LDADD = libtestsupport.la $(top_builddir)/src/libportals.la
DEPENDENCIES_TEST = libtestsupport.la $(top_builddir)/src/libportals.la

include benchmarks/rtt_latency/Makefile.inc
include benchmarks/msg_rate/Makefile.inc

EXTRA_DIST = numawrapper.sh testing.h
EXTRA_PROGRAMS = $(BENCHMARKS)

all: $(TESTS) $(BENCHMARKS)

check_PROGRAMS = $(TESTS)
CLEANFILES = $(BENCHMARKS)

THREADS ?= 1
TESTS_ENVIRONMENT = $(TEST_INVOKE_CMD)

test_init_SOURCES = test_init.c
test_init_DEPENDENCIES = $(DEPENDENCIES_TEST)

test_NIInit_SOURCES = test_NIInit.c
test_NIInit_DEPENDENCIES = $(DEPENDENCIES_TEST)

test_bootstrap_SOURCES = test_bootstrap.c
test_bootstrap_DEPENDENCIES = $(DEPENDENCIES_TEST)

test_LE_get_SOURCES = test_get.c
test_LE_get_CPPFLAGS = $(AM_CPPFLAGS) -DINTERFACE=0
test_LE_get_DEPENDENCIES = $(DEPENDENCIES_TEST)

test_ME_get_SOURCES = test_get.c
test_ME_get_CPPFLAGS = $(AM_CPPFLAGS) -DINTERFACE=1
test_ME_get_DEPENDENCIES = $(DEPENDENCIES_TEST)

test_LE_atomic_SOURCES = test_atomic.c
test_LE_atomic_CPPFLAGS = $(AM_CPPFLAGS) -DINTERFACE=0
test_LE_atomic_DEPENDENCIES = $(DEPENDENCIES_TEST)

test_ME_atomic_SOURCES = test_atomic.c
test_ME_atomic_CPPFLAGS = $(AM_CPPFLAGS) -DINTERFACE=1
test_ME_atomic_DEPENDENCIES = $(DEPENDENCIES_TEST)

test_LE_fetchatomic_SOURCES = test_fetchatomic.c
test_LE_fetchatomic_CPPFLAGS = $(AM_CPPFLAGS) -DINTERFACE=0
test_LE_fetchatomic_DEPENDENCIES = $(DEPENDENCIES_TEST)

test_ME_fetchatomic_SOURCES = test_fetchatomic.c
test_ME_fetchatomic_CPPFLAGS = $(AM_CPPFLAGS) -DINTERFACE=1
test_ME_fetchatomic_DEPENDENCIES = $(DEPENDENCIES_TEST)

test_LE_swap_SOURCES = test_swap.c
test_LE_swap_CPPFLAGS = $(AM_CPPFLAGS) -DINTERFACE=0
test_LE_swap_DEPENDENCIES = $(DEPENDENCIES_TEST)

test_ME_swap_SOURCES = test_swap.c
test_ME_swap_CPPFLAGS = $(AM_CPPFLAGS) -DINTERFACE=1
test_ME_swap_DEPENDENCIES = $(DEPENDENCIES_TEST)

test_event_SOURCES = test_event.c
test_event_DEPENDENCIES = $(DEPENDENCIES_TEST)

test_LE_oversize_get_SOURCES = test_oversize_get.c
test_LE_oversize_get_CPPFLAGS = $(AM_CPPFLAGS) -DINTERFACE=0
test_LE_oversize_get_DEPENDENCIES = $(DEPENDENCIES_TEST)

test_ME_oversize_get_SOURCES = test_oversize_get.c
test_ME_oversize_get_CPPFLAGS = $(AM_CPPFLAGS) -DINTERFACE=1
test_ME_oversize_get_DEPENDENCIES = $(DEPENDENCIES_TEST)

test_LE_oversize_put_SOURCES = test_oversize_put.c
test_LE_oversize_put_CPPFLAGS = $(AM_CPPFLAGS) -DINTERFACE=0
test_LE_oversize_put_DEPENDENCIES = $(DEPENDENCIES_TEST)

test_ME_oversize_put_SOURCES = test_oversize_put.c
test_ME_oversize_put_CPPFLAGS = $(AM_CPPFLAGS) -DINTERFACE=1
test_ME_oversize_put_DEPENDENCIES = $(DEPENDENCIES_TEST)

test_LE_put_truncate_SOURCES = test_put_truncate.c
test_LE_put_truncate_CPPFLAGS = $(AM_CPPFLAGS) -DINTERFACE=0
test_LE_put_truncate_DEPENDENCIES = $(DEPENDENCIES_TEST)

test_ME_put_truncate_SOURCES = test_put_truncate.c
test_ME_put_truncate_CPPFLAGS = $(AM_CPPFLAGS) -DINTERFACE=1
test_ME_put_truncate_DEPENDENCIES = $(DEPENDENCIES_TEST)

test_LE_get_truncate_SOURCES = test_get_truncate.c
test_LE_get_truncate_CPPFLAGS = $(AM_CPPFLAGS) -DINTERFACE=0
test_LE_get_truncate_DEPENDENCIES = $(DEPENDENCIES_TEST)

test_ME_get_truncate_SOURCES = test_get_truncate.c
test_ME_get_truncate_CPPFLAGS = $(AM_CPPFLAGS) -DINTERFACE=1
test_ME_get_truncate_DEPENDENCIES = $(DEPENDENCIES_TEST)

test_triggered_LE_put_SOURCES = test_triggered_put.c
test_triggered_LE_put_CPPFLAGS = $(AM_CPPFLAGS) -DINTERFACE=0
test_triggered_LE_put_DEPENDENCIES = $(DEPENDENCIES_TEST)

test_triggered_ME_put_SOURCES = test_triggered_put.c
test_triggered_ME_put_CPPFLAGS = $(AM_CPPFLAGS) -DINTERFACE=1
test_triggered_ME_put_DEPENDENCIES = $(DEPENDENCIES_TEST)

test_triggered_LE_get_SOURCES = test_triggered_get.c
test_triggered_LE_get_CPPFLAGS = $(AM_CPPFLAGS) -DINTERFACE=0
test_triggered_LE_get_DEPENDENCIES = $(DEPENDENCIES_TEST)

test_triggered_ME_get_SOURCES = test_triggered_get.c
test_triggered_ME_get_CPPFLAGS = $(AM_CPPFLAGS) -DINTERFACE=1
test_triggered_ME_get_DEPENDENCIES = $(DEPENDENCIES_TEST)

test_triggered_ctinc_SOURCES = test_triggered_ctinc.c
test_triggered_ctinc_CPPFLAGS = $(AM_CPPFLAGS) -DORDERED
test_triggered_ctinc_DEPENDENCIES = $(DEPENDENCIES_TEST)

test_triggered_ctinc_unordered_SOURCES = test_triggered_ctinc.c
test_triggered_ctinc_unordered_DEPENDENCIES = $(DEPENDENCIES_TEST)

test_triggered_ctset_SOURCES = test_triggered_ctset.c
test_triggered_ctset_CPPFLAGS = $(AM_CPPFLAGS) -DORDERED
test_triggered_ctset_DEPENDENCIES = $(DEPENDENCIES_TEST)

test_triggered_ctset_unordered_SOURCES = test_triggered_ctset.c
test_triggered_ctset_unordered_DEPENDENCIES = $(DEPENDENCIES_TEST)

test_ME_unexpected_put_SOURCES = test_unexpected_put.c
test_ME_unexpected_put_CPPFLAGS = $(AM_CPPFLAGS) -DINTERFACE=1
test_ME_unexpected_put_DEPENDENCIES = $(DEPENDENCIES_TEST)

test_LE_oversize_atomic_SOURCES = test_oversize_atomic.c
test_LE_oversize_atomic_CPPFLAGS = $(AM_CPPFLAGS) -DINTERFACE=0
test_LE_oversize_atomic_DEPENDENCIES = $(DEPENDENCIES_TEST)

test_ME_oversize_atomic_SOURCES = test_oversize_atomic.c
test_ME_oversize_atomic_CPPFLAGS = $(AM_CPPFLAGS) -DINTERFACE=1
test_ME_oversize_atomic_DEPENDENCIES = $(DEPENDENCIES_TEST)

test_LE_oversize_fetchatomic_SOURCES = test_oversize_fetchatomic.c
test_LE_oversize_fetchatomic_CPPFLAGS = $(AM_CPPFLAGS) -DINTERFACE=0
test_LE_oversize_fetchatomic_DEPENDENCIES = $(DEPENDENCIES_TEST)

test_ME_oversize_fetchatomic_SOURCES = test_oversize_fetchatomic.c
test_ME_oversize_fetchatomic_CPPFLAGS = $(AM_CPPFLAGS) -DINTERFACE=1
test_ME_oversize_fetchatomic_DEPENDENCIES = $(DEPENDENCIES_TEST)

test_LE_oversize_swap_SOURCES = test_oversize_swap.c
test_LE_oversize_swap_CPPFLAGS = $(AM_CPPFLAGS) -DINTERFACE=0
test_LE_oversize_swap_DEPENDENCIES = $(DEPENDENCIES_TEST)

test_ME_oversize_swap_SOURCES = test_oversize_swap.c
test_ME_oversize_swap_CPPFLAGS = $(AM_CPPFLAGS) -DINTERFACE=1
test_ME_oversize_swap_DEPENDENCIES = $(DEPENDENCIES_TEST)
