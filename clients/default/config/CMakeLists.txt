kde4_header()



########### next target ###############

set(kwin_default_config_PART_SRCS config.cpp )

kde4_automoc(${kwin_default_config_PART_SRCS})

kde4_add_plugin(kwin_default_config ${kwin_default_config_PART_SRCS})

kde4_install_libtool_file( ${PLUGIN_INSTALL_DIR} kwin_default_config )

target_link_libraries(kwin_default_config  ${KDE4_KDEUI_LIBS} ${QT_QTGUI_LIBRARY})

install_targets(${PLUGIN_INSTALL_DIR} kwin_default_config )


########### install files ###############

install_files( ${DATA_INSTALL_DIR}/kwin/ FILES  )

kde4_footer()



#original Makefile.am contents follow:

#INCLUDES = $(all_includes)
#
#kde_module_LTLIBRARIES = kwin_default_config.la
#
#kwin_default_config_la_SOURCES = config.cpp
#kwin_default_config_la_LDFLAGS = $(all_libraries) $(KDE_PLUGIN) -module
#kwin_default_config_la_LIBADD = $(LIB_KDEUI)
#
#METASOURCES = AUTO
#noinst_HEADERS = config.h
#
#lnkdir = $(kde_datadir)/kwin/
#
####KMAKE-start (don't edit or delete this block)
#
####KMAKE-end
