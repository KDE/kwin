


########### next target ###############

set(kwin_kde2_config_PART_SRCS config.cpp )


kde4_add_plugin(kwin_kde2_config ${kwin_kde2_config_PART_SRCS})



target_link_libraries(kwin_kde2_config  ${KDE4_KDEUI_LIBS} ${QT_QTGUI_LIBRARY})

install(TARGETS kwin_kde2_config  DESTINATION ${PLUGIN_INSTALL_DIR} )

