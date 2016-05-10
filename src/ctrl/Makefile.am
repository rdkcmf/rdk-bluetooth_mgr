## Copyright (c) 2016 Comcast. All rights reserved.
SUBDIRS = 

if USE_RPC
AM_CPPFLAGS = -Wall -Werror -I${top_srcdir}/src/main -I${top_srcdir}/src/rpc -I${top_srcdir}/include -I${top_srcdir}/include/streamOut ${RPC_FLAGS} -I=${includedir}/rdk/iarmbus  ${GLIB_CFLAGS} ${RPC_FLAGS}
AM_LDFLAGS = $(GLIB_LDFLAGS) $(GLIB_LIBS) $(G_THREAD_LIBS) -ldbus-1  -lIARMBus -lrdkloggers -lbtrCore $(RMFAUDIOCAPTURE_LIBS)

bin_PROGRAMS = btMgrBus 
btMgrBus_SOURCES = btmgr_bus.c ${top_srcdir}/src/rpc/btmgr_iarm_internal_interface.c ${top_srcdir}/src/main/btmgr.c
btMgrBus_LDADD = $(top_builddir)/src/streamOut/libbtrMgrStreamOut.la
endif
