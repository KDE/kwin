kde4_header()




########### next target ###############

set(kwin3_test_PART_SRCS test.cpp )

kde4_automoc(${kwin3_test_PART_SRCS})

kde4_add_plugin(kwin3_test ${kwin3_test_PART_SRCS})

kde4_install_libtool_file( ${PLUGIN_INSTALL_DIR} kwin3_test )

target_link_libraries(kwin3_test  ${KDE4_KDEUI_LIBS} kdecorations )

install_targets(${PLUGIN_INSTALL_DIR} kwin3_test )


########### install files ###############

install_files( ${DATA_INSTALL_DIR}/kwin FILES test.desktop )

kde4_footer()



#original Makefile.am contents follow:

#INCLUDES                  = -I$(srcdir)/../../lib $(all_includes)
#
#kde_module_LTLIBRARIES    = kwin3_test.la
#
#kwin3_test_la_SOURCES   = test.cpp
#kwin3_test_la_LDFLAGS   = $(all_libraries) $(KDE_PLUGIN) -module
#kwin3_test_la_LIBADD    = $(LIB_KDEUI) ../../lib/libkdecorations.la
##kwin_keramik_la_LDFLAGS   = $(all_libraries) -avoid-version -module $(KDE_RPATH) $(KDE_MT_LDFLAGS)
#
#METASOURCES               = AUTO
#
#kwin_test_lnkdir                    = $(kde_datadir)/kwin
#kwin_test_lnk_DATA                  = test.desktop
#
#EXTRA_DIST                = $(kwin_test_lnk_DATA)
