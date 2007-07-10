


########### next target ###############

set(kwin_quartz_config_PART_SRCS config.cpp )


kde4_add_plugin(kwin_quartz_config ${kwin_quartz_config_PART_SRCS})



target_link_libraries(kwin_quartz_config  ${KDE4_KDEUI_LIBS} ${QT_QTGUI_LIBRARY})

install(TARGETS kwin_quartz_config  DESTINATION ${PLUGIN_INSTALL_DIR} )

