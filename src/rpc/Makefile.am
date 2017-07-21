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
AM_CPPFLAGS = -Wall -Werror -I${top_srcdir}/include -I${top_srcdir}/include/priv -I${top_srcdir}/include/rpc ${RPC_FLAGS} -I$(PKG_CONFIG_SYSROOT_DIR)${includedir}/rdk/iarmbus
AM_LDFLAGS = $(GLIB_LDFLAGS) $(GLIB_LIBS) $(G_THREAD_LIBS) -ldbus-1  -lIARMBus -lrdkloggers

lib_LTLIBRARIES = libBTMgr.la
libBTMgr_la_SOURCES = btmgr_iarm_external_interface.c
libBTMgr_la_includedir = $(includedir)
libBTMgr_la_include_HEADERS = ${top_srcdir}/include/btmgr.h
include_HEADERS = ${top_srcdir}/include/rpc/btmgr_iarm_interface.h
endif
