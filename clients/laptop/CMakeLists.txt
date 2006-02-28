kde4_header()

include_directories( ${KDE4_INCLUDE_DIR} ${QT_INCLUDES} ${CMAKE_CURRENT_SOURCE_DIR} ${CMAKE_CURRENT_BINARY_DIR}  )


########### next target ###############

set(kwin3_laptop_PART_SRCS laptopclient.cpp )

kde4_automoc(${kwin3_laptop_PART_SRCS})

kde4_add_plugin(kwin3_laptop ${kwin3_laptop_PART_SRCS})

kde4_install_libtool_file( ${PLUGIN_INSTALL_DIR} kwin3_laptop )

target_link_libraries(kwin3_laptop  ${QT_AND_KDECORE_LIBS} kdecorations kdefx )

install_targets(${LIB_INSTALL_DIR}/kde4 kwin3_laptop )


########### install files ###############

install_files( ${DATA_INSTALL_DIR}/kwin FILES laptop.desktop )

kde4_footer()



#original Makefile.am contents follow:

#
#INCLUDES = -I$(srcdir)/../../lib $(all_includes)
#
#kde_module_LTLIBRARIES = kwin3_laptop.la
#
#kwin3_laptop_la_SOURCES = laptopclient.cpp
#kwin3_laptop_la_LDFLAGS = $(all_libraries) $(KDE_PLUGIN) -module 
#kwin3_laptop_la_LIBADD  = ../../lib/libkdecorations.la -lkdefx
#
#METASOURCES = AUTO
#noinst_HEADERS = laptopclient.h
#
#lnkdir = $(kde_datadir)/kwin
#lnk_DATA = laptop.desktop
#
#EXTRA_DIST = $(lnk_DATA)
#
