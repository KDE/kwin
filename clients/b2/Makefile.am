
INCLUDES = $(all_includes) 

lib_LTLIBRARIES = libkwinb2.la

libkwinb2_la_SOURCES = b2client.cpp

METASOURCES = AUTO
noinst_HEADERS = b2client.h

lnkdir = $(kde_datadir)/kwin/
lnk_DATA = b2.desktop

EXTRA_DIST = $(lnk_DATA)

libkwinb2_la_LDFLAGS = $(all_libraries) -version-info 1:0:0 -module

###KMAKE-start (don't edit or delete this block)

###KMAKE-end
