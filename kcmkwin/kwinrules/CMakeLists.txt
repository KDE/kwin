kde4_header()
include_directories( ${CMAKE_SOURCE_DIR}/workspace/kwin/lib  ${CMAKE_SOURCE_DIR}/workspace/kwin/lib ${KDE4_INCLUDE_DIR} ${QT_INCLUDES} )

ADD_DEFINITIONS(-DKCMRULES)
########### next target ###############

set(kwinrules_SRCS ruleswidget.cpp ruleslist.cpp kwinsrc.cpp detectwidget.cpp)
kde4_add_ui3_files(kwinrules_SRCS ruleswidgetbase.ui ruleslistbase.ui detectwidgetbase.ui editshortcutbase.ui )

set(kwin_rules_dialog_KDEINIT_SRCS main.cpp ${kwinrules_SRCS})

kde4_automoc(${kwin_rules_dialog_KDEINIT_SRCS})

kde4_add_kdeinit_executable( kwin_rules_dialog ${kwin_rules_dialog_KDEINIT_SRCS})

target_link_libraries(kdeinit_kwin_rules_dialog  ${KDE4_KDEUI_LIBS} )

install_targets(${LIB_INSTALL_DIR} kdeinit_kwin_rules_dialog )

target_link_libraries( kwin_rules_dialog kdeinit_kwin_rules_dialog )
install_targets(/bin kwin_rules_dialog )

########### next target ###############

set(kcm_kwinrules_PART_SRCS kcm.cpp ${kwinrules_SRCS})

kde4_automoc(${kcm_kwinrules_PART_SRCS})

kde4_add_plugin(kcm_kwinrules ${kcm_kwinrules_PART_SRCS})

kde4_install_libtool_file( ${PLUGIN_INSTALL_DIR} kcm_kwinrules )

target_link_libraries(kcm_kwinrules  ${KDE4_KDEUI_LIBS} )

install_targets(${LIB_INSTALL_DIR}/kde4 kcm_kwinrules )


########### next target ###############


########### install files ###############

install_files( ${XDG_APPS_DIR} FILES kwinrules.desktop )

kde4_footer()



#original Makefile.am contents follow:

#METASOURCES = AUTO
#AM_CPPFLAGS = $(all_includes) -DKCMRULES
#INCLUDES = -I$(top_srcdir)/workspace/kwin/lib
#
#bin_PROGRAMS =
#lib_LTLIBRARIES =
#kdeinit_LTLIBRARIES = kwin_rules_dialog.la
#kde_module_LTLIBRARIES = kcm_kwinrules.la 
#noinst_LTLIBRARIES = libkwinrules.la
#
#libkwinrules_la_SOURCES = ruleswidget.cpp ruleslist.cpp kwinsrc.cpp detectwidget.cpp \
#        ruleswidgetbase.ui ruleslistbase.ui detectwidgetbase.ui editshortcutbase.ui
#libkwinrules_la_LDFLAGS = $(all_libraries) -no-undefined
#libkwinrules_la_LIBADD = $(LIB_KDEUI)
#
#kcm_kwinrules_la_SOURCES = kcm.cpp 
#kcm_kwinrules_la_LDFLAGS = -module -avoid-version $(all_libraries) -no-undefined
#kcm_kwinrules_la_LIBADD = libkwinrules.la 
#kcm_kwinrules_la_COMPILE_FIRST = ruleslistbase.h ruleswidgetbase.h 
#
#kwin_rules_dialog_la_SOURCES = main.cpp 
#kwin_rules_dialog_la_LDFLAGS = -module -avoid-version $(all_libraries) -no-undefined
#kwin_rules_dialog_la_LIBADD = libkwinrules.la 
#kwin_rules_dialog_la_COMPILE_FIRST = ruleslistbase.h ruleswidgetbase.h
#
#messages: rc.cpp
#	$(XGETTEXT) *.cpp -o $(podir)/kcmkwinrules.pot
#
#xdg_apps_DATA = kwinrules.desktop
