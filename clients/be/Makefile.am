
INCLUDES = $(all_includes) 

lib_LTLIBRARIES = libkwinbe.la

libkwinbe_la_SOURCES = beclient.cpp

METASOURCES = AUTO
noinst_HEADERS = beclient.h

lnkdir = $(kde_datadir)/kwin/
lnk_DATA = be.desktop

EXTRA_DIST = $(lnk_DATA)

libkwinbe_la_LDFLAGS = $(all_libraries) -version-info 1:0:0 -module

###KMAKE-start (don't edit or delete this block)

###KMAKE-end
