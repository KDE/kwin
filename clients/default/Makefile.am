
INCLUDES = $(all_includes)

SUBDIRS = . config

kde_module_LTLIBRARIES = kwin_default.la

kwin_default_la_SOURCES = kdedefault.cpp
kwin_default_la_LIBADD = ../../kwin.la
kwin_default_la_LDFLAGS = $(all_libraries) $(KDE_PLUGIN) -module

METASOURCES = AUTO
noinst_HEADERS = kdedefault.h

lnkdir = $(kde_datadir)/kwin
lnk_DATA = kde2.desktop

###KMAKE-start (don't edit or delete this block)

###KMAKE-end
