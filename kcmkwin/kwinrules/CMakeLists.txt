ADD_DEFINITIONS(-DKCMRULES)
########### next target ###############

set (kwinrules_MOC_HDRS yesnobox.h ../../client_machine.h)
qt4_wrap_cpp(kwinrules_MOC_SRCS ${kwinrules_MOC_HDRS})
set(kwinrules_SRCS ruleswidget.cpp ruleslist.cpp kwinsrc.cpp detectwidget.cpp ${kwinrules_MOC_SRCS})

kde4_add_ui_files(kwinrules_SRCS ruleslist.ui detectwidget.ui editshortcut.ui ruleswidgetbase.ui)

set(kwin_rules_dialog_KDEINIT_SRCS main.cpp ${kwinrules_SRCS})

kde4_add_kdeinit_executable( kwin_rules_dialog ${kwin_rules_dialog_KDEINIT_SRCS})

set(kwin_kcm_rules_XCB_LIBS
    ${XCB_XCB_LIBRARIES}
    ${X11_XCB_LIBRARIES}
)

target_link_libraries(kdeinit_kwin_rules_dialog ${KDE4_KDEUI_LIBS} ${KDE4_KIO_LIBS} ${X11_LIBRARIES} ${KACTIVITIES_LIBRARY} ${kwin_kcm_rules_XCB_LIBS})

install(TARGETS kdeinit_kwin_rules_dialog ${INSTALL_TARGETS_DEFAULT_ARGS} )
install(TARGETS kwin_rules_dialog         DESTINATION ${LIBEXEC_INSTALL_DIR} )

########### next target ###############

set(kcm_kwinrules_PART_SRCS kcm.cpp ${kwinrules_SRCS})


kde4_add_plugin(kcm_kwinrules ${kcm_kwinrules_PART_SRCS})

target_link_libraries(kcm_kwinrules ${KDE4_KDEUI_LIBS} ${KDE4_KIO_LIBS} ${X11_LIBRARIES} ${KACTIVITIES_LIBRARY} ${kwin_kcm_rules_XCB_LIBS})

install(TARGETS kcm_kwinrules  DESTINATION ${PLUGIN_INSTALL_DIR} )


########### next target ###############


########### install files ###############

install( FILES kwinrules.desktop  DESTINATION  ${SERVICES_INSTALL_DIR} )


