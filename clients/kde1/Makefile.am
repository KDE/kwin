
INCLUDES = $(all_includes) 

kde_module_LTLIBRARIES = libkwinkde1.la

libkwinkde1_la_SOURCES = kde1client.cpp
libkwinkde1_la_LIBADD = ../../kwin.la
libkwinkde1_la_LDFLAGS = $(all_libraries) $(KDE_PLUGIN)

METASOURCES = AUTO
noinst_HEADERS = kde1client.h

lnkdir = $(kde_datadir)/kwin/
lnk_DATA = kde1.desktop
EXTRA_DIST = $(lnk_DATA)
