
INCLUDES = $(all_includes)

SUBDIRS = . config

kde_module_LTLIBRARIES = libkwinglow.la

libkwinglow_la_SOURCES = glowclient.cpp glowbutton.cpp
libkwinglow_la_LIBADD = ../../kwin.la
libkwinglow_la_LDFLAGS = $(all_libraries) $(KDE_PLUGIN)

METASOURCES = AUTO
noinst_HEADERS = glowclient.h bitmaps.h glowbutton.h

datadir = $(kde_datadir)/kwin/
data_DATA = glow.desktop

confdir = $(kde_confdir)
conf_DATA = kwinglowrc

EXTRA_DIST = $(data_DATA) $(conf_DATA)

###KMAKE-start (don't edit or delete this block)

###KMAKE-end
