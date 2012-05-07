# -*- Autoconf -*-
#
# Copyright (c) 2010 Sandia Corporation
#

AC_DEFUN([_SANDIA_CHECK_IFDEF],
 [AC_PREPROC_IFELSE(
    [AC_LANG_PROGRAM([[]],[[#ifndef $1
#error $1 not defined
#endif]])],
    [$2],[$3])])

# SANDIA_DETECT_COMPILER_TYPE
# These #defs are based on the list at http://predef.sourceforge.net/precomp.html
# ------------------------------------------------------------------
AC_DEFUN([SANDIA_DETECT_COMPILER_TYPE], [
AC_CACHE_CHECK([what kind of C compiler $CC is],
  [sandia_cv_c_compiler_type],
  [AC_LANG_PUSH([C])
   AS_IF([test "x$sandia_cv_c_compiler_type" == x],
     [_SANDIA_CHECK_IFDEF([__TURBO_C__],[sandia_cv_c_compiler_type=Borland])])
   AS_IF([test "x$sandia_cv_c_compiler_type" == x],
     [_SANDIA_CHECK_IFDEF([__DECC],[sandia_cv_c_compiler_type=Compaq])])
   AS_IF([test "x$sandia_cv_c_compiler_type" == x],
     [_SANDIA_CHECK_IFDEF([_CRAYC],[sandia_cv_c_compiler_type=Cray])])
   AS_IF([test "x$sandia_cv_c_compiler_type" == x],
     [_SANDIA_CHECK_IFDEF([__CYGWIN__],[sandia_cv_c_compiler_type=Cygwin])])
   AS_IF([test "x$sandia_cv_c_compiler_type" == x],
     [_SANDIA_CHECK_IFDEF([__DCC__],[sandia_cv_c_compiler_type=Diab])])
   AS_IF([test "x$sandia_cv_c_compiler_type" == x],
     [_SANDIA_CHECK_IFDEF([__DMC__],[sandia_cv_c_compiler_type=DigitalMars])])
   AS_IF([test "x$sandia_cv_c_compiler_type" == x],
     [_SANDIA_CHECK_IFDEF([__SYSC__],[sandia_cv_c_compiler_type=Dignus])])
   AS_IF([test "x$sandia_cv_c_compiler_type" == x],
     [_SANDIA_CHECK_IFDEF([__DJGPP__],[sandia_cv_c_compiler_type=DJGPP])])
   AS_IF([test "x$sandia_cv_c_compiler_type" == x],
     [_SANDIA_CHECK_IFDEF([__PATHCC__],[sandia_cv_c_compiler_type=EKOPath])])
   AS_IF([test "x$sandia_cv_c_compiler_type" == x],
     [_SANDIA_CHECK_IFDEF([__ghs__],[sandia_cv_c_compiler_type=GreenHill])])
   AS_IF([test "x$sandia_cv_c_compiler_type" == x],
     [_SANDIA_CHECK_IFDEF([__HP_cc],[sandia_cv_c_compiler_type=HP])])
   AS_IF([test "x$sandia_cv_c_compiler_type" == x],
     [_SANDIA_CHECK_IFDEF([__IAR_SYSTEMS_ICC__],[sandia_cv_c_compiler_type=IAR])])
   AS_IF([test "x$sandia_cv_c_compiler_type" == x],
     [_SANDIA_CHECK_IFDEF([__xlc__],[sandia_cv_c_compiler_type=IBM_XL])])
   AS_IF([test "x$sandia_cv_c_compiler_type" == x],
     [_SANDIA_CHECK_IFDEF([__IBMC__],[sandia_cv_c_compiler_type=IBM_zOS])])
   AS_IF([test "x$sandia_cv_c_compiler_type" == x],
     [_SANDIA_CHECK_IFDEF([__INTEL_COMPILER],[sandia_cv_c_compiler_type=Intel])])
   AS_IF([test "x$sandia_cv_c_compiler_type" == x],
     [_SANDIA_CHECK_IFDEF([__IMAGECRAFT__],[sandia_cv_c_compiler_type=ImageCraft])])
   AS_IF([test "x$sandia_cv_c_compiler_type" == x],
     [_SANDIA_CHECK_IFDEF([__KEIL__],[sandia_cv_c_compiler_type=KeilCARM])])
   AS_IF([test "x$sandia_cv_c_compiler_type" == x],
     [_SANDIA_CHECK_IFDEF([__C166__],[sandia_cv_c_compiler_type=KeilC166])])
   AS_IF([test "x$sandia_cv_c_compiler_type" == x],
     [_SANDIA_CHECK_IFDEF([__C51__],[sandia_cv_c_compiler_type=KeilC51])])
   AS_IF([test "x$sandia_cv_c_compiler_type" == x],
     [_SANDIA_CHECK_IFDEF([__LCC__],[sandia_cv_c_compiler_type=LCC])])
   AS_IF([test "x$sandia_cv_c_compiler_type" == x],
     [_SANDIA_CHECK_IFDEF([__llvm__],[sandia_cv_c_compiler_type=LLVM])])
   AS_IF([test "x$sandia_cv_c_compiler_type" == x],
     [_SANDIA_CHECK_IFDEF([__HIGHC__],[sandia_cv_c_compiler_type=MetaWare])])
   AS_IF([test "x$sandia_cv_c_compiler_type" == x],
     [_SANDIA_CHECK_IFDEF([__MWERKS__],[sandia_cv_c_compiler_type=MetaWare])])
   AS_IF([test "x$sandia_cv_c_compiler_type" == x],
     [_SANDIA_CHECK_IFDEF([__MINGW32__],[sandia_cv_c_compiler_type=MinGW])])
   AS_IF([test "x$sandia_cv_c_compiler_type" == x],
     [_SANDIA_CHECK_IFDEF([__sgi],[sandia_cv_c_compiler_type=MIPSpro])])
   AS_IF([test "x$sandia_cv_c_compiler_type" == x],
     [_SANDIA_CHECK_IFDEF([__MRC__],[sandia_cv_c_compiler_type=MPW])])
   AS_IF([test "x$sandia_cv_c_compiler_type" == x],
     [_SANDIA_CHECK_IFDEF([_MSC_VER],[sandia_cv_c_compiler_type=MicrosoftVisual])])
   AS_IF([test "x$sandia_cv_c_compiler_type" == x],
     [_SANDIA_CHECK_IFDEF([_MRI],[sandia_cv_c_compiler_type=Microtec])])
   AS_IF([test "x$sandia_cv_c_compiler_type" == x],
     [_SANDIA_CHECK_IFDEF([__CC_NORCROFT],[sandia_cv_c_compiler_type=Norcroft])])
   AS_IF([test "x$sandia_cv_c_compiler_type" == x],
     [_SANDIA_CHECK_IFDEF([__PACIFIC__],[sandia_cv_c_compiler_type=Pacific])])
   AS_IF([test "x$sandia_cv_c_compiler_type" == x],
     [_SANDIA_CHECK_IFDEF([_PACC_VER],[sandia_cv_c_compiler_type=Palm])])
   AS_IF([test "x$sandia_cv_c_compiler_type" == x],
     [_SANDIA_CHECK_IFDEF([__POCC__],[sandia_cv_c_compiler_type=Pelles])])
   AS_IF([test "x$sandia_cv_c_compiler_type" == x],
     [_SANDIA_CHECK_IFDEF([__PGI],[sandia_cv_c_compiler_type=PortlandGroup])])
   AS_IF([test "x$sandia_cv_c_compiler_type" == x],
     [_SANDIA_CHECK_IFDEF([__CC_ARM],[sandia_cv_c_compiler_type=RealView])])
   AS_IF([test "x$sandia_cv_c_compiler_type" == x],
     [_SANDIA_CHECK_IFDEF([__SASC],[sandia_cv_c_compiler_type=SAS])])
   AS_IF([test "x$sandia_cv_c_compiler_type" == x],
     [_SANDIA_CHECK_IFDEF([_SCO_DS],[sandia_cv_c_compiler_type=SCO])])
   AS_IF([test "x$sandia_cv_c_compiler_type" == x],
     [_SANDIA_CHECK_IFDEF([SDCC],[sandia_cv_c_compiler_type=SmallDevice])])
   AS_IF([test "x$sandia_cv_c_compiler_type" == x],
     [_SANDIA_CHECK_IFDEF([__SUNPRO_C],[sandia_cv_c_compiler_type=SunStudio])])
   AS_IF([test "x$sandia_cv_c_compiler_type" == x],
     [_SANDIA_CHECK_IFDEF([__TenDRA__],[sandia_cv_c_compiler_type=TenDRA])])
   AS_IF([test "x$sandia_cv_c_compiler_type" == x],
     [_SANDIA_CHECK_IFDEF([__TILECC__],[sandia_cv_c_compiler_type=TileCC])])
   AS_IF([test "x$sandia_cv_c_compiler_type" == x],
     [_SANDIA_CHECK_IFDEF([__TINYC__],[sandia_cv_c_compiler_type=TinyC])])
   AS_IF([test "x$sandia_cv_c_compiler_type" == x],
     [_SANDIA_CHECK_IFDEF([_UCC],[sandia_cv_c_compiler_type=Ultimate])])
   AS_IF([test "x$sandia_cv_c_compiler_type" == x],
     [_SANDIA_CHECK_IFDEF([__USLC__],[sandia_cv_c_compiler_type=USL])])
   AS_IF([test "x$sandia_cv_c_compiler_type" == x],
     [_SANDIA_CHECK_IFDEF([__GNUC__],[sandia_cv_c_compiler_type=GNU])])

   AS_IF([test "x$sandia_cv_c_compiler_type" == x],
     [sandia_cv_c_compiler_type=unknown])
   AC_LANG_POP([C])
  ])
dnl AC_CACHE_CHECK([what kind of C++ compiler $CXX is],
dnl   [sandia_cv_cxx_compiler_type],
dnl   [AC_LANG_PUSH([C++])
dnl    AS_IF([test "x$sandia_cv_cxx_compiler_type" == x],
dnl      [_SANDIA_CHECK_IFDEF([__BORLANDC__],[sandia_cv_cxx_compiler_type=Borland])])
dnl    AS_IF([test "x$sandia_cv_cxx_compiler_type" == x],
dnl      [_SANDIA_CHECK_IFDEF([__COMO__],[sandia_cv_cxx_compiler_type=Comeau])])
dnl    AS_IF([test "x$sandia_cv_cxx_compiler_type" == x],
dnl      [_SANDIA_CHECK_IFDEF([__DECCXX__],[sandia_cv_cxx_compiler_type=Compaq])])
dnl    AS_IF([test "x$sandia_cv_cxx_compiler_type" == x],
dnl      [_SANDIA_CHECK_IFDEF([__CYGWIN__],[sandia_cv_cxx_compiler_type=Cygwin])])
dnl    AS_IF([test "x$sandia_cv_cxx_compiler_type" == x],
dnl      [_SANDIA_CHECK_IFDEF([__DCC__],[sandia_cv_cxx_compiler_type=Diab])])
dnl    AS_IF([test "x$sandia_cv_cxx_compiler_type" == x],
dnl      [_SANDIA_CHECK_IFDEF([__DMC__],[sandia_cv_cxx_compiler_type=DigitalMars])])
dnl    AS_IF([test "x$sandia_cv_cxx_compiler_type" == x],
dnl      [_SANDIA_CHECK_IFDEF([__SYSC__],[sandia_cv_cxx_compiler_type=Dignus])])
dnl    AS_IF([test "x$sandia_cv_cxx_compiler_type" == x],
dnl      [_SANDIA_CHECK_IFDEF([__DJGPP__],[sandia_cv_cxx_compiler_type=DJGPP])])
dnl    AS_IF([test "x$sandia_cv_cxx_compiler_type" == x],
dnl      [_SANDIA_CHECK_IFDEF([__PATHCC__],[sandia_cv_cxx_compiler_type=EKOPath])])
dnl    AS_IF([test "x$sandia_cv_cxx_compiler_type" == x],
dnl      [_SANDIA_CHECK_IFDEF([__ghs__],[sandia_cv_cxx_compiler_type=GreenHill])])
dnl    AS_IF([test "x$sandia_cv_cxx_compiler_type" == x],
dnl      [_SANDIA_CHECK_IFDEF([__HP_aCC],[sandia_cv_cxx_compiler_type=HP])])
dnl    AS_IF([test "x$sandia_cv_cxx_compiler_type" == x],
dnl      [_SANDIA_CHECK_IFDEF([__IAR_SYSTEMS_ICC__],[sandia_cv_cxx_compiler_type=IAR])])
dnl    AS_IF([test "x$sandia_cv_cxx_compiler_type" == x],
dnl      [_SANDIA_CHECK_IFDEF([__xlC__],[sandia_cv_cxx_compiler_type=IBM_XL])])
dnl    AS_IF([test "x$sandia_cv_cxx_compiler_type" == x],
dnl      [_SANDIA_CHECK_IFDEF([__IBMCPP__],[sandia_cv_cxx_compiler_type=IBM_zOS])])
dnl    AS_IF([test "x$sandia_cv_cxx_compiler_type" == x],
dnl      [_SANDIA_CHECK_IFDEF([__INTEL_COMPILER],[sandia_cv_cxx_compiler_type=Intel])])
dnl    AS_IF([test "x$sandia_cv_cxx_compiler_type" == x],
dnl      [_SANDIA_CHECK_IFDEF([__KCC],[sandia_cv_cxx_compiler_type=KAI])])
dnl    AS_IF([test "x$sandia_cv_cxx_compiler_type" == x],
dnl      [_SANDIA_CHECK_IFDEF([__KEIL__],[sandia_cv_cxx_compiler_type=KeilCARM])])
dnl    AS_IF([test "x$sandia_cv_cxx_compiler_type" == x],
dnl      [_SANDIA_CHECK_IFDEF([__C166__],[sandia_cv_cxx_compiler_type=KeilC166])])
dnl    AS_IF([test "x$sandia_cv_cxx_compiler_type" == x],
dnl      [_SANDIA_CHECK_IFDEF([__C51__],[sandia_cv_cxx_compiler_type=KeilC51])])
dnl    AS_IF([test "x$sandia_cv_cxx_compiler_type" == x],
dnl      [_SANDIA_CHECK_IFDEF([__LCC__],[sandia_cv_cxx_compiler_type=LCC])])
dnl    AS_IF([test "x$sandia_cv_cxx_compiler_type" == x],
dnl      [_SANDIA_CHECK_IFDEF([__llvm__],[sandia_cv_cxx_compiler_type=LLVM])])
dnl    AS_IF([test "x$sandia_cv_cxx_compiler_type" == x],
dnl      [_SANDIA_CHECK_IFDEF([__HIGHC__],[sandia_cv_cxx_compiler_type=MetaWare])])
dnl    AS_IF([test "x$sandia_cv_cxx_compiler_type" == x],
dnl      [_SANDIA_CHECK_IFDEF([__MWERKS__],[sandia_cv_cxx_compiler_type=MetaWare])])
dnl    AS_IF([test "x$sandia_cv_cxx_compiler_type" == x],
dnl      [_SANDIA_CHECK_IFDEF([__MINGW32__],[sandia_cv_cxx_compiler_type=MinGW])])
dnl    AS_IF([test "x$sandia_cv_cxx_compiler_type" == x],
dnl      [_SANDIA_CHECK_IFDEF([__sgi],[sandia_cv_cxx_compiler_type=MIPSpro])])
dnl    AS_IF([test "x$sandia_cv_cxx_compiler_type" == x],
dnl      [_SANDIA_CHECK_IFDEF([__MRC__],[sandia_cv_cxx_compiler_type=MPW])])
dnl    AS_IF([test "x$sandia_cv_cxx_compiler_type" == x],
dnl      [_SANDIA_CHECK_IFDEF([_MSC_VER],[sandia_cv_cxx_compiler_type=MicrosoftVisual])])
dnl    AS_IF([test "x$sandia_cv_cxx_compiler_type" == x],
dnl      [_SANDIA_CHECK_IFDEF([_MRI],[sandia_cv_cxx_compiler_type=Microtec])])
dnl    AS_IF([test "x$sandia_cv_cxx_compiler_type" == x],
dnl      [_SANDIA_CHECK_IFDEF([_PACC_VER],[sandia_cv_cxx_compiler_type=Palm])])
dnl    AS_IF([test "x$sandia_cv_cxx_compiler_type" == x],
dnl      [_SANDIA_CHECK_IFDEF([__PGI],[sandia_cv_cxx_compiler_type=PortlandGroup])])
dnl    AS_IF([test "x$sandia_cv_cxx_compiler_type" == x],
dnl      [_SANDIA_CHECK_IFDEF([_SCO_DS],[sandia_cv_cxx_compiler_type=SCO])])
dnl    AS_IF([test "x$sandia_cv_cxx_compiler_type" == x],
dnl      [_SANDIA_CHECK_IFDEF([__SUNPRO_CC],[sandia_cv_cxx_compiler_type=SunStudio])])
dnl    AS_IF([test "x$sandia_cv_cxx_compiler_type" == x],
dnl      [_SANDIA_CHECK_IFDEF([__TenDRA__],[sandia_cv_cxx_compiler_type=TenDRA])])
dnl    AS_IF([test "x$sandia_cv_cxx_compiler_type" == x],
dnl      [_SANDIA_CHECK_IFDEF([__TILECC__],[sandia_cv_cxx_compiler_type=TileCC])])
dnl    AS_IF([test "x$sandia_cv_cxx_compiler_type" == x],
dnl      [_SANDIA_CHECK_IFDEF([_UCC],[sandia_cv_cxx_compiler_type=Ultimate])])
dnl    AS_IF([test "x$sandia_cv_cxx_compiler_type" == x],
dnl      [_SANDIA_CHECK_IFDEF([__WATCOMC__],[sandia_cv_cxx_compiler_type=Watcom])])
dnl    AS_IF([test "x$sandia_cv_cxx_compiler_type" == x],
dnl      [_SANDIA_CHECK_IFDEF([__GNUC__],[sandia_cv_cxx_compiler_type=GNU])])
dnl 
dnl    AS_IF([test "x$sandia_cv_cxx_compiler_type" == x],
dnl      [_SANDIA_CHECK_IFDEF([__EDG__],[sandia_cv_cxx_compiler_type=EDG_FrontEnd])])
dnl 
dnl    AS_IF([test "x$sandia_cv_cxx_compiler_type" == x],
dnl      [sandia_cv_cxx_compiler_type=unknown])
dnl    AC_LANG_POP([C++])
dnl  ])
])
