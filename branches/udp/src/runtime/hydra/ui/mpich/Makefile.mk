# -*- Mode: Makefile; -*-
#
# (C) 2008 by Argonne National Laboratory.
#     See COPYRIGHT in top-level directory.
#

AM_CPPFLAGS += -I$(top_srcdir)/ui/utils -DHYDRA_CONF_FILE=\"@sysconfdir@/yod.hydra.conf\"

bin_PROGRAMS += yod.hydra

yod_hydra_SOURCES = $(top_srcdir)/ui/mpich/mpiexec.c $(top_srcdir)/ui/mpich/utils.c
yod_hydra_LDFLAGS = $(external_ldflags) -L$(top_builddir)
yod_hydra_LDADD = -lpm -lhydra $(external_libs)
yod_hydra_DEPENDENCIES = libpm.la libhydra.la
