SUBDIRS =

INCLUDE_FILES =

AM_CPPFLAGS = -Wall -Werror -I${top_srcdir}/include/streamOut -I${top_srcdir}/include/streamIn
bin_PROGRAMS = btrMgrStreamOutTest btrMgrStreamOutCoreTest

btrMgrStreamOutTest_SOURCES = btrMgr_streamOutTest.c
btrMgrStreamOutTest_CFLAGS = $(INCLUDE_FILES)
btrMgrStreamOutTest_LDADD = $(top_builddir)/src/streamOut/libbtrMgrStreamOut.la $(top_builddir)/src/streamIn/libbtrMgrStreamIn.la

btrMgrStreamOutCoreTest_SOURCES = btrMgr_streamOutCoreTest.c
btrMgrStreamOutCoreTest_CFLAGS = $(INCLUDE_FILES) $(GLIB_CFLAGS)
btrMgrStreamOutCoreTest_LDADD = $(top_builddir)/src/streamOut/libbtrMgrStreamOut.la $(top_builddir)/src/streamIn/libbtrMgrStreamIn.la $(BTRCORE_LIBS) $(RMFAUDIOCAPTURE_LIBS)
