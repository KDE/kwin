# SPDX-FileCopyrightText: 2022 Marco Martin <mart@kde.org>
#
# SPDX-License-Identifier: BSD-3-Clause

if (KWIN_BUILD_KCMS)
    add_subdirectory(kcm)
endif()

set(tileseditor_SOURCES
    main.cpp
    tileseditoreffect.cpp
)

kwin_add_builtin_effect(tileseditor ${tileseditor_SOURCES})

ecm_add_qml_module(tileseditor
    URI org.kde.kwin.tileseditor
    RESOURCES qml/layouts.svg
    QML_FILES
        qml/Main.qml
        qml/ResizeCorner.qml
        qml/ResizeHandle.qml
        qml/TileDelegate.qml
    QT_NO_PLUGIN
)

target_link_libraries(tileseditor PRIVATE
    kwin

    KF6::ConfigGui
    KF6::GlobalAccel
    KF6::I18n

    Qt::Quick
)
