# -*- Makefile -*-
#
# Copyright (c) 2012 Sandia Corporation


DIST_SUBDIRS = ib runtime
SUBDIRS = ib

if WANT_RUNTIME
SUBDIRS += runtime
endif

lib_LTLIBRARIES = libportals.la
libportals_la_SOURCES=
libportals_la_LIBADD = ib/libportals_ib.la
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
libportals_la_LDFLAGS = -version-info 4:0:0 -no-undefined $(LD_VERSION_SCRIPT)
