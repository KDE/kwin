INCLUDES = $(all_includes)

lib_LTLIBRARIES = libkwindefault.la

libkwindefault_la_SOURCES = kdedefault.cpp
libkwindefault_la_LIBADD = $(LIB_KDEUI)
libkwindefault_la_LDFLAGS = $(all_libraries) -module

METASOURCES = AUTO
noinst_HEADERS = kdedefault.h

lnkdir = $(kde_datadir)/kwin/
lnk_DATA = kde.desktop

EXTRA_DIST = $(lnk_DATA)


###KMAKE-start (don't edit or delete this block)

###KMAKE-end
