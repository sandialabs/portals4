# -*- Makefile -*-
#
# Copyright (c) 2010 Sandia Corporation
#

AM_CPPFLAGS=
AM_CXXFLAGS = @RESTRICT_CXXFLAGS@

DIST_SUBDIRS=sfw
SUBDIRS=. 

if HAVE_LIBXML
SUBDIRS+=sfw
endif

include support/Makefile.am

TESTS = \
	test_pmi_hello \
	test_init \
	test_NIInit \
	test_bootstrap \
	test_LE_put_self \
	test_ME_put_self \
	test_LE_put \
	test_ME_put \
        test_LE_put_phys \
        test_ME_put_phys \
	test_LE_put_multiple \
	test_ME_put_multiple \
	test_LE_put_multiple_large \
	test_ME_put_multiple_large \
	test_LE_put_multiple_overlap \
	test_ME_put_multiple_overlap \
	test_LE_put_multiple_large_overlap \
	test_ME_put_multiple_large_overlap \
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
	test_ME_unexpected_put \
	test_LE_flowctl_noeq \
	test_ME_flowctl_noeq \
	test_LE_flowctl_norecv \
	test_ME_flowctl_norecv \
	test_ct_ack

EXTRA_TESTS = \
	test_triggered_ME_ops

if WITH_TRIG_ME_OPS
TESTS += \
	test_triggered_ME_ops
endif

BENCHMARKS =
check_PROGRAMS =

AM_CPPFLAGS += -I$(top_srcdir)/include
LDADD = libtestsupport.la $(top_builddir)/src/libportals.la

include benchmarks/rtt_latency/Makefile.inc
include benchmarks/msg_rate/Makefile.inc

EXTRA_DIST = numawrapper.sh testing.h
EXTRA_PROGRAMS = $(BENCHMARKS)

all: $(TESTS) $(BENCHMARKS)

check_PROGRAMS += $(TESTS)

if WITH_PPE
 check_PROGRAMS += \
	test_LE_put_phys \
        test_ME_put_phys 
endif

CLEANFILES = $(BENCHMARKS)

THREADS ?= 2
TESTS_ENVIRONMENT = $(top_builddir)/src/runtime/hydra/yod.hydra -np $(THREADS)

test_pmi_hello_SOURCES = test_pmi_hello.c

test_init_SOURCES = test_init.c

test_NIInit_SOURCES = test_NIInit.c

test_bootstrap_SOURCES = test_bootstrap.c

test_LE_put_self_SOURCES = test_put_self.c
test_LE_put_self_CPPFLAGS = $(AM_CPPFLAGS) -DINTERFACE=0

test_ME_put_self_SOURCES = test_put_self.c
test_ME_put_self_CPPFLAGS = $(AM_CPPFLAGS) -DINTERFACE=1

test_LE_put_SOURCES = test_put.c
test_LE_put_CPPFLAGS = $(AM_CPPFLAGS) -DINTERFACE=0 -DLOGICAL=1

test_ME_put_SOURCES = test_put.c
test_ME_put_CPPFLAGS = $(AM_CPPFLAGS) -DINTERFACE=1 -DLOGICAL=1

test_LE_put_phys_SOURCES = test_put.c
test_LE_put_phys_CPPFLAGS = $(AM_CPPFLAGS) -DINTERFACE=0 -DLOGICAL=0

test_ME_put_phys_SOURCES = test_put.c
test_ME_put_phys_CPPFLAGS = $(AM_CPPFLAGS) -DINTERFACE=1 -DLOGICAL=0

test_LE_put_multiple_SOURCES = test_put_multiple.c
test_LE_put_multiple_CPPFLAGS = $(AM_CPPFLAGS) -DINTERFACE=0 -DBUFSIZE=256 -DOVERLAP=0

test_ME_put_multiple_SOURCES = test_put_multiple.c
test_ME_put_multiple_CPPFLAGS = $(AM_CPPFLAGS) -DINTERFACE=1 -DBUFSIZE=256 -DOVERLAP=0

test_LE_put_multiple_large_SOURCES = test_put_multiple.c
test_LE_put_multiple_large_CPPFLAGS = $(AM_CPPFLAGS) -DINTERFACE=0 -DBUFSIZE=4096 -DOVERLAP=0

test_ME_put_multiple_large_SOURCES = test_put_multiple.c
test_ME_put_multiple_large_CPPFLAGS = $(AM_CPPFLAGS) -DINTERFACE=1 -DBUFSIZE=4096 -DOVERLAP=0

test_LE_put_multiple_overlap_SOURCES = test_put_multiple.c
test_LE_put_multiple_overlap_CPPFLAGS = $(AM_CPPFLAGS) -DINTERFACE=0 -DBUFSIZE=256 -DOVERLAP=1

test_ME_put_multiple_overlap_SOURCES = test_put_multiple.c
test_ME_put_multiple_overlap_CPPFLAGS = $(AM_CPPFLAGS) -DINTERFACE=1 -DBUFSIZE=256 -DOVERLAP=1

test_LE_put_multiple_large_overlap_SOURCES = test_put_multiple.c
test_LE_put_multiple_large_overlap_CPPFLAGS = $(AM_CPPFLAGS) -DINTERFACE=0 -DBUFSIZE=4096 -DOVERLAP=1

test_ME_put_multiple_large_overlap_SOURCES = test_put_multiple.c
test_ME_put_multiple_large_overlap_CPPFLAGS = $(AM_CPPFLAGS) -DINTERFACE=1 -DBUFSIZE=4096 -DOVERLAP=1

