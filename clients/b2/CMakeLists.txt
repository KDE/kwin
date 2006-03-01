kde4_header()

add_subdirectory( config ) 

include_directories( ${KDE4_INCLUDE_DIR} ${QT_INCLUDES} ${CMAKE_CURRENT_SOURCE_DIR} ${CMAKE_CURRENT_BINARY_DIR}   ${CMAKE_SOURCE_DIR}/workspace/kwin/lib )


########### next target ###############

set(kwin3_b2_PART_SRCS b2client.cpp )

kde4_automoc(${kwin3_b2_PART_SRCS})

kde4_add_plugin(kwin3_b2 ${kwin3_b2_PART_SRCS})

kde4_install_libtool_file( ${PLUGIN_INSTALL_DIR} kwin3_b2 )

target_link_libraries(kwin3_b2  ${QT_AND_KDECORE_LIBS} kdecorations kdefx ${QT_QT3SUPPORT_LIBRARY} ${X11_LIBRARIES} ${QT_QTGUI_LIBRARY})

install_targets(${LIB_INSTALL_DIR}/kde4 kwin3_b2 )


########### install files ###############

install_files( ${DATA_INSTALL_DIR}/kwin/ FILES b2.desktop )

kde4_footer()



#original Makefile.am contents follow:

#
#INCLUDES = -I$(srcdir)/../../lib $(all_includes) 
#
#SUBDIRS = . config
#
#kde_module_LTLIBRARIES = kwin3_b2.la
#
#kwin3_b2_la_SOURCES = b2client.cpp
#kwin3_b2_la_LDFLAGS = $(all_libraries) $(KDE_PLUGIN) -module
## kwin_b2_la_LDFLAGS = $(all_libraries) -avoid-version -module $(KDE_RPATH) $(KDE_MT_LDFLAGS)
#kwin3_b2_la_LIBADD  = ../../lib/libkdecorations.la -lkdefx
#
#METASOURCES = AUTO
#noinst_HEADERS = b2client.h
#
#lnkdir = $(kde_datadir)/kwin/
#lnk_DATA = b2.desktop
#
#EXTRA_DIST = $(lnk_DATA)
#
####KMAKE-start (don't edit or delete this block)
#
####KMAKE-end
