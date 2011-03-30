bin_PROGRAMS = ptl_test make_test_atomic make_test_swap

ptl_test_CPPFLAGS = -Wall -ggdb -O0 -I/usr/include/libxml2 -I$(top_srcdir)/include
ptl_test_LDFLAGS = -L$(top_builddir)/src
ptl_test_LDADD = -lpthread -lrdmacm -libverbs -lxml2 $(top_builddir)/src/libportals.la -lmpi
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

make_test_swap_SOURCES = data.c make_test_swap.c
make_test_swap_CPPFLAGS = -Wall -ggdb -O0 -I$(top_srcdir)/include
make_test_atomic_SOURCES = data.c make_test_atomic.c
make_test_atomic_CPPFLAGS = -Wall -ggdb -O0 -I$(top_srcdir)/include
