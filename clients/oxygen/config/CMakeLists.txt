include_directories( ${KDEBASE_WORKSPACE_SOURCE_DIR}/kwin/lib  )


########### next target ###############

set(kwin_oxygen_config_PART_SRCS config.cpp )


kde4_add_ui_files(kwin_oxygen_config_PART_SRCS oxygenconfig.ui )

kde4_add_plugin(kwin_oxygen_config ${kwin_oxygen_config_PART_SRCS})



target_link_libraries(kwin_oxygen_config  ${KDE4_KDEUI_LIBS} ${QT_QTGUI_LIBRARY})

install(TARGETS kwin_oxygen_config  DESTINATION ${PLUGIN_INSTALL_DIR} )
