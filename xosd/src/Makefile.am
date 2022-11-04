# Programs.  Don't install testprog.
bin_PROGRAMS 	= osd_cat display_info
noinst_PROGRAMS = testprog

osd_cat_SOURCES  = osd_cat.c
testprog_SOURCES = testprog.c
display_info_SOURCES = display_info.c

osd_cat_LDADD 	= libxosd/libxosd.la
diplsy_info_LDADD = libxosd/libxosd.la
testprog_LDADD 	= libxosd/libxosd.la

include_HEADERS = xosd.h

AM_CFLAGS = ${GTK_CFLAGS}

SUBDIRS=libxosd xmms_plugin bmp_plugin
