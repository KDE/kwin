INCLUDES = $(all_includes)

SUBDIRS = . config icewm-themes

kde_module_LTLIBRARIES = libkwinicewm.la

libkwinicewm_la_SOURCES = icewm.cpp 
libkwinicewm_la_LIBADD = ../../kwin.la
libkwinicewm_la_LDFLAGS = $(all_libraries) $(KDE_PLUGIN)

METASOURCES = AUTO
noinst_HEADERS = icewm.h

lnkdir = $(kde_datadir)/kwin/
lnk_DATA = icewm.desktop

EXTRA_DIST = $(lnk_DATA)


###KMAKE-start (don't edit or delete this block)

###KMAKE-end
