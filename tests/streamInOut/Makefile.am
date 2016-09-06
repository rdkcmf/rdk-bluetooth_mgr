SUBDIRS =

INCLUDE_FILES =

AM_CPPFLAGS = -Wall -Werror -I${top_srcdir}/include/streamOut -I${top_srcdir}/include/streamIn
bin_PROGRAMS = btrMgrStreamInOutCoreTest

btrMgrStreamInOutCoreTest_SOURCES = btrMgr_streamInOutCoreTest.c
btrMgrStreamInOutCoreTest_CFLAGS = $(INCLUDE_FILES) $(GLIB_CFLAGS)
btrMgrStreamInOutCoreTest_LDADD = $(top_builddir)/src/streamOut/libbtrMgrStreamOut.la $(top_builddir)/src/streamIn/libbtrMgrStreamIn.la $(BTRCORE_LIBS) $(RMFAUDIOCAPTURE_LIBS)
