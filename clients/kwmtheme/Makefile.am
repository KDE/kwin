SUBDIRS=applytheme defaultpics

INCLUDES = $(all_includes) 

lib_LTLIBRARIES = libkwinkwmtheme.la

libkwinkwmtheme_la_SOURCES = kwmthemeclient.cpp

METASOURCES = AUTO
noinst_HEADERS = kwmthemeclient.h

lnkdir = $(kde_datadir)/kwin/
lnk_DATA = kwmtheme.desktop

EXTRA_DIST = $(lnk_DATA)

libkwinkwmtheme_la_LDFLAGS = $(all_libraries) -version-info 1:0:0 -module -rdynamic

###KMAKE-start (don't edit or delete this block)

###KMAKE-end
