SUBDIRS=cli_installer
INCLUDES = $(all_includes) 

kde_module_LTLIBRARIES = kwin_kwmtheme.la

kwin_kwmtheme_la_SOURCES = kwmthemeclient.cpp
#kwin_kwmtheme_la_LDFLAGS = $(all_libraries) $(KDE_PLUGIN) -module
kwin_kwmtheme_la_LDFLAGS = $(all_libraries) -avoid-version -module $(KDE_RPATH) $(KDE_MT_LDFLAGS)

METASOURCES = AUTO
noinst_HEADERS = kwmthemeclient.h
