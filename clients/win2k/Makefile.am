
INCLUDES = $(all_includes)

kde_module_LTLIBRARIES = libkwin2k.la

libkwin2k_la_SOURCES = win2k.cpp
libkwin2k_la_LIBADD = ../../kwin.la
libkwin2k_la_LDFLAGS = $(all_libraries) $(KDE_PLUGIN)

METASOURCES = AUTO
noinst_HEADERS = win2k.h

lnkdir = $(kde_datadir)/kwin/
lnk_DATA = win2k.desktop

EXTRA_DIST = $(lnk_DATA)


###KMAKE-start (don't edit or delete this block)

###KMAKE-end
