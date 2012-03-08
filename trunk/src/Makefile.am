# -*- Makefile -*-
#
# Copyright (c) 2010 Sandia Corporation


DIST_SUBDIRS = ib mc shmem yod
SUBDIRS = 

lib_LTLIBRARIES = libportals.la
libportals_la_SOURCES=
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
libportals_la_LIBADD =

if BACKEND_IMPL_SHMEM
SUBDIRS += shmem
libportals_la_LIBADD += shmem/libportals_shmem.la
endif

if BACKEND_IMPL_IB
SUBDIRS += ib
libportals_la_LIBADD += ib/libportals_ib.la
libportals_la_LDFLAGS += $(LD_VERSION_SCRIPT)
endif

if BACKEND_IMPL_MC
SUBDIRS += mc
libportals_la_LIBADD += mc/libportals_mc.la
endif

if !BACKEND_IMPL_IB
SUBDIRS += . yod
endif
