
INCLUDES = $(all_includes)

kde_module_LTLIBRARIES = libkwinlaptop.la

libkwinlaptop_la_SOURCES = laptopclient.cpp
libkwinlaptop_la_LIBADD = ../../kwin.la
libkwinlaptop_la_LDFLAGS = $(all_libraries) $(KDE_PLUGIN)

METASOURCES = AUTO
noinst_HEADERS = laptopclient.h

lnkdir = $(kde_datadir)/kwin/
lnk_DATA = laptop.desktop

EXTRA_DIST = $(lnk_DATA)


###KMAKE-start (don't edit or delete this block)

###KMAKE-end
