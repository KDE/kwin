
INCLUDES = $(all_includes) 

lib_LTLIBRARIES = libkwinsystem.la

libkwinsystem_la_SOURCES = systemclient.cpp

METASOURCES = AUTO
noinst_HEADERS = systemclient.h

lnkdir = $(kde_datadir)/kwin/
lnk_DATA = system.desktop

EXTRA_DIST = $(lnk_DATA)

libkwinsystem_la_LDFLAGS = $(all_libraries) -version-info 1:0:0 -module -rdynamic

###KMAKE-start (don't edit or delete this block)

###KMAKE-end
