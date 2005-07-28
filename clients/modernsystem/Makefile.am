
INCLUDES = -I$(srcdir)/../../lib $(all_includes)

SUBDIRS = config

kde_module_LTLIBRARIES = kwin3_modernsys.la

kwin3_modernsys_la_SOURCES = modernsys.cpp
kwin3_modernsys_la_LIBADD = ../../lib/libkdecorations.la -lkdefx
kwin3_modernsys_la_LDFLAGS = $(all_libraries) $(KDE_PLUGIN) -module

METASOURCES = AUTO
noinst_HEADERS = modernsys.h

lnkdir = $(kde_datadir)/kwin/
lnk_DATA = modernsystem.desktop

EXTRA_DIST = $(lnk_DATA)

