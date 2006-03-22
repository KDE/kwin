kde4_header()

include_directories( ${KDE4_INCLUDE_DIR} ${QT_INCLUDES} ${CMAKE_CURRENT_SOURCE_DIR} ${CMAKE_CURRENT_BINARY_DIR}  )


########### next target ###############

set(kwin_modernsys_config_PART_SRCS config.cpp )

kde4_automoc(${kwin_modernsys_config_PART_SRCS})

kde4_add_plugin(kwin_modernsys_config ${kwin_modernsys_config_PART_SRCS})

kde4_install_libtool_file( ${PLUGIN_INSTALL_DIR} kwin_modernsys_config )

target_link_libraries(kwin_modernsys_config  ${KDE4_KDECORE_LIBS} kdeui ${QT_QTGUI_LIBRARY}) 

install_targets(${LIB_INSTALL_DIR}/kde4 kwin_modernsys_config )


########### install files ###############

install_files( ${DATA_INSTALL_DIR}/kwin/ FILES  )

kde4_footer()



#original Makefile.am contents follow:

#
#INCLUDES = $(all_includes)
#
#kde_module_LTLIBRARIES = kwin_modernsys_config.la
#
#kwin_modernsys_config_la_SOURCES = config.cpp
#kwin_modernsys_config_la_LDFLAGS = $(all_libraries) $(KDE_PLUGIN) -module
#kwin_modernsys_config_la_LIBADD = $(LIB_KDEUI)
#
#METASOURCES = AUTO
#noinst_HEADERS = config.h
#
#lnkdir = $(kde_datadir)/kwin/
#
