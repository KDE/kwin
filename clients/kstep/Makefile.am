
INCLUDES = $(all_includes) 

kde_module_LTLIBRARIES = libkwinkstep.la

libkwinkstep_la_SOURCES = nextclient.cpp
libkwinkstep_la_LIBADD = ../../kwin.la
libkwinkstep_la_LDFLAGS = $(all_libraries) $(KDE_PLUGIN)

METASOURCES = AUTO
noinst_HEADERS = nextclient.h

lnkdir = $(kde_datadir)/kwin/
lnk_DATA = kstep.desktop

EXTRA_DIST = $(lnk_DATA)


###KMAKE-start (don't edit or delete this block)

###KMAKE-end
