
INCLUDES = $(all_includes)

SUBDIRS = . config

kde_module_LTLIBRARIES = kwin_default.la

kwin_default_la_SOURCES = kdedefault.cpp
# kwin_default_la_LDFLAGS = $(all_libraries) $(KDE_PLUGIN) -module
kwin_default_la_LDFLAGS = $(all_libraries) -avoid-version -module $(KDE_RPATH) $(KDE_MT_LDFLAGS)

METASOURCES = AUTO
noinst_HEADERS = kdedefault.h

