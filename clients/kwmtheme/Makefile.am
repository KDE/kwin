
INCLUDES = $(all_includes) 

kde_module_LTLIBRARIES = libkwinkwmtheme.la

libkwinkwmtheme_la_SOURCES = kwmthemeclient.cpp
libkwinkwmtheme_la_LIBADD = ../../kwin.la
libkwinkwmtheme_la_LDFLAGS = $(all_libraries) $(KDE_PLUGIN)

METASOURCES = AUTO
noinst_HEADERS = kwmthemeclient.h

#lnkdir = $(kde_datadir)/kwin/
#lnk_DATA = kwmtheme.desktop

#EXTRA_DIST = $(lnk_DATA)


###KMAKE-start (don't edit or delete this block)

###KMAKE-end
