
INCLUDES = $(all_includes)

SUBDIRS = . config

kde_module_LTLIBRARIES = kwin_quartz.la

kwin_quartz_la_SOURCES = quartz.cpp
kwin_quartz_la_LIBADD = ../../kwin.la
kwin_quartz_la_LDFLAGS = $(all_libraries) $(KDE_PLUGIN) -module

METASOURCES = AUTO
noinst_HEADERS = quartz.h

lnkdir = $(kde_datadir)/kwin/
lnk_DATA = quartz.desktop

EXTRA_DIST = $(lnk_DATA)


###KMAKE-start (don't edit or delete this block)

###KMAKE-end
