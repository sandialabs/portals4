# -*- Makefile -*-
#
# Copyright (c)      2008  Sandia Corporation
#

EXTRA_DIST = autogen.sh

SUBDIRS = doc include src test

ACLOCAL_AMFLAGS = -I config

tags:
	ctags `find include src test -name \*.c -o -name \*.h`

clean-local:
	$(RM) tags

# DISTCHECK_CONFIGURE_FLAGS = $(EXTRA_DISTCHECK_OPTIONS)
