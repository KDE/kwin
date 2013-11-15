########### next target ###############

set(kwin_b2_config_PART_SRCS config.cpp )


kde4_add_plugin(kwin_b2_config ${kwin_b2_config_PART_SRCS})



target_link_libraries(kwin_b2_config  ${KDE4_KDEUI_LIBS} Qt5::Gui)

install(TARGETS kwin_b2_config  DESTINATION ${PLUGIN_INSTALL_DIR} )


