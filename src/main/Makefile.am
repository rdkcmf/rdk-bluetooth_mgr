## Copyright (c) 2016 Comcast. All rights reserved.
SUBDIRS = 

if !USE_RPC
AM_CPPFLAGS = -Wall -Werror -I${top_srcdir}/include -I=${includedir} ${GLIB_CFLAGS} ${RPC_FLAGS} -g O2
AM_LDFLAGS = $(GLIB_LDFLAGS) $(GLIB_LIBS) $(G_THREAD_LIBS) -lbtrCore -lrdkloggers $(RMFAUDIOCAPTURE_LIBS)

lib_LTLIBRARIES = libBTMgr.la
libBTMgr_la_SOURCES = btmgr.c
libBTMgr_la_includedir = $(includedir)
libBTMgr_la_include_HEADERS = ${top_srcdir}/include/btmgr.h
endif
