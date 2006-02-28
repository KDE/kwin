kde4_header()

add_subdirectory( config ) 

include_directories( ${KDE4_INCLUDE_DIR} ${QT_INCLUDES} ${CMAKE_CURRENT_SOURCE_DIR} ${CMAKE_CURRENT_BINARY_DIR}  )


########### next target ###############

set(kwin3_quartz_PART_SRCS quartz.cpp )

kde4_automoc(${kwin3_quartz_PART_SRCS})

kde4_add_plugin(kwin3_quartz ${kwin3_quartz_PART_SRCS})

kde4_install_libtool_file( ${PLUGIN_INSTALL_DIR} kwin3_quartz )

target_link_libraries(kwin3_quartz  ${QT_AND_KDECORE_LIBS} kdeui kdecorations )

install_targets(${LIB_INSTALL_DIR}/kde4 kwin3_quartz )


########### install files ###############

install_files( ${DATA_INSTALL_DIR}/kwin/ FILES quartz.desktop )

kde4_footer()



#original Makefile.am contents follow:

#
#INCLUDES = -I$(srcdir)/../../lib $(all_includes)
#
#SUBDIRS = . config
#
#kde_module_LTLIBRARIES = kwin3_quartz.la
#
#kwin3_quartz_la_SOURCES = quartz.cpp
#kwin3_quartz_la_LDFLAGS = $(all_libraries) $(KDE_PLUGIN) -module
#kwin3_quartz_la_LIBADD  = $(LIB_KDEUI)  ../../lib/libkdecorations.la
#
#METASOURCES = AUTO
#noinst_HEADERS = quartz.h
#
#lnkdir = $(kde_datadir)/kwin/
#lnk_DATA = quartz.desktop
#
#EXTRA_DIST = $(lnk_DATA)
#
#
####KMAKE-start (don't edit or delete this block)
#
####KMAKE-end
