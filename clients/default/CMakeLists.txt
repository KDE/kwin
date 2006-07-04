
add_subdirectory( config ) 

include_directories( ${CMAKE_SOURCE_DIR}/workspace/kwin/lib  )

########### next target ###############

set(kwin3_default_PART_SRCS kdedefault.cpp )

kde4_automoc(${kwin3_default_PART_SRCS})

kde4_add_plugin(kwin3_default ${kwin3_default_PART_SRCS})

kde4_install_libtool_file( ${PLUGIN_INSTALL_DIR} kwin3_default )

target_link_libraries(kwin3_default  ${KDE4_KDECORE_LIBS} kdefx kdecorations ${QT_QTGUI_LIBRARY})

install(TARGETS kwin3_default  DESTINATION ${PLUGIN_INSTALL_DIR} )


########### install files ###############





#original Makefile.am contents follow:

#
#INCLUDES = -I$(srcdir)/../../lib $(all_includes)
#
#SUBDIRS = . config
#
#kde_module_LTLIBRARIES = kwin3_default.la
#
#kwin3_default_la_SOURCES = kdedefault.cpp
#kwin3_default_la_LDFLAGS = $(all_libraries) $(KDE_PLUGIN) -module
#kwin3_default_la_LIBADD  = $(LIB_KDECORE) -lkdefx ../../lib/libkdecorations.la
#
#METASOURCES = AUTO
#noinst_HEADERS = kdedefault.h
#
