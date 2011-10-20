# vim:ft=automake

AM_CPPFLAGS = -I$(top_srcdir)/include

noinst_LTLIBRARIES = libtestsupport.la

libtestsupport_la_SOURCES = libP4support.h libP4support.c

if BACKEND_IMPL_SHMEM
AM_CPPFLAGS += -I$(top_srcdir)/src/shmem
libtestsupport_la_SOURCES += shmemrt.c
endif
