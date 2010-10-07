# -*- Makefile -*-
#
# Copyright (c) 2010 Sandia Corporation


AM_CPPFLAGS = -I$(top_srcdir)/include

lib_LTLIBRARIES = libportals.la
bin_PROGRAMS = yod

# version-info fields are:
# 1. the current interface revision number (i.e. whenever arguments of existing
#    functions change, increment this number)
# 2. the revision number (the library with the highest revision number will be
#    preferred)
# 3. the age (how many previous interfaces are supported)
#
# So...
# - If you add functions, increase the interface number AND the age, and reset
#   the revision number to 0.
# - If you modify (or remove) existing functions, increase the interface number
#   and reset the revision and age numbers to 0
# - If you support all the same functions (just bugfixes or whatnot), increment
#   the revision number
libportals_la_LDFLAGS = -version-info 4:0:0 -no-undefined
libportals_la_SOURCES =

yod_SOURCES =

noinst_HEADERS =

if BACKEND_IMPL_SHMEM
include shmem/Makefile.inc
endif
