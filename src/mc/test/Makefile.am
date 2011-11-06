all: $(check_PROGRAMS)

check_PROGRAMS = \
	command_queue \
	connection_manager

AM_CPPFLAGS = -I$(top_srcdir)/include -I$(top_srcdir)/src/mc
LDADD = $(XPMEM_LDFLAGS) $(top_builddir)/src/mc/libportals_mc_shared.la $(XPMEM_LIBS)

command_queue_SOURCES = command_queue.c

connection_manager_SOURCES = connection_manager.c
