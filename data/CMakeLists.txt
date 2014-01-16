########### next target ###############
set(kwin_update_default_rules_SRCS update_default_rules.cpp)


add_executable( kwin_update_default_rules ${kwin_update_default_rules_SRCS})

target_link_libraries( kwin_update_default_rules  )

install(TARGETS kwin_update_default_rules DESTINATION ${LIB_INSTALL_DIR}/kconf_update_bin/  )

########### install files ###############

install( FILES fsp_workarounds_1.kwinrules  DESTINATION  ${DATA_INSTALL_DIR}/kwin/default_rules )
install( FILES pop.wav  DESTINATION  ${SOUND_INSTALL_DIR} )
install( FILES kwin_fsp_workarounds_1.upd
         DESTINATION  ${KCONF_UPDATE_INSTALL_DIR} )
install( FILES stripTitle.js  DESTINATION  ${DATA_INSTALL_DIR}/kwin )

