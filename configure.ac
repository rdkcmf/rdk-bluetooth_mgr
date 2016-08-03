#                                               -*- Autoconf -*-
# Process this file with autoconf to produce a configure script.

AC_PREREQ([2.69])
AC_INIT([btr-mgr], [1.0], [Chandresh_Pitty@cable.comcast.com])
AC_CONFIG_MACRO_DIR([cfg])
AC_CONFIG_AUX_DIR([cfg])
AM_INIT_AUTOMAKE([subdir-objects -Wall -Werror foreign no-dist-gzip dist-bzip2 1.9])

m4_ifdef([AM_PROG_AR], [AM_PROG_AR])

LT_INIT
AC_LANG([C])

m4_ifdef([AM_SILENT_RULES],[AM_SILENT_RULES([yes])],
[AM_DEFAULT_VERBOSITY=1
AC_SUBST(AM_DEFAULT_VERBOSITY)])


# Checks for programs.
AC_PROG_CXX
AC_PROG_INSTALL
AC_PROG_MAKE_SET
AC_PROG_CC

# Checks for libraries.
AC_CHECK_LIB(btrCore, BTRCore_Init,
                   btrCore_ok=yes, btrCore_ok=no
                   AC_MSG_ERROR([BTRCORE library (libbtrCore) not found]))
if test "$btrCore_ok" = yes; then
   AC_MSG_CHECKING([for btrCore])
   AC_TRY_CPP(
[#include <stdio.h>
#undef PACKAGE
#undef VERSION
#undef HAVE_STDLIB_H
#include "btrCoreTypes.h"
#include "btrCore.h"],
         btrCore_ok=yes,
         btrCore_ok=no)
   AC_MSG_RESULT($btrCore_ok)
   if test "$btrCore_ok" = yes; then
      BTRCORE_LIBS="-lbtrCore"
   else
      AC_MSG_ERROR([BTRCORE library (libbtrCore) not found])
   fi
fi
AC_SUBST([BTRCORE_LIBS])

AC_CHECK_LIB(rmfAudioCapture, RMF_AudioCapture_Open,
                   rmfAudioCapture_ok=yes, rmfAudioCapture_ok=no
                   AC_MSG_ERROR([RMFAUDIOCAPTURE library (librmfAudioCapture) not found]))
if test "$rmfAudioCapture_ok" = yes; then
   AC_MSG_CHECKING([for rmfAudioCapture])
   AC_TRY_COMPILE(
[#include <stdio.h>
#undef PACKAGE
#undef VERSION
#undef HAVE_STDLIB_H
#include "rmfAudioCapture.h"],,
         rmfAudioCapture_ok=yes,
         rmfAudioCapture_ok=no)
   AC_MSG_RESULT($rmfAudioCapture_ok)
   if test "$rmfAudioCapture_ok" = yes; then
      RMFAUDIOCAPTURE_LIBS="-lrmfAudioCapture"
   else
      AC_MSG_ERROR([RMFAUDIOCAPTURE library (librmfAudioCapture) not found])
   fi
fi
AC_SUBST([RMFAUDIOCAPTURE_LIBS])

# Checks for header files.
AC_CHECK_HEADERS([stdlib.h unistd.h btrCoreTypes.h btrCore.h btrCore_avMedia.h])

# Use Glib
PKG_CHECK_MODULES([GLIB], [glib-2.0 >= 2.24.0])

# GStreamer version. Use 1.0 Only
AC_ARG_ENABLE(gstreamer1,
              AS_HELP_STRING([--enable-gstreamer1],
                             [Use GStreamer 1.x]), ,
              [enable_gstreamer1=auto])

AS_IF([test "x$enable_gstreamer1" = "xyes"], [
  GST_MAJORMINOR=1.0
  PKG_CHECK_MODULES([GST], [gstreamer-1.0 >= 1.4], have_gst1="yes", have_gst1="no")

  AS_IF([test "x$have_gst1" = "xyes"], [
    PKG_CHECK_MODULES([GSTBASE], [gstreamer-base-1.0 >= 1.4])
    AC_DEFINE(USE_GST1, 1, [Build with GStreamer 1.x])
  ], [])
], [])

AS_IF([test "x$enable_gstreamer1" = "xyes" -a "x$have_gst1" != "xyes"], [
   AC_MSG_ERROR([Could not find GStreamer 1.x dependencies:
$GST_PKG_ERRORS])], [])

AM_CONDITIONAL(USE_GST1, test "x$enable_gstreamer1" = "xyes" -a "x$have_gst1" = "xyes")

RPC_ENABLE="" #true"
RPC_FLAGS="" #-DBTMGR_ENABLE_IARM_INTERFACE"
AC_ARG_ENABLE([rpc],
              AS_HELP_STRING([--enable-rpc],[enable rpc plugin (default is no)]),
              [
      		case "${enableval}" in
        	 yes) RPC_FLAGS="-DBTMGR_ENABLE_IARM_INTERFACE" RPC_ENABLE=true;;
        	 no)  RPC_ENABLE=false AC_MSG_ERROR([rpc plugin is disabled]) ;;
        	 *) AC_MSG_ERROR([bad value ${enableval} for --enable-rpc]) ;;
       		esac
    	      ],
    	      [echo "rpc plugin is disabled"])

plugindir="\$(libdir)/gstreamer-$GST_MAJORMINOR"

BTR_MGR_SRC="src"
BTR_MGR_TEST="tests"

AM_CONDITIONAL(USE_RPC, test "x$RPC_ENABLE" = xtrue)

# Checks for typedefs, structures, and compiler characteristics.
AC_TYPE_PID_T
AC_TYPE_SIZE_T

AC_SUBST(plugindir)
AC_SUBST(GST_MAJORMINOR)
AC_SUBST(BTR_MGR_SRC)
AC_SUBST(BTR_MGR_TEST)
AC_SUBST(RPC_FLAGS)

# Checks for library functions.

AC_CONFIG_FILES([Makefile
                 src/Makefile
                 src/main/Makefile
                 src/rpc/Makefile
                 src/ctrl/Makefile
                 src/streamOut/Makefile
                 src/streamIn/Makefile
                 tests/Makefile
                 tests/streamOut/Makefile])

AC_OUTPUT
