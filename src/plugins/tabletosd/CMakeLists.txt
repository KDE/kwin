# SPDX-FileCopyrightText: 2021 Vlad Zahorodnii <vlad.zahorodnii@kde.org>
#
# SPDX-License-Identifier: BSD-3-Clause

set(tabletosd_SOURCES
    main.cpp
    tabletosdeffect.cpp
    inputsequence.cpp
)

kwin_add_builtin_effect(tabletosd ${tabletosd_SOURCES})

ecm_add_qml_module(tabletosd
    URI "org.kde.kwin.tabletosd"
    QML_FILES
        qml/Main.qml
    QT_NO_PLUGIN
)

target_link_libraries(tabletosd PRIVATE
    kwin

    KF6::ConfigGui
    KF6::GlobalAccel
    KF6::I18n

    Qt::Quick
    PkgConfig::LIBWACOM
)
