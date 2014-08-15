#
# $Id$
#

bin_PROGRAMS = register 
 
AM_CPPFLAGS = \
	-I${top_srcdir}/libhttpd/ \
	-DSYSCONFDIR='"$(sysconfdir)"' 
register_LDADD = $(top_builddir)/libhttpd/libhttpd.la -lm

register_SOURCES = commandline.c \
	conf.c \
	debug.c \
	gateway.c \
	http.c \
	util.c \
	fetchconf.c \
	safe.c \
	httpd_thread.c \
	cJSON.c
	
noinst_HEADERS = commandline.h \
	common.h \
	conf.h \
	debug.h \
	gateway.h \
	http.h \
	util.h \
	fetchconf.h \
	safe.h \
	httpd_thread.h \
	cJSON.h

