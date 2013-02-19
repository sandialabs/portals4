# -*- Autoconf -*-
#
# Copyright (c)      2013  Sandia Corporation
#

# SANDIA_CHECK_PATH([variable], [action-if-absolute], [action-if-relative])
# ------------------------------------------------------------------------------
AC_DEFUN([SANDIA_CHECK_PATH], [
  
  AS_IF([test "$1" != "no" -a "$1" != "yes" -a "$1" != ""], [
    case $1 in
    [[\\/]* | ?:[\\/]*] )  # Absolute name.
      $2
      ;;
    *) # Relative name.
      $3
      ;;
    esac
  ])
])
