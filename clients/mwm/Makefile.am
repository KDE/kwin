
INCLUDES = $(all_includes) 

kde_module_LTLIBRARIES = libkwinmwm.la

libkwinmwm_la_SOURCES = mwmclient.cpp
libkwinmwm_la_LIBADD = ../../kwin.la
libkwinmwm_la_LDFLAGS = $(all_libraries) $(KDE_PLUGIN)

METASOURCES = AUTO
noinst_HEADERS = mwmclient.h

lnkdir = $(kde_datadir)/kwin/
lnk_DATA = mwm.desktop

EXTRA_DIST = $(lnk_DATA)


###KMAKE-start (don't edit or delete this block)

###KMAKE-end
