SUBDIRS = streamInOut
INCLUDE_FILES =


AM_CPPFLAGS = -Wall -Werror -I${top_srcdir}/src/main -I${top_srcdir}/include ${RPC_FLAGS} -I=${includedir}/rdk/iarmbus

bin_PROGRAMS = btMgrTest btMgrHTMLControl
btMgrTest_SOURCES = btmgr_test.c
btMgrHTMLControl_SOURCES = btmgr_test_fcgi.c
btMgrHTMLControl_LDFLAGS = -lfcgi

if USE_RPC
AM_LDFLAGS = -ldbus-1  -lIARMBus
btMgrTest_LDADD     = $(top_builddir)/src/rpc/libBTMgr.la
btMgrHTMLControl_LDADD = $(top_builddir)/src/rpc/libBTMgr.la
else
btMgrTest_LDADD     = $(top_builddir)/src/main/libBTMgr.la
btMgrHTMLControl_LDADD = $(top_builddir)/src/main/libBTMgr.la
endif

