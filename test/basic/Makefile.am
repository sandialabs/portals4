# -*- Makefile -*-
#
# Copyright (c) 2010 Sandia Corporation
#

AM_CXXFLAGS = @RESTRICT_CXXFLAGS@
AM_CPPFLAGS = -I$(top_srcdir)/include -I$(top_srcdir)/test
AM_LDFLAGS = $(LIBTOOL_WRAPPER_LDFLAGS)
LDADD = $(top_builddir)/src/libportals.la $(top_builddir)/test/libtestsupport.la
noinst_HEADERS = testing.h

TESTS = \
	test_pmi_hello \
	test_init \
	test_PA_NIInit \
	test_LA_NIInit \
	test_bootstrap \
	test_PA_LE_put_self \
	test_PA_ME_put_self \
	test_LA_LE_put_self \
	test_LA_ME_put_self \
	test_PA_LE_put \
	test_PA_ME_put \
	test_LA_LE_put \
	test_LA_ME_put \
	test_PA_LE_put_event \
	test_PA_ME_put_event \
	test_LA_LE_put_event \
	test_LA_ME_put_event \
	test_PA_LE_put_send_disable \
	test_PA_ME_put_send_disable \
	test_LA_LE_put_send_disable \
	test_LA_ME_put_send_disable \
	test_PA_LE_put_overflow_ct \
	test_PA_ME_put_overflow_ct \
	test_LA_LE_put_overflow_ct \
	test_LA_ME_put_overflow_ct \
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
	test_LE_flowctl_nohdr \
	test_ME_flowctl_nohdr \
	test_LE_unlink \
	test_ME_unlink \
	test_PA_LE_persistent_search \
	test_PA_ME_persistent_search \
	test_ct_ack \
	test_amo \
	test_amo_barrier

EXTRA_TESTS = \
	test_triggered_ME_ops

if WITH_TRIG_ME_OPS
TESTS += \
	test_triggered_ME_ops
endif

noinst_PROGRAMS = $(TESTS)

NPROCS ?= 2
TESTS_ENVIRONMENT = $(top_builddir)/src/runtime/hydra/yod.hydra -np $(NPROCS)

test_pmi_hello_SOURCES = test_pmi_hello.c
test_pmi_hello_CPPFLAGS = $(AM_CPPFLAGS) -I$(top_srcdir)/src/runtime

test_init_SOURCES = test_init.c

test_PA_NIInit_SOURCES = test_NIInit.c
test_PA_NIInit_CPPFLAGS = $(AM_CPPFLAGS) -DPHYSICAL_ADDR=1

test_LA_NIInit_SOURCES = test_NIInit.c
test_LA_NIInit_CPPFLAGS = $(AM_CPPFLAGS) -DPHYSICAL_ADDR=0

test_bootstrap_SOURCES = test_bootstrap.c

test_PA_LE_put_self_SOURCES = test_put_self.c
test_PA_LE_put_self_CPPFLAGS = $(AM_CPPFLAGS) -DPHYSICAL_ADDR=1 -DMATCHING=0

test_PA_ME_put_self_SOURCES = test_put_self.c
test_PA_ME_put_self_CPPFLAGS = $(AM_CPPFLAGS) -DPHYSICAL_ADDR=1 -DMATCHING=1

test_LA_LE_put_self_SOURCES = test_put_self.c
test_LA_LE_put_self_CPPFLAGS = $(AM_CPPFLAGS) -DPHYSICAL_ADDR=0 -DMATCHING=0

test_LA_ME_put_self_SOURCES = test_put_self.c
test_LA_ME_put_self_CPPFLAGS = $(AM_CPPFLAGS) -DPHYSICAL_ADDR=0 -DMATCHING=1

test_PA_LE_put_SOURCES = test_put.c
test_PA_LE_put_CPPFLAGS = $(AM_CPPFLAGS)  -DPHYSICAL_ADDR=1 -DMATCHING=0

test_PA_ME_put_SOURCES = test_put.c
test_PA_ME_put_CPPFLAGS = $(AM_CPPFLAGS) -DPHYSICAL_ADDR=1 -DMATCHING=1

test_LA_LE_put_SOURCES = test_put.c
test_LA_LE_put_CPPFLAGS = $(AM_CPPFLAGS) -DPHYSICAL_ADDR=0 -DMATCHING=0

