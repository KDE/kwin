
INCLUDES = $(all_includes) 

kde_module_LTLIBRARIES = kwin_kde1.la

kwin_kde1_la_SOURCES = kde1client.cpp
kwin_kde1_la_LIBADD = ../../kwin.la
kwin_kde1_la_LDFLAGS = $(all_libraries) $(KDE_PLUGIN) -module

METASOURCES = AUTO
noinst_HEADERS = kde1client.h

lnkdir = $(kde_datadir)/kwin/
lnk_DATA = kde1.desktop
EXTRA_DIST = $(lnk_DATA)
