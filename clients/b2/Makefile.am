
INCLUDES = $(all_includes) 

SUBDIRS = . config

kde_module_LTLIBRARIES = kwin_b2.la

kwin_b2_la_SOURCES = b2client.cpp
# kwin_b2_la_LDFLAGS = $(all_libraries) $(KDE_PLUGIN) -module
kwin_b2_la_LDFLAGS = $(all_libraries) -avoid-version -module $(KDE_RPATH) $(KDE_MT_LDFLAGS)

METASOURCES = AUTO
noinst_HEADERS = b2client.h

lnkdir = $(kde_datadir)/kwin/
lnk_DATA = b2.desktop

EXTRA_DIST = $(lnk_DATA)


###KMAKE-start (don't edit or delete this block)

###KMAKE-end
