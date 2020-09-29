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
SUBDIRS =
if USE_RPC
AM_CPPFLAGS = -Wall -Werror -I${top_srcdir}/include -I${top_srcdir}/include/rpc -I${top_srcdir}/include/common -I${top_srcdir}/include/audioCap -I${top_srcdir}/include/persistIf -I${top_srcdir}/include/streamOut  -I${top_srcdir}/include/streamIn -I${top_srcdir}/include/logger -I${top_srcdir}/include/sysDiag -I${top_srcdir}/include/columbo -I${top_srcdir}/include/leOnboarding ${RPC_FLAGS} -I$(PKG_CONFIG_SYSROOT_DIR)${includedir}/rdk/iarmbus  -I$(PKG_CONFIG_SYSROOT_DIR)${includedir}/wdmp-c  -I$(PKG_CONFIG_SYSROOT_DIR)${includedir} ${GLIB_CFLAGS} ${RPC_FLAGS} $(BREAKPAD_CFLAGS) $(SYSTEMD_SDNOTIFY_CFLAGS)
AM_LDFLAGS = $(GLIB_LDFLAGS) $(GLIB_LIBS) $(G_THREAD_LIBS) $(BTRCORE_LIBS) -ldbus-1 -lIARMBus -lrdkloggers -lrfcapi $(BREAKPAD_LFLAGS) $(SYSTEMD_SDNOTIFY_LDFLAGS)

if WITH_PI_BUILD
AM_CPPFLAGS += $(PI_BUILD_FLAG)
endif
bin_PROGRAMS = btMgrBus 
btMgrBus_SOURCES = btrMgr_main.c ${top_srcdir}/src/rpc/btmgr_iarm_internal_interface.c ${top_srcdir}/src/ifce/btrMgr.c
btMgrBus_LDADD = $(top_builddir)/src/audioCap/libbtrMgrAudioCapture.la $(top_builddir)/src/persistIf/libbtrMgrPersistInterface.la $(top_builddir)/src/streamOut/libbtrMgrStreamOut.la $(top_builddir)/src/streamIn/libbtrMgrStreamIn.la $(top_builddir)/src/sysDiag/libbtrMgrSysDiag.la $(top_builddir)/src/columbo/libbtrMgrColumbo.la $(top_builddir)/src/leOnboarding/libbtrMgrLeOnboarding.la
endif

