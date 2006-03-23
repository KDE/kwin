kde4_header()

add_subdirectory( config )

include_directories( ${CMAKE_SOURCE_DIR}/workspace/kwin/lib ${KDE4_INCLUDE_DIR} ${QT_INCLUDES} )


########### next target ###############

set(kwin3_plastik_PART_SRCS 
   plastik.cpp 
   plastikclient.cpp 
   plastikbutton.cpp 
   misc.cpp )

kde4_automoc(${kwin3_plastik_PART_SRCS})

kde4_add_plugin(kwin3_plastik ${kwin3_plastik_PART_SRCS})

kde4_install_libtool_file( ${PLUGIN_INSTALL_DIR} kwin3_plastik )

target_link_libraries(kwin3_plastik  ${KDE4_KDECORE_LIBS} kdeui kdecorations kdefx)

install_targets(${LIB_INSTALL_DIR}/kde4 kwin3_plastik )


########### install files ###############

install_files( ${DATA_INSTALL_DIR}/kwin/ FILES plastik.desktop )

kde4_footer()



#original Makefile.am contents follow:

#AUTOMAKE_OPTIONS = foreign
#
#SUBDIRS = config
#
#KDE_CXXFLAGS = -DQT_PLUGIN
#
#INCLUDES = -I$(srcdir)/../../lib $(all_includes)
#
#kwindir = $(kde_datadir)/kwin/
#kwin_DATA = plastik.desktop
#
#kde_module_LTLIBRARIES = kwin3_plastik.la
#kwin3_plastik_la_SOURCES = plastik.cpp plastikclient.cpp plastikbutton.cpp misc.cpp
#kwin3_plastik_la_LDFLAGS = $(all_libraries) $(KDE_PLUGIN) -module
#kwin3_plastik_la_LIBADD = $(LIB_KDEUI) ../../lib/libkdecorations.la
#kwin3_plastik_la_METASOURCES = AUTO
#
#DISTCLEANFILES = $(kwin3_plastik_la_METASOURCES)
#
