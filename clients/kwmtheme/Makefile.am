SUBDIRS=cli_installer
INCLUDES = $(all_includes) 

kde_module_LTLIBRARIES = kwin_kwmtheme.la

kwin_kwmtheme_la_SOURCES = kwmthemeclient.cpp
kwin_kwmtheme_la_LIBADD = ../../kwin.la
kwin_kwmtheme_la_LDFLAGS = $(all_libraries) $(KDE_PLUGIN) -module

METASOURCES = AUTO
noinst_HEADERS = kwmthemeclient.h
