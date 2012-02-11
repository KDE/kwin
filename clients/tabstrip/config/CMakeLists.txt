
set(kwin_tabstrip_config_SRCS tabstripconfig.cpp )

kde4_add_ui_files(kwin_tabstrip_config_SRCS tabstripconfig.ui )

kde4_add_plugin(kwin_tabstrip_config ${kwin_tabstrip_config_SRCS})

target_link_libraries(kwin_tabstrip_config  ${KDE4_KDEUI_LIBS} ${QT_QTGUI_LIBRARY})

install(TARGETS kwin_tabstrip_config  DESTINATION ${PLUGIN_INSTALL_DIR} )
