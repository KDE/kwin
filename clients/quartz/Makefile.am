
INCLUDES = $(all_includes)

SUBDIRS = . config

kde_module_LTLIBRARIES = kwin_quartz.la

kwin_quartz_la_SOURCES = quartz.cpp
#kwin_quartz_la_LDFLAGS = $(all_libraries) $(KDE_PLUGIN) -module
kwin_quartz_la_LDFLAGS = $(all_libraries) -avoid-version -module $(KDE_RPATH) $(KDE_MT_LDFLAGS)

METASOURCES = AUTO
noinst_HEADERS = quartz.h

lnkdir = $(kde_datadir)/kwin/
lnk_DATA = quartz.desktop

EXTRA_DIST = $(lnk_DATA)


###KMAKE-start (don't edit or delete this block)

###KMAKE-end
