INCLUDES = $(all_includes) 

lib_LTLIBRARIES = libkwinriscos.la

libkwinriscos_la_SOURCES = riscosclient.cpp

METASOURCES = AUTO
noinst_HEADERS = riscosclient.h

lnkdir = $(kde_datadir)/kwin/
lnk_DATA = riscos.desktop

EXTRA_DIST = $(lnk_DATA)

libkwinriscos_la_LDFLAGS = $(all_libraries) -version-info 1:0:0 -module -rdynamic 

###KMAKE-start (don't edit or delete this block)

###KMAKE-end
