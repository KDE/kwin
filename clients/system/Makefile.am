
INCLUDES = $(all_includes)

kde_module_LTLIBRARIES = libkwinsystem.la

libkwinsystem_la_SOURCES = systemclient.cpp
libkwinsystem_la_LIBADD = ../../kwin.la
libkwinsystem_la_LDFLAGS = $(all_libraries) $(KDE_PLUGIN)

METASOURCES = AUTO
noinst_HEADERS = systemclient.h

lnkdir = $(kde_datadir)/kwin/
lnk_DATA = system.desktop

EXTRA_DIST = $(lnk_DATA)


###KMAKE-start (don't edit or delete this block)

###KMAKE-end
