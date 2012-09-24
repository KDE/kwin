########### next target ###############

set(kcm_kwinoptions_PART_SRCS windows.cpp mouse.cpp main.cpp )
kde4_add_ui_files(kcm_kwinoptions_PART_SRCS actions.ui advanced.ui focus.ui mouse.ui moving.ui)
kde4_add_plugin(kcm_kwinoptions ${kcm_kwinoptions_PART_SRCS})
target_link_libraries(kcm_kwinoptions  ${KDE4_KDEUI_LIBS})
install(TARGETS kcm_kwinoptions  DESTINATION ${PLUGIN_INSTALL_DIR} )


########### install files ###############

install( FILES kwinoptions.desktop kwinactions.desktop kwinadvanced.desktop
    kwinfocus.desktop kwinmoving.desktop
    DESTINATION ${SERVICES_INSTALL_DIR})

kde4_install_icons( ${ICON_INSTALL_DIR} )
