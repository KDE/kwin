# SPDX-FileCopyrightText: 2022 Marco Martin <mart@kde.org>
#
# SPDX-License-Identifier: BSD-3-Clause

set(mousemark_SOURCES
    main.cpp
    mouseclickeffect.cpp
)

kwin_add_builtin_effect(mouseclick2 ${mousemark_SOURCES})

ecm_add_qml_module(mouseclick2
    URI org.kde.kwin.mouseclick2
    QML_FILES
        qml/Main.qml
    QT_NO_PLUGIN
)

qt6_add_shaders(mouseclick2 "shaders"
    BATCHABLE
    PRECOMPILE
    OPTIMIZED
    PREFIX
        "/qt/qml/org/kde/kwin/mouseclick2/qml"
    FILES
        "shaders/water.vert"
        "shaders/water.frag"
        "shaders/watersimulation.frag"
)

target_link_libraries(mouseclick2 PRIVATE
    kwin

    KF6::ConfigGui
    KF6::GlobalAccel
    KF6::I18n

    Qt::Quick
    Qt::ShaderTools
)
