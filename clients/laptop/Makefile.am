
INCLUDES = $(all_includes)

lib_LTLIBRARIES = libkwinlaptop.la

libkwinlaptop_la_SOURCES = laptopclient.cpp

METASOURCES = AUTO
noinst_HEADERS = laptopclient.h

lnkdir = $(kde_datadir)/kwin/
lnk_DATA = laptop.desktop

EXTRA_DIST = $(lnk_DATA)

libkwinlaptop_la_LDFLAGS = $(all_libraries) -version-info 1:0:0 -module
libkwinlaptop_la_LIBADD = $(LIB_KDEUI)

###KMAKE-start (don't edit or delete this block)

###KMAKE-end
