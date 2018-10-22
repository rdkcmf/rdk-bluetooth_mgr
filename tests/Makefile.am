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
SUBDIRS = streamInOut
INCLUDE_FILES =


AM_CPPFLAGS = -Wall -Werror -I${top_srcdir}/src/main -I${top_srcdir}/include ${RPC_FLAGS} -I$(PKG_CONFIG_SYSROOT_DIR)${includedir}/rdk/iarmbus -I$(PKG_CONFIG_SYSROOT_DIR)${includedir}/wdmp-c -I$(PKG_CONFIG_SYSROOT_DIR)${includedir}

bin_PROGRAMS = btrMgrTest btrMgrHTMLControl
btrMgrTest_SOURCES = btrMgr_test.c
btrMgrHTMLControl_SOURCES = btrMgr_testFcgi.c
btrMgrHTMLControl_LDFLAGS = -lfcgi

if USE_RPC
AM_LDFLAGS = -ldbus-1  -lIARMBus -lrfcapi
btrMgrTest_LDADD     = $(top_builddir)/src/rpc/libBTMgr.la
btrMgrHTMLControl_LDADD = $(top_builddir)/src/rpc/libBTMgr.la
else
btrMgrTest_LDADD     = $(top_builddir)/src/ifce/libBTMgr.la
btrMgrHTMLControl_LDADD = $(top_builddir)/src/ifce/libBTMgr.la
endif

