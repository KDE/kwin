# $Id$

INCLUDES = $(all_includes)

SUBDIRS = config

kde_module_LTLIBRARIES = kwin_modernsys.la

kwin_modernsys_la_SOURCES = modernsys.cpp
kwin_modernsys_la_LIBADD = ../../kwin.la
kwin_modernsys_la_LDFLAGS = $(all_libraries) $(KDE_PLUGIN) -module

METASOURCES = AUTO
noinst_HEADERS = modernsys.h

lnkdir = $(kde_datadir)/kwin/
lnk_DATA = modernsystem.desktop

EXTRA_DIST = $(lnk_DATA)

