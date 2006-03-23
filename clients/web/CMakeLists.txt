kde4_header()

include_directories( ${KDE4_INCLUDE_DIR} ${QT_INCLUDES} )


########### next target ###############

set(kwin3_web_PART_SRCS Web.cpp WebButton.cpp )

kde4_automoc(${kwin3_web_PART_SRCS})

kde4_add_plugin(kwin3_web ${kwin3_web_PART_SRCS})

kde4_install_libtool_file( ${PLUGIN_INSTALL_DIR} kwin3_web )

target_link_libraries(kwin3_web  ${KDE4_KDECORE_LIBS} kdeui kdecorations )

install_targets(${LIB_INSTALL_DIR}/kde4 kwin3_web )


########### install files ###############

install_files( ${DATA_INSTALL_DIR}/kwin/ FILES web.desktop )

kde4_footer()



#original Makefile.am contents follow:

#INCLUDES                  = -I$(top_srcdir) $(all_includes)
#kde_module_LTLIBRARIES    = kwin3_web.la
#
#kwin3_web_la_SOURCES      = Web.cpp WebButton.cpp
#
#noinst_HEADERS            = Web.h WebButton.h
#
#kwin3_web_la_LDFLAGS      = $(all_libraries) $(KDE_PLUGIN) -module
#kwin3_web_la_LIBADD       = $(LIB_KDEUI) ../../lib/libkdecorations.la
#METASOURCES               = AUTO
#
#linkdir                   = $(kde_datadir)/kwin/
#link_DATA                 = web.desktop
#EXTRA_DIST                = $(link_DATA)
#
