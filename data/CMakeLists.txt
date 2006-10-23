


########### next target ###############
set(kwin_update_window_settings_SRCS update_window_settings.cpp)

kde4_automoc(${kwin_update_window_settings_SRCS})

kde4_add_executable( kwin_update_window_settings ${kwin_update_window_settings_SRCS})

target_link_libraries( kwin_update_window_settings ${KDE4_KDECORE_LIBS} )

install(TARGETS kwin_update_window_settings DESTINATION ${LIB_INSTALL_DIR}/kconf_update_bin/  )



########### next target ###############
set(kwin_update_default_rules_SRCS update_default_rules.cpp)

kde4_automoc(${kwin_update_default_rules})

kde4_add_executable( kwin_update_default_rules ${kwin_update_default_rules_SRCS})

target_link_libraries( kwin_update_default_rules ${KDE4_KDECORE_LIBS} )

install(TARGETS kwin_update_default_rules DESTINATION ${LIB_INSTALL_DIR}/kconf_update_bin/  )


########### install files ###############

install( FILES fsp_workarounds_1  DESTINATION  ${DATA_INSTALL_DIR}/kwin/default_rules )
install( FILES pop.wav  DESTINATION  ${SOUND_INSTALL_DIR} )
install( FILES kwin.upd kwinsticky.upd kwiniconify.upd kwin3_plugin.upd kwin_focus1.upd          kwinupdatewindowsettings.upd kwin_focus2.upd kwin_fsp_workarounds_1.upd  DESTINATION  ${KCONF_UPDATE_INSTALL_DIR} )
install(PROGRAMS pluginlibFix.pl kwin3_plugin.pl kwin_focus1.sh kwin_focus2.sh  DESTINATION  ${KCONF_UPDATE_INSTALL_DIR} )

