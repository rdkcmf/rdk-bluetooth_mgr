##########################################################################
# If not stated otherwise in this file or this component's Licenses.txt
# file the following copyright and licenses apply:
#
# Copyright 2016 RDK Management
#
# Licensed under the Apache License, Version 2.0 (the "License");
# you may not use this file except in compliance with the License.
# You may obtain a copy of the License at
#
# http://www.apache.org/licenses/LICENSE-2.0
#
# Unless required by applicable law or agreed to in writing, software
# distributed under the License is distributed on an "AS IS" BASIS,
# WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
# See the License for the specific language governing permissions and
# limitations under the License.
##########################################################################
#                                               -*- Autoconf -*-
# Process this file with autoconf to produce a configure script.

AC_PREREQ([2.69])
AC_INIT([btr-mgr], [1.0], [Chandresh_Pitty@cable.comcast.com])
AC_CONFIG_MACRO_DIR([m4])
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


AC_ARG_ENABLE(ac_rmf,
              AS_HELP_STRING([--enable-ac_rmf],
                             [Use RMF Audio Capture (default is no)]),
                             [enable_ac_rmf=$enableval],
                             [enable_ac_rmf=no])

AS_IF([test "x$enable_ac_rmf" = "xyes"], [
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
          AUDIOCAPTURE_LIBS="-lrmfAudioCapture"
       else
          AC_MSG_ERROR([AUDIOCAPTURE library (librmfAudioCapture) not found])
       fi
    fi
], [])


AC_ARG_ENABLE(acm,
              AS_HELP_STRING([--enable-acm],
                             [Use Audio Capture Manager (default is no)]),
                             [enable_acm=$enableval],
                             [enable_acm=no])

AS_IF([test "x$enable_acm" = "xyes"], [
       AC_MSG_CHECKING([for audioCaptureMgr])
       AC_TRY_COMPILE(
    [#include <stdio.h>
    #include <stdbool.h>
    #undef PACKAGE
    #undef VERSION
    #undef HAVE_STDLIB_H
    #include "basic_types.h"
    #include "audiocapturemgr_iarm.h"],,
             audioCaptureMgr_ok=yes,
             audioCaptureMgr_ok=no)
       AC_MSG_RESULT($audioCaptureMgr_ok)
       if test "$audioCaptureMgr_ok" = yes; then
          AUDIOCAPTURE_LIBS+="  "
       else
          AC_MSG_ERROR([AUDIOCAPTURE Interface (audiocapturemgr_iarm.h) not found])
       fi
], [])


AM_CONDITIONAL(USE_RMF_AC, test "x$enable_ac_rmf" = xyes)
AM_CONDITIONAL(USE_ACM, test "x$enable_acm" = xyes)
AC_SUBST([AUDIOCAPTURE_LIBS])


# Checks for header files.
AC_CHECK_HEADERS([stdlib.h unistd.h btrCoreTypes.h btrCore.h btrCore_service.h])


SYSTEMD_SDNOTIFY_LDFLAGS=""
SYSTEMD_SDNOTIFY_CFLAGS=""
AC_ARG_ENABLE([systemd-notify],
        AS_HELP_STRING([--enable-systemd-notify],[enable systemd sd notify]),
        [
                case "${enableval}" in
                yes) SYSTEMD_SDNOTIFY_CFLAGS="-DENABLE_SD_NOTIFY"
                     SYSTEMD_SDNOTIFY_LDFLAGS="-lsystemd" ;;
                no) AC_MSG_ERROR([systemd sd notify  is disabled]) ;;
                *) AC_MSG_ERROR([bad value ${enableval} for --enable-systemd ]) ;;
                esac
                ],
        [echo "systemd sd notify  is disable"])


# Use Glib
PKG_CHECK_MODULES([GLIB], [glib-2.0 >= 2.24.0])
PKG_CHECK_MODULES([GTHREAD], [gthread-2.0 >= 2.24.0])
PKG_CHECK_MODULES([LIBCJSON], [libcjson >= 1.0])

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
  ], [])
], [])

