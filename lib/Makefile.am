# FRAME libkwin???
lib_LTLIBRARIES = libkwin.la

libkwin_la_SOURCES = kdecoration.cpp kdecoration_p.cpp kdecoration_plugins_p.cpp \
        kdecorationfactory.cpp
libkwin_la_LIBADD = $(LIB_KDECORE)
libkwin_la_LDFLAGS = $(all_libraries) -version-info 1:0:0 -no-undefined

# FRAME
include_HEADERS = kdecoration.h kdecoration_p.h kdecoration_plugins_p.h \
        kdecorationfactory.h

INCLUDES = $(all_includes)
METASOURCES = AUTO

include ../../admin/Doxyfile.am

