set(plugin_SRCS
    previewbutton.cpp previewbutton.h
    previewbridge.cpp previewbridge.h
    previewclient.cpp previewclient.h
    previewitem.cpp previewitem.h
    previewsettings.cpp previewsettings.h
    plugin.cpp plugin.h
    buttonsmodel.cpp buttonsmodel.h
    ../../../decorations/decorationpalette.cpp ../../../decorations/decorationpalette.h
    ../../../decorations/decorations_logging.cpp ../../../decorations/decorations_logging.h
)

add_library(kdecorationprivatedeclarative SHARED ${plugin_SRCS})
target_link_libraries(kdecorationprivatedeclarative
    KDecoration2::KDecoration
    KDecoration2::KDecoration2Private
    Qt::DBus
    Qt::Quick
    KF5::CoreAddons
    KF5::ConfigWidgets
    KF5::I18n
    KF5::Service
)

install(TARGETS kdecorationprivatedeclarative DESTINATION ${KDE_INSTALL_QMLDIR}/org/kde/kwin/private/kdecoration )
install(FILES qmldir DESTINATION ${KDE_INSTALL_QMLDIR}/org/kde/kwin/private/kdecoration )
