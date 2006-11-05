include_directories( ${CMAKE_SOURCE_DIR}/workspace/kwin/lib  ${CMAKE_SOURCE_DIR}/workspace/kwin/lib  )

ADD_DEFINITIONS(-DKCMRULES)
########### next target ###############

set(kwinrules_SRCS ruleswidget.cpp ruleslist.cpp kwinsrc.cpp detectwidget.cpp)
kde4_add_ui_files(kwinrules_SRCS ruleslistbase.ui detectwidgetbase.ui)
kde4_add_ui3_files(kwinrules_SRCS ruleswidgetbase.ui editshortcutbase.ui )

set(kwin_rules_dialog_KDEINIT_SRCS main.cpp ${kwinrules_SRCS})

kde4_automoc(${kwin_rules_dialog_KDEINIT_SRCS})

kde4_add_kdeinit_executable( kwin_rules_dialog ${kwin_rules_dialog_KDEINIT_SRCS})

target_link_libraries(kdeinit_kwin_rules_dialog  ${KDE4_KDEUI_LIBS} )

install(TARGETS kdeinit_kwin_rules_dialog  DESTINATION ${LIB_INSTALL_DIR} )

target_link_libraries( kwin_rules_dialog kdeinit_kwin_rules_dialog )
install(TARGETS kwin_rules_dialog DESTINATION ${BIN_INSTALL_DIR})

########### next target ###############

set(kcm_kwinrules_PART_SRCS kcm.cpp ${kwinrules_SRCS})

kde4_automoc(${kcm_kwinrules_PART_SRCS})

kde4_add_plugin(kcm_kwinrules ${kcm_kwinrules_PART_SRCS})


target_link_libraries(kcm_kwinrules  ${KDE4_KDEUI_LIBS} )

install(TARGETS kcm_kwinrules  DESTINATION ${PLUGIN_INSTALL_DIR} )


########### install files ###############

install( FILES kwinrules.desktop  DESTINATION  ${SERVICES_INSTALL_DIR} )


