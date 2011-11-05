all: $(check_PROGRAMS)

check_PROGRAMS = \
	command_queue

AM_CPPFLAGS = -I$(top_srcdir)/include -I$(top_srcdir)/src/mc
LDADD = $(top_builddir)/src/mc/libportals_shared_mc.la

command_queue_SOURCES = command_queue.c
