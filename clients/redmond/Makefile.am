
INCLUDES = $(all_includes)

kde_module_LTLIBRARIES = libkwinredmond.la

libkwinredmond_la_SOURCES = redmond.cpp
libkwinredmond_la_LIBADD = ../../kwin.la
libkwinredmond_la_LDFLAGS = $(all_libraries) $(KDE_PLUGIN)

METASOURCES = AUTO
noinst_HEADERS = redmond.h

lnkdir = $(kde_datadir)/kwin/
lnk_DATA = redmond.desktop

EXTRA_DIST = $(lnk_DATA)


###KMAKE-start (don't edit or delete this block)

###KMAKE-end
