


########### next target ###############

set(kwin_plastik_config_PART_SRCS config.cpp )

kde4_automoc(${kwin_plastik_config_PART_SRCS})

kde4_add_ui3_files(kwin_plastik_config_PART_SRCS configdialog.ui )

kde4_add_plugin(kwin_plastik_config ${kwin_plastik_config_PART_SRCS})

kde4_install_libtool_file( ${PLUGIN_INSTALL_DIR} kwin_plastik_config )

target_link_libraries(kwin_plastik_config  ${KDE4_KDEUI_LIBS} ${QT_QTGUI_LIBRARY} ${QT_QT3SUPPORT_LIBRARY})

install(TARGETS kwin_plastik_config  DESTINATION ${PLUGIN_INSTALL_DIR} )