test_LE_get_SOURCES = test_get.c
test_LE_get_CPPFLAGS = $(AM_CPPFLAGS) -DINTERFACE=0

test_ME_get_SOURCES = test_get.c
test_ME_get_CPPFLAGS = $(AM_CPPFLAGS) -DINTERFACE=1

test_LE_atomic_SOURCES = test_atomic.c
test_LE_atomic_CPPFLAGS = $(AM_CPPFLAGS) -DINTERFACE=0

test_ME_atomic_SOURCES = test_atomic.c
test_ME_atomic_CPPFLAGS = $(AM_CPPFLAGS) -DINTERFACE=1

test_LE_fetchatomic_SOURCES = test_fetchatomic.c
test_LE_fetchatomic_CPPFLAGS = $(AM_CPPFLAGS) -DINTERFACE=0

test_ME_fetchatomic_SOURCES = test_fetchatomic.c
test_ME_fetchatomic_CPPFLAGS = $(AM_CPPFLAGS) -DINTERFACE=1

test_LE_swap_SOURCES = test_swap.c
test_LE_swap_CPPFLAGS = $(AM_CPPFLAGS) -DINTERFACE=0

test_ME_swap_SOURCES = test_swap.c
test_ME_swap_CPPFLAGS = $(AM_CPPFLAGS) -DINTERFACE=1

test_event_SOURCES = test_event.c

test_LE_oversize_get_SOURCES = test_oversize_get.c
test_LE_oversize_get_CPPFLAGS = $(AM_CPPFLAGS) -DINTERFACE=0

test_ME_oversize_get_SOURCES = test_oversize_get.c
test_ME_oversize_get_CPPFLAGS = $(AM_CPPFLAGS) -DINTERFACE=1

test_LE_oversize_put_SOURCES = test_oversize_put.c
test_LE_oversize_put_CPPFLAGS = $(AM_CPPFLAGS) -DINTERFACE=0

test_ME_oversize_put_SOURCES = test_oversize_put.c
test_ME_oversize_put_CPPFLAGS = $(AM_CPPFLAGS) -DINTERFACE=1

test_LE_put_truncate_SOURCES = test_put_truncate.c
test_LE_put_truncate_CPPFLAGS = $(AM_CPPFLAGS) -DINTERFACE=0

test_ME_put_truncate_SOURCES = test_put_truncate.c
test_ME_put_truncate_CPPFLAGS = $(AM_CPPFLAGS) -DINTERFACE=1

test_LE_get_truncate_SOURCES = test_get_truncate.c
test_LE_get_truncate_CPPFLAGS = $(AM_CPPFLAGS) -DINTERFACE=0

test_ME_get_truncate_SOURCES = test_get_truncate.c
test_ME_get_truncate_CPPFLAGS = $(AM_CPPFLAGS) -DINTERFACE=1

test_triggered_LE_put_SOURCES = test_triggered_put.c
test_triggered_LE_put_CPPFLAGS = $(AM_CPPFLAGS) -DINTERFACE=0

test_triggered_ME_put_SOURCES = test_triggered_put.c
test_triggered_ME_put_CPPFLAGS = $(AM_CPPFLAGS) -DINTERFACE=1

test_triggered_ME_ops_SOURCES = test_triggered_me_ops.c
test_triggered_ME_ops_CPPFLAGS = $(AM_CPP_FLAGS) -DINTERFACE=1

test_triggered_LE_get_SOURCES = test_triggered_get.c
test_triggered_LE_get_CPPFLAGS = $(AM_CPPFLAGS) -DINTERFACE=0

test_triggered_ME_get_SOURCES = test_triggered_get.c
test_triggered_ME_get_CPPFLAGS = $(AM_CPPFLAGS) -DINTERFACE=1

test_triggered_ctinc_SOURCES = test_triggered_ctinc.c
test_triggered_ctinc_CPPFLAGS = $(AM_CPPFLAGS) -DORDERED

test_triggered_ctinc_unordered_SOURCES = test_triggered_ctinc.c

test_triggered_ctset_SOURCES = test_triggered_ctset.c
test_triggered_ctset_CPPFLAGS = $(AM_CPPFLAGS) -DORDERED

test_triggered_ctset_unordered_SOURCES = test_triggered_ctset.c

test_ME_unexpected_put_SOURCES = test_unexpected_put.c
test_ME_unexpected_put_CPPFLAGS = $(AM_CPPFLAGS) -DINTERFACE=1

test_LE_flowctl_noeq_SOURCES = test_flowctl_noeq.c
test_LE_flowctl_noeq_CPPFLAGS = $(AM_CPPFLAGS) -DINTERFACE=0

test_ME_flowctl_noeq_SOURCES = test_flowctl_noeq.c
test_ME_flowctl_noeq_CPPFLAGS = $(AM_CPPFLAGS) -DINTERFACE=1

test_LE_flowctl_norecv_SOURCES = test_flowctl_norecv.c
test_LE_flowctl_norecv_CPPFLAGS = $(AM_CPPFLAGS) -DINTERFACE=0

test_ME_flowctl_norecv_SOURCES = test_flowctl_norecv.c
test_ME_flowctl_norecv_CPPFLAGS = $(AM_CPPFLAGS) -DINTERFACE=1

test_ct_ack_SOURCES = test_ct_ack.c
