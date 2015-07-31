set(plugin_SRCS
    previewbutton.cpp
    previewbridge.cpp
    previewclient.cpp
    previewitem.cpp
    previewsettings.cpp
    plugin.cpp
    buttonsmodel.cpp
    ../../../decorations/decorationpalette.cpp
    ../../../decorations/decorations_logging.cpp
)

add_library(kdecorationprivatedeclarative SHARED ${plugin_SRCS})
target_link_libraries(kdecorationprivatedeclarative
    KDecoration2::KDecoration
    KDecoration2::KDecoration2Private
    Qt5::DBus
    Qt5::Quick
    KF5::CoreAddons
    KF5::ConfigWidgets
    KF5::I18n
    KF5::Service
)

install(TARGETS kdecorationprivatedeclarative DESTINATION ${QML_INSTALL_DIR}/org/kde/kwin/private/kdecoration )
install(FILES qmldir DESTINATION ${QML_INSTALL_DIR}/org/kde/kwin/private/kdecoration )
