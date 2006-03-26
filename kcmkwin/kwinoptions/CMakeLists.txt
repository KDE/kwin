kde4_header()
include_directories( ${KDE4_INCLUDES} )


########### next target ###############

set(kcm_kwinoptions_PART_SRCS windows.cpp mouse.cpp main.cpp )

kde4_automoc(${kcm_kwinoptions_PART_SRCS})

kde4_add_plugin(kcm_kwinoptions ${kcm_kwinoptions_PART_SRCS})

kde4_install_libtool_file( ${PLUGIN_INSTALL_DIR} kcm_kwinoptions )

target_link_libraries(kcm_kwinoptions  ${KDE4_KDEUI_LIBS} ${QT_QTGUI_LIBRARY} DCOP ${QT_QT3SUPPORT_LIBRARY})

install_targets(${LIB_INSTALL_DIR}/kde4 kcm_kwinoptions )


########### install files ###############

install_files( ${XDG_APPS_DIR} FILES kwinoptions.desktop )
install_files( ${APPLNK_INSTALL_DIR}/.hidden FILES kwinactions.desktop  kwinadvanced.desktop  kwinfocus.desktop  kwinmoving.desktop  kwintranslucency.desktop  )

kde4_install_icons( ${ICON_INSTALL_DIR} crystalsvg )

kde4_footer()



#original Makefile.am contents follow:

#METASOURCES = AUTO
#INCLUDES = $(all_includes)
#
#kde_module_LTLIBRARIES = kcm_kwinoptions.la 
#
#kcm_kwinoptions_la_SOURCES = windows.cpp mouse.cpp main.cpp
#kcm_kwinoptions_la_LDFLAGS = -module -avoid-version $(all_libraries) -no-undefined
#kcm_kwinoptions_la_LIBADD = $(LIB_KDEUI)
#
#noinst_HEADERS = windows.h mouse.h 
#
#messages:
#	$(XGETTEXT) *.cpp -o $(podir)/kcmkwm.pot
#
#kwin_kcmkwin_options_data_DATA = kwinactions.desktop  kwinadvanced.desktop  kwinfocus.desktop  kwinmoving.desktop  kwintranslucency.desktop 
#kwin_kcmkwin_options_datadir = $(kde_appsdir)/.hidden
#
#xdg_apps_DATA = kwinoptions.desktop
#KDE_ICON=AUTO
