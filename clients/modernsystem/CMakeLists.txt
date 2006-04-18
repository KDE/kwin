kde4_header()

add_subdirectory( config ) 

include_directories( ${CMAKE_SOURCE_DIR}/workspace/kwin/lib ${KDE4_INCLUDES} )


########### next target ###############

set(kwin3_modernsys_PART_SRCS modernsys.cpp )

kde4_automoc(${kwin3_modernsys_PART_SRCS})

kde4_add_plugin(kwin3_modernsys ${kwin3_modernsys_PART_SRCS})

kde4_install_libtool_file( ${PLUGIN_INSTALL_DIR} kwin3_modernsys )

target_link_libraries(kwin3_modernsys  ${KDE4_KDECORE_LIBS} kdecorations kdefx ${QT_QTGUI_LIBRARY})

install_targets(${PLUGIN_INSTALL_DIR} kwin3_modernsys )


########### install files ###############

install_files( ${DATA_INSTALL_DIR}/kwin/ FILES modernsystem.desktop )

kde4_footer()



#original Makefile.am contents follow:

#
#INCLUDES = -I$(srcdir)/../../lib $(all_includes)
#
#SUBDIRS = config
#
#kde_module_LTLIBRARIES = kwin3_modernsys.la
#
#kwin3_modernsys_la_SOURCES = modernsys.cpp
#kwin3_modernsys_la_LIBADD = ../../lib/libkdecorations.la -lkdefx
#kwin3_modernsys_la_LDFLAGS = $(all_libraries) $(KDE_PLUGIN) -module
#
#METASOURCES = AUTO
#noinst_HEADERS = modernsys.h
#
#lnkdir = $(kde_datadir)/kwin/
#lnk_DATA = modernsystem.desktop
#
#EXTRA_DIST = $(lnk_DATA)
#
