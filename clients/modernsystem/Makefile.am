# $Id$

INCLUDES = $(all_includes)

SUBDIRS = config

kde_module_LTLIBRARIES = kwin_modernsys.la

kwin_modernsys_la_SOURCES = modernsys.cpp
#kwin_modernsys_la_LDFLAGS = $(all_libraries) $(KDE_PLUGIN) -module
kwin_modernsys_la_LDFLAGS = $(all_libraries) -avoid-version -module $(KDE_RPATH) $(KDE_MT_LDFLAGS)

METASOURCES = AUTO
noinst_HEADERS = modernsys.h

lnkdir = $(kde_datadir)/kwin/
lnk_DATA = modernsystem.desktop

EXTRA_DIST = $(lnk_DATA)

