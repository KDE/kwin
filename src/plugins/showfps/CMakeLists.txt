# SPDX-FileCopyrightText: 2022 Arjen Hiemstra <ahiemstra@heimr.nl>
#
# SPDX-License-Identifier: BSD-3-Clause

set(showfps_SOURCES
    main.cpp
    showfpseffect.cpp
)

kwin_add_builtin_effect(showfps ${showfps_SOURCES})

qt_add_resources(showfps showfps_qml PREFIX "org.kde.kwin/showfps"
    FILES
        qml/main.qml
        qml/main-fallback.qml
)

target_link_libraries(showfps PRIVATE
    kwin

    KF6::I18n

    Qt::Quick
    )
