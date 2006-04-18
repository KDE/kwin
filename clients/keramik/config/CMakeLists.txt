kde4_header()
include_directories( ${CMAKE_SOURCE_DIR}/workspace/kwin/lib ${KDE4_INCLUDES} )


########### next target ###############

set(kwin_keramik_config_PART_SRCS config.cpp )

kde4_automoc(${kwin_keramik_config_PART_SRCS})

kde4_add_ui3_files(kwin_keramik_config_PART_SRCS keramikconfig.ui )

kde4_add_plugin(kwin_keramik_config ${kwin_keramik_config_PART_SRCS})

kde4_install_libtool_file( ${PLUGIN_INSTALL_DIR} kwin_keramik_config )

target_link_libraries(kwin_keramik_config  ${KDE4_KDEUI_LIBS} ${QT_QTGUI_LIBRARY})

install_targets(${PLUGIN_INSTALL_DIR} kwin_keramik_config )


########### install files ###############

install_files( ${DATA_INSTALL_DIR}/kwin FILES  )

kde4_footer()



#original Makefile.am contents follow:

#INCLUDES                       = $(all_includes)
#
#kde_module_LTLIBRARIES         = kwin_keramik_config.la
#
#kwin_keramik_config_la_SOURCES = config.cpp keramikconfig.ui
#kwin_keramik_config_la_LDFLAGS = $(all_libraries) $(KDE_PLUGIN) -module
#kwin_keramik_config_la_LIBADD  = $(LIB_KDEUI)
#
#METASOURCES                    = AUTO
#noinst_HEADERS                 = config.h keramikconfig.h
#
#lnkdir                         = $(kde_datadir)/kwin
#
