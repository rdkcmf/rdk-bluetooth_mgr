SUBDIRS = streamOut
INCLUDE_FILES =


AM_CPPFLAGS = -Wall -Werror -I${top_srcdir}/src/main -I${top_srcdir}/include ${RPC_FLAGS} -I=${includedir}/rdk/iarmbus

bin_PROGRAMS = btMgrTest btMgrHTMLTest
btMgrTest_SOURCES = btmgr_test.c
btMgrHTMLTest_SOURCES = btmgr_test_fcgi.c
btMgrHTMLTest_LDFLAGS = -lfcgi

if USE_RPC
AM_LDFLAGS = -ldbus-1  -lIARMBus
btMgrTest_LDADD     = $(top_builddir)/src/rpc/libBTMgr.la
btMgrHTMLTest_LDADD = $(top_builddir)/src/rpc/libBTMgr.la
else
btMgrTest_LDADD     = $(top_builddir)/src/main/libBTMgr.la
btMgrHTMLTest_LDADD = $(top_builddir)/src/main/libBTMgr.la
endif

