## Copyright (c) 2016 Comcast. All rights reserved.
SUBDIRS = 

if USE_RPC
AM_CPPFLAGS = -Wall -Werror -I${top_srcdir}/src/main -I${top_srcdir}/include ${RPC_FLAGS} -I=${includedir}/rdk/iarmbus
AM_LDFLAGS = $(GLIB_LDFLAGS) $(GLIB_LIBS) $(G_THREAD_LIBS) -ldbus-1  -lIARMBus -lrdkloggers

lib_LTLIBRARIES = libBTMgr.la
libBTMgr_la_SOURCES = btmgr_iarm_external_interface.c
libBTMgr_la_includedir = $(includedir)
libBTMgr_la_include_HEADERS = ${top_srcdir}/include/btmgr.h
endif