AM_CONDITIONAL(USE_GSTREAMER1, test "x$enable_gstreamer1" = "xyes" -a "x$have_gst1" = "xyes")


AS_IF([test "x$enable_gstreamer1" = "xyes" -a "x$have_gst1" != "xyes"], [
   AC_MSG_ERROR([Could not find GStreamer 1.x dependencies:
$GST_PKG_ERRORS])], [])


AC_ARG_ENABLE([rpc],
              AS_HELP_STRING([--enable-rpc],[enable rpc plugin (default is no)]),
              [
      		case "${enableval}" in
        	 yes) remotepc=true ;;
        	 no)  remotepc=false AC_MSG_ERROR([rpc plugin is disabled]) ;;
        	 *) AC_MSG_ERROR([bad value ${enableval} for --enable-rpc]) ;;
       		esac
    	      ],
    	      [remotepc=false])
AM_CONDITIONAL([USE_RPC], [test "x$remotepc" = xtrue])

SYS_DIAG_ENABLE="" #true"
AC_ARG_ENABLE([sys-diag],
              AS_HELP_STRING([--enable-sys-diag],[enable system diagnostics (default is no)]),
              [
      		case "${enableval}" in
        	 yes) SYS_DIAG_ENABLE=true ;;
        	 no)  SYS_DIAG_ENABLE=false AC_MSG_ERROR([system diagnostics is disabled]) ;;
        	 *) AC_MSG_ERROR([bad value ${enableval} for --enable-sys-diag]) ;;
       		esac
    	      ],
    	      [echo "system diagnostics is disabled default"])
AM_CONDITIONAL(USE_SYS_DIAG, test "x$SYS_DIAG_ENABLE" = xtrue)

# Check for breakpad
BREAKPAD_CFLAGS=" "
BREAKPAD_LFLAGS=" "
AC_ARG_ENABLE([breakpad],
		AS_HELP_STRING([--enable-breakpad],[enable breakpad support (default is no)]),
		[
			case "${enableval}" in
        	 yes) BREAKPAD_CFLAGS="-DINCLUDE_BREAKPAD"
                    BREAKPAD_LFLAGS="-lbreakpadwrapper";;
        	 no) AC_MSG_ERROR([breakpad is disabled]) ;;
        	 *) AC_MSG_ERROR([bad value ${enableval} for --enable-breakpad]) ;;
        	 esac
        	 ],
    	    [echo "breakpad is disabled"])

# Check for rdk logger 
AC_ARG_ENABLE([rdk-logger],
              AS_HELP_STRING([--enable-rdk-logger],[enable rdk logger (default is no)]),
              [
      		case "${enableval}" in
        	 yes) rdk_logger_ok=yes;;
        	 no)  rdk_logger_ok=no;;
        	 *) AC_MSG_ERROR([bad value ${enableval} for --enable-rdk-logger]) ;;
       		esac
    	      ],
    	      [echo "rdk logger is disabled"])
