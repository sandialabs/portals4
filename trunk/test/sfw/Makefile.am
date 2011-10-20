CC=$(TEST_CC)

EXTRA_DIST = \
	test_atomic_logical_lb.xml \
	test_atomic_mixed.xml \
	test_atomic_overflow.xml \
	test_atomic_physical_lb.xml \
	test_basic_bundle.xml \
	test_basic_ct.xml \
	test_basic_eq.xml \
	test_basic_handle.xml \
	test_basic_id.xml \
	test_basic_init.xml \
	test_basic_le.xml \
	test_basic_md.xml \
	test_basic_me.xml \
	test_basic_move.xml \
	test_basic_ni.xml \
	test_basic_pt.xml \
	test_basic_rt.xml \
	test_basic_trig.xml \
	test_ct_events.xml \
	test_eq_overflow.xml \
	test_events_logical.xml \
	test_flow.xml \
	test_limits.xml \
	test_loopback_move.xml \
	test_mapping.xml \
	test_phys_client.xml \
	test_phys_server.xml \
	test_put_overflow.xml \
	test_put.xml \
	test_rdma_move_logical_lb.xml \
	test_rdma_move_logical.xml \
	test_rdma_move_physical_lb.xml \
	test_search.xml \
	test_short.xml \
	test_swap_logical_lb.xml \
	test_swap_mixed.xml \
	test_swap_physical_lb.xml \
	test_threads.xml \
	test_unlink.xml

noinst_PROGRAMS = ptl_test make_test_atomic make_test_swap make_test_move test

# Each binary needs the portals library and the runtime
PORTALSLIB = $(top_builddir)/src/libportals.la $(top_builddir)/test/libtestsupport.la
AM_CPPFLAGS = -I$(top_srcdir)/include -I$(top_srcdir)/test -I/usr/include/libxml2

ptl_test_LDFLAGS = -L$(top_builddir)/src
ptl_test_LDADD = -lpthread -lrdmacm -libverbs -lxml2 $(PORTALSLIB) -lmpi
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
test_LDADD = -lpthread -lrdmacm -libverbs $(PORTALSLIB) -lmpi
test_SOURCES = \
	test.c

make_test_move_SOURCES = data.c make_test_move.c
make_test_swap_SOURCES = data.c make_test_swap.c
make_test_atomic_SOURCES = data.c make_test_atomic.c
