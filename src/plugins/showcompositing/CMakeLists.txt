# SPDX-FileCopyrightText: 2022 Arjen Hiemstra <ahiemstra@heimr.nl>
# SPDX-FileCopyrightText: 2024 Xaver Hugl <xaver.hugl@gmail.com>
#
# SPDX-License-Identifier: BSD-3-Clause

set(showcompositing_SOURCES
    main.cpp
    showcompositing.cpp
)

kwin_add_builtin_effect(showcompositing ${showcompositing_SOURCES})

ecm_add_qml_module(showcompositing
    URI org.kde.kwin.showcompositing
    QML_FILES
          qml/Main.qml
    QT_NO_PLUGIN
)

target_link_libraries(showcompositing PRIVATE
    kwin
    KF6::I18n
    Qt::Quick
)