AS_IF([test "x$rdk_logger_ok" = "xyes"], [
    AC_CHECK_LIB(rdkloggers, rdk_logger_init,
                       enable_rdk_logger=yes, enable_rdk_logger=no
                       AC_MSG_ERROR([RDK_LOGGER library (librdkloggers) not found]))
    if test "$enable_rdk_logger" = yes; then
       AC_MSG_CHECKING([for rdkloggers])
       AC_TRY_COMPILE(
    [#include <stdio.h>
    #undef PACKAGE
    #undef VERSION
    #undef HAVE_STDLIB_H
    #include "rdk_debug.h"],,
             enable_rdk_logger=yes,
             enable_rdk_logger=no)
       AC_MSG_RESULT($enable_rdk_logger)
       if test "$enable_rdk_logger" = yes; then
          AC_DEFINE(RDK_LOGGER_ENABLED, 1, [Enabled RDK Logger])
	      RDKLOGGER_LIBS="-lrdkloggers"
       else
          AC_MSG_ERROR([RDK_LOGGER library (librdkloggers) not found])
       fi
    fi
], [])
AC_SUBST([RDKLOGGER_LIBS])

# Check for pi build
AC_ARG_ENABLE([pi-build],
               AS_HELP_STRING([--enable-pi-build],[enable raspberrypi build (default is no)]),
               [
                 case "${enableval}" in
                  yes) pi_build_ok=yes
                    PI_BUILD_FLAG="-DBUILD_FOR_PI" ;;
                  no)  pi_build_ok=no;;
                  *) AC_MSG_ERROR([bad value ${enableval} for --enable-pi-build]) ;;
                 esac
               ],
               [echo "Building for comcast devices"])
AM_CONDITIONAL([WITH_PI_BUILD], [test x$pi_build_ok = xyes])
AC_SUBST(PI_BUILD_FLAG)

# Check for mtk build
AC_ARG_ENABLE([brcm-build],
               AS_HELP_STRING([--enable-brcm-build],[enable broadcom build (default is no)]),
               [
                 case "${enableval}" in
                  yes) brcm_build_ok=yes
                    BRCM_BUILD_FLAG="-DBUILD_FOR_BRCM" ;;
                  no)  brcm_build_ok=no;;
                  *) AC_MSG_ERROR([bad value ${enableval} for --enable-brcm-build]) ;;
                 esac
               ],
               [echo "Building for broadcom BRCM devices"])
AM_CONDITIONAL([WITH_BRCM_BUILD], [test x$brcm_build_ok = xyes])
AC_SUBST(BRCM_BUILD_FLAG)

# Check for sky build
AC_ARG_ENABLE([sky-build],
	       AS_HELP_STRING([--enable-sky-build],[enable sky build (default is no)]),
	       [
		 case "${enableval}" in
		  yes) sky_build_ok=yes
			  echo "Building for sky devices" ;;
		  no)  sky_build_ok=no;;
		  *) AC_MSG_ERROR([bad value ${enableval} for --enable-sky-build]) ;;
		 esac
	       ],
	       [echo "Not enabled for comcast devices"])
AS_IF([test "x$sky_build_ok" = "xyes"], [
    if test "$sky_build_ok" = yes; then
          AC_DEFINE(BUILD_SKY, 1, [Enabled includes for sky])
    else
          AC_MSG_ERROR([sky_build_ok false])
    fi
], [])
plugindir="\$(libdir)/gstreamer-$GST_MAJORMINOR"

BTR_MGR_SRC="src"
BTR_MGR_TEST="tests"


# Checks for typedefs, structures, and compiler characteristics.
AC_TYPE_PID_T
AC_TYPE_SIZE_T

AC_SUBST(plugindir)
AC_SUBST(GST_MAJORMINOR)
AC_SUBST(BTR_MGR_SRC)
AC_SUBST(BTR_MGR_TEST)
AC_SUBST(BREAKPAD_CFLAGS)
AC_SUBST(BREAKPAD_LFLAGS)
AC_SUBST(SYSTEMD_SDNOTIFY_LDFLAGS)
AC_SUBST(SYSTEMD_SDNOTIFY_CFLAGS)

# Checks for library functions.

AC_CONFIG_FILES([Makefile
                 src/Makefile
                 src/main/Makefile
                 src/persistIf/Makefile
                 src/rpc/Makefile
                 src/ifce/Makefile
                 src/audioCap/Makefile
                 src/streamOut/Makefile
                 src/streamIn/Makefile
                 src/sysDiag/Makefile
                 src/columbo/Makefile
                 src/leOnboarding/Makefile
                 tests/Makefile
                 tests/streamInOut/Makefile])

AC_OUTPUT
