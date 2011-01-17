#! /bin/sh

set -x
aclocal
autoheader
libtoolize
automake
autoconf
