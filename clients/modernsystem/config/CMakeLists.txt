


########### next target ###############

set(kwin_modernsys_config_PART_SRCS config.cpp )


kde4_add_plugin(kwin_modernsys_config ${kwin_modernsys_config_PART_SRCS})



target_link_libraries(kwin_modernsys_config  ${KDE4_KDEUI_LIBS} ${QT_QTGUI_LIBRARY}) 

install(TARGETS kwin_modernsys_config  DESTINATION ${PLUGIN_INSTALL_DIR} )

