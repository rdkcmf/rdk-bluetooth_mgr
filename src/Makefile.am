SUBDIRS = 

INCLUDE_FILES = 

AM_CPPFLAGS = -Wall -Werror -I${top_srcdir}/include -I${top_srcdir}/include/ctrl -I${top_srcdir}/include/iarmBtIfce -I${top_srcdir}/include/streamOut
lib_LTLIBRARIES = libbtrMgr.la
libbtrMgr_la_SOURCES =  ctrl/btrMgr_ctrl.c iarmBtIfce/btrMgr_iarmBtIfce.c streamOut/btrMgr_streamOut.c
libbtrMgr_la_CFLAGS = $(INCLUDE_FILES)

bin_PROGRAMS = btrMgrService
btrMgrService_SOURCES = btrMgr_serviceMain.c
btrMgrService_LDADD = libbtrMgr.la
btrMgrService_CFLAGS = $(INCLUDE_FILES)

include_HEADERS = $(top_srcdir)/include/*.h

