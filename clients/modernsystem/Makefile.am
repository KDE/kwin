# $Id$

INCLUDES = $(all_includes)

SUBDIRS = config

kde_module_LTLIBRARIES = libkwinmodernsys.la

libkwinmodernsys_la_SOURCES = modernsys.cpp
libkwinmodernsys_la_LIBADD = ../../kwin.la
libkwinmodernsys_la_LDFLAGS = $(all_libraries) $(KDE_PLUGIN)

METASOURCES = AUTO
noinst_HEADERS = modernsys.h

lnkdir = $(kde_datadir)/kwin/
lnk_DATA = modernsystem.desktop

EXTRA_DIST = $(lnk_DATA)

