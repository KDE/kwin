
INCLUDES = $(all_includes)

SUBDIRS = . config

kde_module_LTLIBRARIES = libkwindefault.la

libkwindefault_la_SOURCES = kdedefault.cpp
libkwindefault_la_LIBADD = ../../kwin.la
libkwindefault_la_LDFLAGS = $(all_libraries) $(KDE_PLUGIN)

METASOURCES = AUTO
noinst_HEADERS = kdedefault.h

###KMAKE-start (don't edit or delete this block)

###KMAKE-end
