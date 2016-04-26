SUBDIRS = streamOut
INCLUDE_FILES =


AM_CPPFLAGS = -Wall -Werror -I${top_srcdir}/src/main -I${top_srcdir}/include ${RPC_FLAGS} -I=${includedir}/rdk/iarmbus

bin_PROGRAMS = btMgrTest
btMgrTest_SOURCES = btmgr_test.c

if USE_RPC
AM_LDFLAGS = $(GLIB_LDFLAGS) $(GLIB_LIBS) $(G_THREAD_LIBS) -ldbus-1  -lIARMBus
btMgrTest_LDADD = $(top_builddir)/src/rpc/libBTMgr.la
else
btMgrTest_LDADD = $(top_builddir)/src/main/libBTMgr.la
endif

