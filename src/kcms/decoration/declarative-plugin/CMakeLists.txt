ecm_add_qml_module(kdecorationprivatedeclarative URI org.kde.kwin.private.kdecoration DEPENDENCIES QtCore QtQuick GENERATE_PLUGIN_SOURCE)

target_sources(kdecorationprivatedeclarative PRIVATE
    previewbutton.cpp
    previewbridge.cpp
    previewclient.cpp
    previewitem.cpp
    previewsettings.cpp
    buttonsmodel.cpp
    ../../../decorations/decorationpalette.cpp
    ../../../decorations/decorations_logging.cpp
    types.h
)

target_link_libraries(kdecorationprivatedeclarative PRIVATE
    KDecoration3::KDecoration
    KDecoration3::KDecoration3Private
    Qt::DBus
    Qt::Quick
    KF6::CoreAddons
    KF6::KCMUtils
    KF6::I18n
    KF6::Service
)

ecm_finalize_qml_module(kdecorationprivatedeclarative)
