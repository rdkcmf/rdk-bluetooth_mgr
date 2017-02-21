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

INCLUDE_FILES =

AM_CPPFLAGS = -Wall -Werror -I${top_srcdir}/include/common -I${top_srcdir}/include/audioCap -I${top_srcdir}/include/streamOut -I${top_srcdir}/include/streamIn
bin_PROGRAMS = btrMgrStreamInOutCoreTest

btrMgrStreamInOutCoreTest_SOURCES = btrMgr_streamInOutCoreTest.c
btrMgrStreamInOutCoreTest_CFLAGS = $(INCLUDE_FILES) $(GLIB_CFLAGS)
btrMgrStreamInOutCoreTest_LDADD = $(top_builddir)/src/audioCap/libbtrMgrAudioCapture.la $(top_builddir)/src/streamOut/libbtrMgrStreamOut.la $(top_builddir)/src/streamIn/libbtrMgrStreamIn.la $(BTRCORE_LIBS)
