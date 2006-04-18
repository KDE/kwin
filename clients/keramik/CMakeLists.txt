kde4_header()

add_subdirectory( config ) 

include_directories( ${CMAKE_SOURCE_DIR}/workspace/kwin/lib ${KDE4_INCLUDES} )


########### next target ###############

set(kwin3_keramik_PART_SRCS keramik.cpp )

kde4_automoc(${kwin3_keramik_PART_SRCS})

qt4_add_resources(kwin3_keramik_PART_SRCS tiles.qrc )

kde4_add_plugin(kwin3_keramik ${kwin3_keramik_PART_SRCS})

kde4_install_libtool_file( ${PLUGIN_INSTALL_DIR} kwin3_keramik )

target_link_libraries(kwin3_keramik  ${KDE4_KDEUI_LIBS} kdecorations ${QT_QT3SUPPORT_LIBRARY} ${QT_QTGUI_LIBRARY})

install_targets(${PLUGIN_INSTALL_DIR} kwin3_keramik )


########### install files ###############

install_files( ${DATA_INSTALL_DIR}/kwin FILES keramik.desktop )

kde4_footer()



#original Makefile.am contents follow:

#INCLUDES                  = -I$(srcdir)/../../lib $(all_includes)
#
#SUBDIRS                   = . config
#
#kde_module_LTLIBRARIES    = kwin3_keramik.la
#
#kwin3_keramik_la_SOURCES   = keramik.cpp tiles.qrc
#kwin3_keramik_la_LDFLAGS   = $(all_libraries) $(KDE_PLUGIN) -module
#kwin3_keramik_la_LIBADD    = $(LIB_KDEUI) ../../lib/libkdecorations.la
##kwin3_keramik_la_LDFLAGS   = $(all_libraries) -avoid-version -module $(KDE_RPATH) $(KDE_MT_LDFLAGS)
#
#METASOURCES               = AUTO
#noinst_headers        	  = keramik.h
#
#lnkdir                    = $(kde_datadir)/kwin
#lnk_DATA                  = keramik.desktop
#
#EXTRA_DIST                = $(lnk_DATA)
#
#
