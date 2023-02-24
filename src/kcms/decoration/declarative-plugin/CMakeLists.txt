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
    Qt::DBus
    Qt::Quick
    KF6::CoreAddons
    KF6::ConfigWidgets
    KF6::I18n
    KF6::Service
)

install(TARGETS kdecorationprivatedeclarative DESTINATION ${KDE_INSTALL_QMLDIR}/org/kde/kwin/private/kdecoration )
install(FILES qmldir DESTINATION ${KDE_INSTALL_QMLDIR}/org/kde/kwin/private/kdecoration )
