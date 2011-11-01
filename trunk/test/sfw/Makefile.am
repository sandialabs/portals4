CC=$(TEST_CC)

XMLTESTS1 = \
	test_basic_init.xml \
	test_basic_ni.xml \
	test_basic_handle.xml \
	test_basic_pt.xml \
	test_basic_eq.xml \
	test_basic_ct.xml \
	test_basic_id.xml \
	test_basic_md.xml \
	test_basic_le.xml \
	test_basic_me.xml \
	test_basic_move.xml \
	test_basic_bundle.xml \
	test_basic_trig.xml \
	test_limits.xml \
	test_ct_events.xml \
	test_unlink.xml \
	test_short.xml \
	test_atomic_logical_lb.xml \
	test_swap_logical_lb.xml


XMLTESTS2 = \
	test_rdma_move_logical.xml \
	test_events_logical.xml \
	test_flow.xml \
	test_eq_overflow.xml \
	test_put_overflow.xml \
	test_atomic_overflow.xml \
	test_search.xml

XMLEXTRA = \
	test_atomic_mixed.xml \
	test_atomic_physical_lb.xml \
	test_mapping.xml \
	test_phys_client.xml \
	test_phys_server.xml \
	test_put.xml \
	test_rdma_move_logical_lb.xml \
	test_rdma_move_physical_lb.xml \
	test_swap_mixed.xml \
	test_swap_physical_lb.xml \
	test_threads.xml

EXTRA_DIST = $(XMLTESTS1) $(XMLTESTS2) $(XMLEXTRA)

noinst_PROGRAMS = ptl_test make_test_atomic make_test_swap make_test_move test

# Each binary needs the portals library and the runtime
PORTALSLIB = $(top_builddir)/src/libportals.la $(top_builddir)/test/libtestsupport.la
AM_CPPFLAGS = -I$(top_srcdir)/include -I$(top_srcdir)/test -I/usr/include/libxml2

ptl_test_LDFLAGS = -L$(top_builddir)/src
ptl_test_LDADD = -lpthread -lrdmacm -libverbs -lxml2 $(PORTALSLIB)
ptl_test_SOURCES = \
	api.c \
	api.h \
	cio.c \
	cio.h \
	data.h \
	dict.c \
	dict.h \
	enum.c \
	main.c \
	mask.c \
	ptl_test.h \
	rt.c \
	run.c

test_LDFLAGS = -L$(top_builddir)/src
test_LDADD = -lpthread -lrdmacm -libverbs $(PORTALSLIB)
test_SOURCES = \
	test.c

make_test_move_SOURCES = data.c make_test_move.c
make_test_swap_SOURCES = data.c make_test_swap.c
make_test_atomic_SOURCES = data.c make_test_atomic.c

runtests:
if TEST_RUNTIME_MPI
	@for TEST in $(XMLTESTS1); do \
		echo testing $$TEST ; \
		mpirun  -H localhost -n 1 ./ptl_test -f $(top_srcdir)/test/sfw/$$TEST || exit 1; \
	 done
	@for TEST in $(XMLTESTS2); do \
		echo testing $$TEST ; \
		mpirun  -H localhost -n 2 ./ptl_test -f $(top_srcdir)/test/sfw/$$TEST || exit 1; \
	 done
endif
if TEST_RUNTIME_SHMEMRT
	@for TEST in $(XMLTESTS1); do \
		echo testing $$TEST ; \
		$(top_builddir)/src/yod/yod -c 1 -- ./ptl_test -f $(top_srcdir)/test/sfw/$$TEST ; \
	 done
	@for TEST in $(XMLTESTS2); do \
		echo testing $$TEST ; \
		$(top_builddir)/src/yod/yod -c 2 -- ./ptl_test -f $(top_srcdir)/test/sfw/$$TEST ; \
	 done
endif
