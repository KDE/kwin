SUBDIRS=cli_installer
INCLUDES = $(all_includes) 

kde_module_LTLIBRARIES = libkwinkwmtheme.la

libkwinkwmtheme_la_SOURCES = kwmthemeclient.cpp
libkwinkwmtheme_la_LIBADD = ../../kwin.la
libkwinkwmtheme_la_LDFLAGS = $(all_libraries) $(KDE_PLUGIN)

METASOURCES = AUTO
noinst_HEADERS = kwmthemeclient.h
