# vim:ft=automake

AM_CPPFLAGS += -I$(top_srcdir)/src/runtime

noinst_LTLIBRARIES = libtestsupport.la
libtestsupport_la_SOURCES = support.h support/support.c support/pmi.c
libtestsupport_la_LIBADD = \
	$(top_builddir)/src/libportals.la \
	$(top_builddir)/src/runtime/libportals_runtime.la
