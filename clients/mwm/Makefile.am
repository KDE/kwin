
INCLUDES = $(all_includes) 

lib_LTLIBRARIES = libkwinmwm.la

libkwinmwm_la_SOURCES = mwmclient.cpp

METASOURCES = AUTO
noinst_HEADERS = mwmclient.h

lnkdir = $(kde_datadir)/kwin/
lnk_DATA = mwm.desktop

EXTRA_DIST = $(lnk_DATA)

libkwinmwm_la_LDFLAGS = $(all_libraries) -version-info 1:0:0 -module -rdynamic

###KMAKE-start (don't edit or delete this block)

###KMAKE-end
