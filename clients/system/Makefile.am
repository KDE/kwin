
INCLUDES = $(all_includes)

kde_module_LTLIBRARIES = kwin_system.la

kwin_system_la_SOURCES = systemclient.cpp
kwin_system_la_LIBADD = ../../kwin.la
kwin_system_la_LDFLAGS = $(all_libraries) $(KDE_PLUGIN) -module

METASOURCES = AUTO
noinst_HEADERS = systemclient.h

lnkdir = $(kde_datadir)/kwin/
lnk_DATA = system.desktop

EXTRA_DIST = $(lnk_DATA)


###KMAKE-start (don't edit or delete this block)

###KMAKE-end
