########### decoration ###############

set(kwin3_aurorae_PART_SRCS aurorae.cpp themeconfig.cpp)

kde4_add_plugin(kwin3_aurorae ${kwin3_aurorae_PART_SRCS})

target_link_libraries(kwin3_aurorae ${KDE4_KDEUI_LIBS} ${KDE4_PLASMA_LIBS} kdecorations)

install(TARGETS kwin3_aurorae  DESTINATION ${PLUGIN_INSTALL_DIR} )

########### install files ###############

install( FILES aurorae.desktop  DESTINATION  ${DATA_INSTALL_DIR}/kwin )

########### config ###############
set(kwin_aurorae_config_PART_SRCS config/config.cpp themeconfig.cpp )

kde4_add_ui_files(kwin_aurorae_config_PART_SRCS config/config.ui)

kde4_add_plugin(kwin_aurorae_config ${kwin_aurorae_config_PART_SRCS})

target_link_libraries(kwin_aurorae_config  ${KDE4_KDEUI_LIBS} ${KDE4_KIO_LIBS} ${KDE4_PLASMA_LIBS} ${QT_QTGUI_LIBRARY})

install(TARGETS kwin_aurorae_config  DESTINATION ${PLUGIN_INSTALL_DIR} )
