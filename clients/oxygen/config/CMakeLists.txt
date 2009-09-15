include_directories( ${KDEBASE_WORKSPACE_SOURCE_DIR}/kwin/lib  )


set(kwin_oxygen_config_PART_SRCS
    config.cpp
    itemmodel.cpp
    ../nitrogenconfiguration.cpp
    ../nitrogenshadowconfiguration.cpp
    ../nitrogenexception.cpp
    ../nitrogenexceptionlist.cpp
    nitrogenconfigurationui.cpp
    nitrogendetectwidget.cpp
    nitrogenexceptiondialog.cpp
    nitrogenexceptionlistwidget.cpp
    nitrogenexceptionmodel.cpp
    nitrogenshadowconfigurationui.cpp
  )

kde4_add_plugin(kwin_oxygen_config ${kwin_oxygen_config_PART_SRCS})

target_link_libraries(
    kwin_oxygen_config
    ${KDE4_KDEUI_LIBS}
    ${QT_QTGUI_LIBRARY}
    ${X11_X11_LIB}
    kdecorations
  )

install(TARGETS kwin_oxygen_config  DESTINATION ${PLUGIN_INSTALL_DIR} )
