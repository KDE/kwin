include_directories( ${CMAKE_SOURCE_DIR}/workspace/kwin/lib  )


########### next target ###############

set(kcm_kwindecoration_PART_SRCS kwindecorationadaptor.cpp kwindecoration.cpp buttons.cpp preview.cpp )

kde4_automoc(${kcm_kwindecoration_PART_SRCS})


kde4_add_plugin(kcm_kwindecoration ${kcm_kwindecoration_PART_SRCS})

kde4_install_libtool_file( ${PLUGIN_INSTALL_DIR} kcm_kwindecoration )

target_link_libraries(kcm_kwindecoration  ${KDE4_KDEUI_LIBS} kdecorations ${QT_QT3SUPPORT_LIBRARY} ${X11_LIBRARIES})

install_targets(${PLUGIN_INSTALL_DIR} kcm_kwindecoration )


########### install files ###############

install_files( ${XDG_APPS_DIR} FILES kwindecoration.desktop )




#original Makefile.am contents follow:

#INCLUDES = -I$(srcdir)/../../lib $(all_includes)
#
#kde_module_LTLIBRARIES = kcm_kwindecoration.la
#
#kcm_kwindecoration_la_SOURCES = kwindecoration.cpp buttons.cpp kwindecorationIface.skel preview.cpp
#noinst_HEADERS = kwindecoration.h kwindecorationIface.h buttons.h preview.h
#
#kcm_kwindecoration_la_LDFLAGS = \
#  -module -avoid-version $(all_libraries) -no-undefined
#
#kcm_kwindecoration_la_LIBADD = $(LIB_KDEUI) ../../lib/libkdecorations.la
#
#METASOURCES = AUTO
#
#messages:
#	$(XGETTEXT) *.cpp -o $(podir)/kcmkwindecoration.pot
#
#xdg_apps_DATA = kwindecoration.desktop
