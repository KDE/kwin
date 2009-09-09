include_directories( ${KDEBASE_WORKSPACE_SOURCE_DIR}/kwin/lib  )


set(kwin_nitrogen_config_PART_SRCS
    config.cpp
    itemmodel.cpp
    ../nitrogenconfiguration.cpp
    ../nitrogenexception.cpp
    ../nitrogenexceptionlist.cpp
    nitrogenconfigurationui.cpp
    nitrogenexceptiondialog.cpp
    nitrogenexceptionlistwidget.cpp
    nitrogenexceptionmodel.cpp
  )

kde4_add_plugin(kwin_nitrogen_config ${kwin_nitrogen_config_PART_SRCS})

target_link_libraries(
    kwin_nitrogen_config
    ${KDE4_KDEUI_LIBS}
    ${QT_QTGUI_LIBRARY}
  )

install(TARGETS kwin_nitrogen_config  DESTINATION ${PLUGIN_INSTALL_DIR} )

