include_directories( ${KDEBASE_WORKSPACE_SOURCE_DIR}/kwin/lib  )


########### next target ###############

set(kwin_keramik_config_PART_SRCS config.cpp )


kde4_add_ui_files(kwin_keramik_config_PART_SRCS keramikconfig.ui )

kde4_add_plugin(kwin_keramik_config ${kwin_keramik_config_PART_SRCS})



target_link_libraries(kwin_keramik_config  ${KDE4_KDEUI_LIBS} ${QT_QTGUI_LIBRARY})

install(TARGETS kwin_keramik_config  DESTINATION ${PLUGIN_INSTALL_DIR} )

