include_directories( ${KDEBASE_WORKSPACE_SOURCE_DIR}/libs/oxygen)

set(kwin_oxygen_config_PART_SRCS
    oxygenanimationconfigwidget.cpp
    oxygenconfig.cpp
    oxygenconfigwidget.cpp
    oxygendetectwidget.cpp
    oxygenexceptiondialog.cpp
    oxygenexceptionlistwidget.cpp
    oxygenexceptionmodel.cpp
    ../oxygenexceptionlist.cpp
  )

kde4_add_kcfg_files(kwin_oxygen_config_PART_SRCS ../oxygenconfiguration.kcfgc)

set( kwin_oxygen_config_PART_FORMS
   ui/oxygenconfigurationui.ui
   ui/oxygendetectwidget.ui
   ui/oxygenexceptiondialog.ui
   ui/oxygenexceptionlistwidget.ui
)

qt5_wrap_ui(kwin_oxygen_config_PART_FORMS_HEADERS ${kwin_oxygen_config_PART_FORMS})

add_library(kwin_oxygen_config
    ${kwin_oxygen_config_PART_SRCS}
    ${kwin_oxygen_config_PART_FORMS_HEADERS})

target_link_libraries(kwin_oxygen_config Qt5::Core Qt5::Gui Qt5::Widgets Qt5::DBus)
target_link_libraries(kwin_oxygen_config KF5::KConfigCore KF5::KConfigGui KF5::KCompletion KF5::KGuiAddons KF5::KWindowSystem KF5::KI18n)
target_link_libraries(kwin_oxygen_config kdecorations oxygenstyle  oxygenstyleconfig)

if(X11_FOUND)
  target_link_libraries(kwin_oxygen_config ${X11_LIBRARIES})
  target_link_libraries(kwin_oxygen_config Qt5::X11Extras)
endif()

install(TARGETS kwin_oxygen_config DESTINATION ${QT_PLUGIN_INSTALL_DIR}/kf5)
