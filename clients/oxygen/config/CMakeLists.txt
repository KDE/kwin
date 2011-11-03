include_directories( ${KDEBASE_WORKSPACE_SOURCE_DIR}/libs/oxygen)

set(kwin_oxygen_config_PART_SRCS
    ../oxygenconfiguration.cpp
    ../oxygenexception.cpp
    ../oxygenexceptionlist.cpp
    oxygenanimationconfigwidget.cpp
    oxygenconfig.cpp
    oxygenconfigurationui.cpp
    oxygendetectwidget.cpp
    oxygenexceptiondialog.cpp
    oxygenexceptionlistwidget.cpp
    oxygenexceptionmodel.cpp
    oxygenshadowconfigurationui.cpp
  )

kde4_add_ui_files(kwin_oxygen_config_PART_SRCS
   ui/oxygenconfigurationui.ui
   ui/oxygendetectwidget.ui
   ui/oxygenexceptiondialog.ui
   ui/oxygenexceptionlistwidget.ui
   ui/oxygenshadowconfigurationui.ui
)

kde4_add_plugin(kwin_oxygen_config ${kwin_oxygen_config_PART_SRCS})

target_link_libraries(
    kwin_oxygen_config
    ${KDE4_KDEUI_LIBS}
    ${QT_QTGUI_LIBRARY}
    ${X11_X11_LIB}
    kdecorations
    oxygenstyle
    oxygenstyleconfig
  )

install(TARGETS kwin_oxygen_config  DESTINATION ${PLUGIN_INSTALL_DIR} )
