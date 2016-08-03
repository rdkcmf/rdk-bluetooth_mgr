SUBDIRS = 

INCLUDE_FILES =

AM_CPPFLAGS = -Wall -Werror -I${top_srcdir}/include/streamIn	
AM_LDFLAGS =

if USE_GST1
AM_CPPFLAGS += $(GST_CFLAGS)
AM_LDFLAGS += $(GST_LIBS) -lgstapp-1.0
endif

lib_LTLIBRARIES = libbtrMgrStreamIn.la
libbtrMgrStreamIn_la_SOURCES = btrMgr_streamIn.c
libbtrMgrStreamIn_la_CFLAGS = $(INCLUDE_FILES) $(GLIB_CFLAGS)
libbtrMgrStreamIn_la_LIBADD = $(GLIB_LIBS)

if USE_GST1
libbtrMgrStreamIn_la_SOURCES += btrMgr_streamInGst.c
endif

include_HEADERS = $(top_srcdir)/include/streamIn/btrMgr_streamIn.h

