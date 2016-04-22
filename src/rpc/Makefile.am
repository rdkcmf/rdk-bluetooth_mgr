## Copyright (c) 2016 Comcast. All rights reserved.
SUBDIRS = 

if USE_RPC
AM_CPPFLAGS = -Wall -Werror -I${top_srcdir}/src/main -I${top_srcdir}/include ${RPC_FLAGS} -I=${includedir}/rdk/iarmbus
AM_LDFLAGS = $(GLIB_LDFLAGS) $(GLIB_LIBS) $(G_THREAD_LIBS) -ldbus-1  -lIARMBus -lrdkloggers

#lib_LTLIBRARIES = libBTMgrExternal.la libBTMgrInternal.la
lib_LTLIBRARIES = libBTMgr.la
libBTMgr_la_SOURCES = btmgr_iarm_external_interface.c
libBTMgr_la_includedir = $(includedir)
libBTMgr_la_include_HEADERS = btmgr_iarm_interface.h

#libBTMgrExternal_la_SOURCES = btmgr_iarm_external_interface.c
#libBTMgrExternal_la_includedir = $(includedir)
#libBTMgrExternal_la_include_HEADERS = btmgr_iarm_interface.h

#libBTMgrInternal_la_SOURCES = btmgr_iarm_internal_interface.c
#libBTMgrInternal_la_includedir = $(includedir)
#libBTMgrInternal_la_include_HEADERS = btmgr_iarm_interface.h
endif
