CC=$(TEST_CC)

noinst_PROGRAMS = ptl_test make_test_atomic make_test_swap make_test_move test
noinst_LTLIBRARIES = libportals_rt.la

# Each binary needs the portals library and the runtime
PORTALSLIB = $(top_builddir)/src/libportals.la libportals_rt.la
AM_CPPFLAGS = -I$(top_srcdir)/include

# Runtime library used by the tests
libportals_rt_la_CPPFLAGS = -I$(top_srcdir)/include
libportals_rt_la_LIBADD = -lmpi
libportals_rt_la_SOURCES = ptl_rt.c

ptl_test_CPPFLAGS = -Wall -ggdb -O0 -I/usr/include/libxml2
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

test_CPPFLAGS = -Wall -ggdb -O0 -I$(top_srcdir)/include
test_LDFLAGS = -L$(top_builddir)/src
test_LDADD = -lpthread -lrdmacm -libverbs $(PORTALSLIB) -lmpi
test_SOURCES = \
	test.c

make_test_move_SOURCES = data.c make_test_move.c
make_test_move_CPPFLAGS = -Wall -ggdb -O0 -I$(top_srcdir)/include
make_test_swap_SOURCES = data.c make_test_swap.c
make_test_swap_CPPFLAGS = -Wall -ggdb -O0 -I$(top_srcdir)/include
make_test_atomic_SOURCES = data.c make_test_atomic.c
make_test_atomic_CPPFLAGS = -Wall -ggdb -O0 -I$(top_srcdir)/include
