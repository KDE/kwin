# FRAME libkwin???
lib_LTLIBRARIES = libkdecorations.la

libkdecorations_la_SOURCES = kdecoration.cpp kdecoration_p.cpp kdecoration_plugins_p.cpp \
        kdecorationfactory.cpp kcommondecoration.cpp
libkdecorations_la_LIBADD = $(LIB_KDECORE)
libkdecorations_la_LDFLAGS = $(all_libraries) -version-info 1:0:0 -no-undefined

# FRAME
include_HEADERS = kdecoration.h kdecoration_p.h kdecoration_plugins_p.h \
        kdecorationfactory.h kcommondecoration.h

INCLUDES = $(all_includes)
METASOURCES = AUTO

include ../../admin/Doxyfile.am

messages: rc.cpp
	$(XGETTEXT) `find . -name \*.cpp` -o $(podir)/kwin_lib.pot
	-rm rc.cpp
