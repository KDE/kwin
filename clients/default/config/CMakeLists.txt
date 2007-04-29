


########### next target ###############

set(kwin_default_config_PART_SRCS config.cpp )

kde4_automoc(kwin_default_config ${kwin_default_config_PART_SRCS})

kde4_add_plugin(kwin_default_config ${kwin_default_config_PART_SRCS})



target_link_libraries(kwin_default_config  ${KDE4_KDEUI_LIBS} ${QT_QTGUI_LIBRARY})

install(TARGETS kwin_default_config  DESTINATION ${PLUGIN_INSTALL_DIR} )

