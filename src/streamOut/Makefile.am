SUBDIRS = 

INCLUDE_FILES =

AM_CPPFLAGS = -Wall -Werror -I${top_srcdir}/include/streamOut
AM_LDFLAGS =

if USE_GST1
AM_CPPFLAGS += $(GST_CFLAGS)
AM_LDFLAGS += $(GST_LIBS)
endif

lib_LTLIBRARIES = libbtrMgrStreamOut.la
libbtrMgrStreamOut_la_SOURCES = btrMgr_streamOut.c
libbtrMgrStreamOut_la_CFLAGS = $(INCLUDE_FILES)

if USE_GST1
libbtrMgrStreamOut_la_SOURCES += btrMgr_streamOutGst.c
endif

include_HEADERS = $(top_srcdir)/include/streamOut/btrMgr_streamOut.h

