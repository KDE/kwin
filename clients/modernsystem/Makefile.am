
INCLUDES = $(all_includes)

lib_LTLIBRARIES = libkwinmodernsys.la

libkwinmodernsys_la_SOURCES = modernsys.cpp

METASOURCES = AUTO
noinst_HEADERS = modernsys.h

lnkdir = $(kde_datadir)/kwin/
lnk_DATA = modernsystem.desktop

EXTRA_DIST = $(lnk_DATA)

libkwinmodernsys_la_LDFLAGS = $(all_libraries) -version-info 1:0:0 -module
libkwinmodernsys_la_LIBADD = $(LIB_KDEUI)

###KMAKE-start (don't edit or delete this block)

###KMAKE-end
