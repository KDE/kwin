
set(kwin_xml  ${CMAKE_CURRENT_SOURCE_DIR}/../org.kde.KWin.xml)


########### next target ###############
set(kwin_update_window_settings_SRCS update_window_settings.cpp )



kde4_add_executable( kwin_update_window_settings ${kwin_update_window_settings_SRCS})

target_link_libraries( kwin_update_window_settings ${KDE4_KDECORE_LIBS})

install(TARGETS kwin_update_window_settings DESTINATION ${LIB_INSTALL_DIR}/kconf_update_bin/  )


########### next target ###############
set(kwin_update_default_rules_SRCS update_default_rules.cpp)


kde4_add_executable( kwin_update_default_rules ${kwin_update_default_rules_SRCS})

target_link_libraries( kwin_update_default_rules ${KDE4_KDECORE_LIBS} )

install(TARGETS kwin_update_default_rules DESTINATION ${LIB_INSTALL_DIR}/kconf_update_bin/  )

########### next target ###############
set( kwin_update_tabbox_settings_SRCS update_tabbox_settings.cpp )

kde4_add_executable( kwin_update_tabbox_settings ${kwin_update_tabbox_settings_SRCS} )

target_link_libraries( kwin_update_tabbox_settings ${KDE4_KDECORE_LIBS} )

install( TARGETS kwin_update_tabbox_settings DESTINATION ${LIB_INSTALL_DIR}/kconf_update_bin/ )
########### next target ###############
set( kwin_update_tabbox_qml_settings_SRCS update_tabbox_qml_settings.cpp )

kde4_add_executable( kwin_update_tabbox_qml_settings ${kwin_update_tabbox_qml_settings_SRCS} )

target_link_libraries( kwin_update_tabbox_qml_settings ${KDE4_KDECORE_LIBS} )

install( TARGETS kwin_update_tabbox_qml_settings DESTINATION ${LIB_INSTALL_DIR}/kconf_update_bin/ )

########### install files ###############

install( FILES fsp_workarounds_1.kwinrules  DESTINATION  ${DATA_INSTALL_DIR}/kwin/default_rules )
install( FILES pop.wav  DESTINATION  ${SOUND_INSTALL_DIR} )
install( FILES kwin.upd kwinsticky.upd kwiniconify.upd kwin3_plugin.upd kwin_focus1.upd kwinupdatewindowsettings.upd kwin_focus2.upd kwin_fsp_workarounds_1.upd kwin_on_off.upd kwin_window_shortcuts.upd kwin_update_tabbox_settings.upd kwin_remove_effects.upd kwin_update_tabbox_qml_settings.upd kwin_remove_delay_focus.upd DESTINATION  ${KCONF_UPDATE_INSTALL_DIR} )
install( PROGRAMS pluginlibFix.pl kwin3_plugin.pl kwin_focus1.sh kwin_focus2.sh on-off_to_true-false.sh  kwin_window_shortcuts.sh kwin_remove_delay_focus.sh DESTINATION  ${KCONF_UPDATE_INSTALL_DIR} )