test_LA_ME_put_SOURCES = test_put.c
test_LA_ME_put_CPPFLAGS = $(AM_CPPFLAGS) -DPHYSICAL_ADDR=0 -DMATCHING=1

test_PA_LE_put_event_SOURCES = test_put_event.c
test_PA_LE_put_event_CPPFLAGS = $(AM_CPPFLAGS)  -DPHYSICAL_ADDR=1 -DMATCHING=0

test_PA_ME_put_event_SOURCES = test_put_event.c
test_PA_ME_put_event_CPPFLAGS = $(AM_CPPFLAGS) -DPHYSICAL_ADDR=1 -DMATCHING=1

test_LA_LE_put_event_SOURCES = test_put_event.c
test_LA_LE_put_event_CPPFLAGS = $(AM_CPPFLAGS) -DPHYSICAL_ADDR=0 -DMATCHING=0

test_LA_ME_put_event_SOURCES = test_put_event.c
test_LA_ME_put_event_CPPFLAGS = $(AM_CPPFLAGS) -DPHYSICAL_ADDR=0 -DMATCHING=1

test_PA_LE_put_send_disable_SOURCES = test_put_send_disable.c
test_PA_LE_put_send_disable_CPPFLAGS = $(AM_CPPFLAGS)  -DPHYSICAL_ADDR=1 -DMATCHING=0

test_PA_ME_put_send_disable_SOURCES = test_put_send_disable.c
test_PA_ME_put_send_disable_CPPFLAGS = $(AM_CPPFLAGS) -DPHYSICAL_ADDR=1 -DMATCHING=1

test_LA_LE_put_send_disable_SOURCES = test_put_send_disable.c
test_LA_LE_put_send_disable_CPPFLAGS = $(AM_CPPFLAGS) -DPHYSICAL_ADDR=0 -DMATCHING=0

test_LA_ME_put_send_disable_SOURCES = test_put_send_disable.c
test_LA_ME_put_send_disable_CPPFLAGS = $(AM_CPPFLAGS) -DPHYSICAL_ADDR=0 -DMATCHING=1

test_PA_LE_put_overflow_ct_SOURCES = test_put_overflow_ct.c
test_PA_LE_put_overflow_ct_CPPFLAGS = $(AM_CPPFLAGS)  -DPHYSICAL_ADDR=1 -DMATCHING=0

test_PA_ME_put_overflow_ct_SOURCES = test_put_overflow_ct.c
test_PA_ME_put_overflow_ct_CPPFLAGS = $(AM_CPPFLAGS) -DPHYSICAL_ADDR=1 -DMATCHING=1

test_LA_LE_put_overflow_ct_SOURCES = test_put_overflow_ct.c
test_LA_LE_put_overflow_ct_CPPFLAGS = $(AM_CPPFLAGS) -DPHYSICAL_ADDR=0 -DMATCHING=0

test_LA_ME_put_overflow_ct_SOURCES = test_put_overflow_ct.c
test_LA_ME_put_overflow_ct_CPPFLAGS = $(AM_CPPFLAGS) -DPHYSICAL_ADDR=0 -DMATCHING=1

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

test_LE_flowctl_nohdr_SOURCES = test_flowctl_nohdr.c
test_LE_flowctl_nohdr_CPPFLAGS = $(AM_CPPFLAGS) -DINTERFACE=0

test_ME_flowctl_nohdr_SOURCES = test_flowctl_nohdr.c
test_ME_flowctl_nohdr_CPPFLAGS = $(AM_CPPFLAGS) -DINTERFACE=1

test_LE_unlink_SOURCES = test_unlink.c
test_LE_unlink_CPPFLAGS = $(AM_CPPFLAGS) -DMATCHING=0

test_ME_unlink_SOURCES = test_unlink.c
test_ME_unlink_CPPFLAGS = $(AM_CPPFLAGS) -DMATCHING=1

test_PA_LE_persistent_search_SOURCES = test_persistent_search.c
test_PA_LE_persistent_search_CPPFLAGS = $(AM_CPPFLAGS) -DMATCHING=0

test_PA_ME_persistent_search_SOURCES = test_persistent_search.c
test_PA_ME_persistent_search_CPPFLAGS = $(AM_CPPFLAGS) -DMATCHING=1

test_ct_ack_SOURCES = test_ct_ack.c

test_amo_SOURCES = test_amo.c

test_amo_barrier_SOURCES = test_amo_barrier.c
