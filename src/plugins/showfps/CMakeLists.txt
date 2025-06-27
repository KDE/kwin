# SPDX-FileCopyrightText: 2022 Arjen Hiemstra <ahiemstra@heimr.nl>
#
# SPDX-License-Identifier: BSD-3-Clause

set(showfps_SOURCES
    main.cpp
    showfpseffect.cpp
)

kwin_add_builtin_effect(showfps ${showfps_SOURCES})

ecm_add_qml_module(showfps
    URI "org.kde.kwin.showfps"
    QML_FILES
        qml/Main.qml
        qml/Fallback.qml
    QT_NO_PLUGIN
)

target_link_libraries(showfps PRIVATE
    kwin

    KF6::I18n

    Qt::Quick
    )
