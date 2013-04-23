# -*- Makefile -*-
#
# Copyright (c)      2008  Sandia Corporation
#

EXTRA_DIST = autogen.sh README NEWS TODO\
	patches/rdma-bw-xrc-v1.diff \
	patches/rdmacm-xrc-v3.diff \
	portals4-ib.spec .indent.pro

DIST_SUBDIRS = doc include src test
SUBDIRS = doc include src

if !HAVE_KITTEN
SUBDIRS +=  test
endif

ACLOCAL_AMFLAGS = -I config
