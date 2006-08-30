include_directories( ${CMAKE_SOURCE_DIR}/workspace/kwin/lib  )


########### next target ###############

set(kwin_b2_config_PART_SRCS config.cpp )

kde4_automoc(${kwin_b2_config_PART_SRCS})

kde4_add_plugin(kwin_b2_config ${kwin_b2_config_PART_SRCS})

kde4_install_libtool_file( ${PLUGIN_INSTALL_DIR} kwin_b2_config )

target_link_libraries(kwin_b2_config  ${KDE4_KDEUI_LIBS} ${QT_QT3SUPPORT_LIBRARY} ${QT_QTGUI_LIBRARY})

install(TARGETS kwin_b2_config  DESTINATION ${PLUGIN_INSTALL_DIR} )


########### install files ###############

install( FILES   DESTINATION  ${DATA_INSTALL_DIR}/kwin/ )
