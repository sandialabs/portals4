# -*- Makefile -*-
#
# Copyright (c) 2010 Sandia Corporation


DIST_SUBDIRS = shmem ib support

SUBDIRS = support

if BACKEND_IMPL_SHMEM
SUBDIRS += shmem
endif

if BACKEND_IMPL_IB
SUBDIRS += ib
endif
