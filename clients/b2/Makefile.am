
INCLUDES = $(all_includes) 

kde_module_LTLIBRARIES = libkwinb2.la

libkwinb2_la_SOURCES = b2client.cpp
libkwinb2_la_LIBADD = ../../kwin.la
libkwinb2_la_LDFLAGS = $(all_libraries) $(KDE_PLUGIN)

METASOURCES = AUTO
noinst_HEADERS = b2client.h

lnkdir = $(kde_datadir)/kwin/
lnk_DATA = b2.desktop

EXTRA_DIST = $(lnk_DATA)


###KMAKE-start (don't edit or delete this block)

###KMAKE-end
