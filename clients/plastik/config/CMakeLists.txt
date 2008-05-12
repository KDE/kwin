set(kwin_plastik_config_PART_SRCS config.cpp )

kde4_add_ui_files(kwin_plastik_config_PART_SRCS configdialog.ui )

kde4_add_plugin(kwin_plastik_config ${kwin_plastik_config_PART_SRCS})

target_link_libraries(kwin_plastik_config ${KDE4_KDEUI_LIBS})

install(TARGETS kwin_plastik_config  DESTINATION ${PLUGIN_INSTALL_DIR} )
