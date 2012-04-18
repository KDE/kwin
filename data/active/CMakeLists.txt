########### next target ###############
set( kwin_update_tabbox_qml_settings_SRCS update_tabbox_qml_settings_active.cpp )

kde4_add_executable( kwin_update_tabbox_qml_settings ${kwin_update_tabbox_qml_settings_SRCS} )

target_link_libraries( kwin_update_tabbox_qml_settings_active ${KDE4_KDECORE_LIBS} )

install( TARGETS kwin_update_tabbox_qml_settings_active DESTINATION ${LIB_INSTALL_DIR}/kconf_update_bin/ )

########### install files ###############

install( FILES kwin_update_tabbox_qml_settings_active.upd DESTINATION  ${KCONF_UPDATE_INSTALL_DIR} )
