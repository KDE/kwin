


########### next target ###############

set(kdecorations_LIB_SRCS 
   kdecoration.cpp 
   kdecoration_p.cpp 
   kdecoration_plugins_p.cpp 
   kdecorationfactory.cpp 
   kcommondecoration.cpp )

kde4_automoc(${kdecorations_LIB_SRCS})

kde4_add_library(kdecorations SHARED ${kdecorations_LIB_SRCS})

target_link_libraries(kdecorations  ${KDE4_KDECORE_LIBS} ${QT_QTGUI_LIBRARY})

set_target_properties(kdecorations PROPERTIES VERSION 1.0.0 SOVERSION 1 )
install(TARGETS kdecorations  DESTINATION ${LIB_INSTALL_DIR} )


########### install files ###############

install( FILES kdecoration.h kdecoration_p.h kdecoration_plugins_p.h kdecorationfactory.h kcommondecoration.h DESTINATION ${INCLUDE_INSTALL_DIR})




#original Makefile.am contents follow:

## FRAME libkwin???
#lib_LTLIBRARIES = libkdecorations.la
#
#libkdecorations_la_SOURCES = kdecoration.cpp kdecoration_p.cpp kdecoration_plugins_p.cpp \
#        kdecorationfactory.cpp kcommondecoration.cpp
#libkdecorations_la_LIBADD = $(LIB_KDECORE)
#libkdecorations_la_LDFLAGS = $(all_libraries) -version-info 1:0:0 -no-undefined
#
## FRAME
#include_HEADERS = kdecoration.h kdecoration_p.h kdecoration_plugins_p.h \
#        kdecorationfactory.h kcommondecoration.h
#
#INCLUDES = $(all_includes)
#METASOURCES = AUTO
#
#include ../../../admin/Doxyfile.am
#
#messages: rc.cpp
#	$(XGETTEXT) `find . -name \*.cpp` -o $(podir)/kwin_lib.pot
#	-rm rc.cpp
