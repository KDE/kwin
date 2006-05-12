kde4_header()
include_directories( ${CMAKE_SOURCE_DIR}/workspace/kwin/lib  )


########### next target ###############

set(kwin_b2_config_PART_SRCS config.cpp )

kde4_automoc(${kwin_b2_config_PART_SRCS})

kde4_add_plugin(kwin_b2_config ${kwin_b2_config_PART_SRCS})

kde4_install_libtool_file( ${PLUGIN_INSTALL_DIR} kwin_b2_config )

target_link_libraries(kwin_b2_config  ${KDE4_KDEUI_LIBS} ${QT_QT3SUPPORT_LIBRARY} ${QT_QTGUI_LIBRARY})

install_targets(${PLUGIN_INSTALL_DIR} kwin_b2_config )


########### install files ###############

install_files( ${DATA_INSTALL_DIR}/kwin/ FILES  )

kde4_footer()



#original Makefile.am contents follow:

#INCLUDES = $(all_includes)
#
#kde_module_LTLIBRARIES = kwin_b2_config.la
#
#kwin_b2_config_la_SOURCES = config.cpp
#kwin_b2_config_la_LDFLAGS = $(all_libraries) $(KDE_PLUGIN) -module
#kwin_b2_config_la_LIBADD = $(LIB_KDEUI)
#
#METASOURCES = AUTO
#noinst_HEADERS = config.h
#
#lnkdir = $(kde_datadir)/kwin/
#
####KMAKE-start (don't edit or delete this block)
#
####KMAKE-end
