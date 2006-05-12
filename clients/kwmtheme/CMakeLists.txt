kde4_header()

add_subdirectory( cli_installer ) 




########### next target ###############

set(kwin3_kwmtheme_PART_SRCS kwmthemeclient.cpp )

kde4_automoc(${kwin3_kwmtheme_PART_SRCS})

kde4_add_plugin(kwin3_kwmtheme ${kwin3_kwmtheme_PART_SRCS})

kde4_install_libtool_file( ${PLUGIN_INSTALL_DIR} kwin3_kwmtheme )

target_link_libraries(kwin3_kwmtheme  ${KDE4_KDECORE_LIBS} kdecorations )

install_targets(${PLUGIN_INSTALL_DIR} kwin3_kwmtheme )


########### install files ###############

install_files( ${DATA_INSTALL_DIR}/kwin FILES kwmtheme.desktop )

kde4_footer()



#original Makefile.am contents follow:

#SUBDIRS=cli_installer
#INCLUDES = $(all_includes) 
#
#kde_module_LTLIBRARIES = kwin3_kwmtheme.la
#
#kwin3_kwmtheme_la_SOURCES = kwmthemeclient.cpp
#kwin3_kwmtheme_la_LDFLAGS = $(all_libraries) $(KDE_PLUGIN) -module
#kwin3_kwmtheme_la_LIBADD  = -lkdecorations
#
#METASOURCES = AUTO
#noinst_HEADERS = kwmthemeclient.h
#
#lnkdir = $(kde_datadir)/kwin
#lnk_DATA = kwmtheme.desktop
#
