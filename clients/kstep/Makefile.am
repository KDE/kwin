
INCLUDES = $(all_includes) 

lib_LTLIBRARIES = libkwinkstep.la

libkwinkstep_la_SOURCES = nextclient.cpp

METASOURCES = AUTO
noinst_HEADERS = nextclient.h

lnkdir = $(kde_datadir)/kwin/
lnk_DATA = kstep.desktop

EXTRA_DIST = $(lnk_DATA)

libkwinkstep_la_LDFLAGS = $(all_libraries) -version-info 1:0:0 -module -rdynamic

###KMAKE-start (don't edit or delete this block)

###KMAKE-end
