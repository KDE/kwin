# SPDX-FileCopyrightText: 2022 Arjen Hiemstra <ahiemstra@heimr.nl>
#
# SPDX-License-Identifier: BSD-3-Clause

set(showfps_SOURCES
    main.cpp
    showfpseffect.cpp
)

kwin4_add_effect_module(kwin4_effect_showfps ${showfps_SOURCES})

target_link_libraries(kwin4_effect_showfps PRIVATE
    kwineffects

    KF5::I18n

    Qt::Quick
    )

install(DIRECTORY qml DESTINATION ${KDE_INSTALL_DATADIR}/kwin/effects/showfps)
