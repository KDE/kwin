SUBDIRS=cli_installer
INCLUDES = $(all_includes) 

kde_module_LTLIBRARIES = kwin3_kwmtheme.la

kwin3_kwmtheme_la_SOURCES = kwmthemeclient.cpp
kwin3_kwmtheme_la_LDFLAGS = $(all_libraries) $(KDE_PLUGIN) -module
kwin3_kwmtheme_la_LIBADD  = -lkdecorations

METASOURCES = AUTO
noinst_HEADERS = kwmthemeclient.h

lnkdir = $(kde_datadir)/kwin
lnk_DATA = kwmtheme.desktop

