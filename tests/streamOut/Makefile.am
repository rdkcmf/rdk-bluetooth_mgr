SUBDIRS =

INCLUDE_FILES =

AM_CPPFLAGS = -Wall -Werror -I${top_srcdir}/include/streamOut
bin_PROGRAMS = btrMgrStreamOutTest
btrMgrStreamOutTest_SOURCES = btrMgr_streamOutTest.c
btrMgrStreamOutTest_CFLAGS = $(INCLUDE_FILES)
btrMgrStreamOutTest_LDADD = $(top_builddir)/src/streamOut/libbtrMgrStreamOut.la
