# SPDX-FileCopyrightText: 2021 Vlad Zahorodnii <vlad.zahorodnii@kde.org>
#
# SPDX-License-Identifier: BSD-3-Clause

if (KWIN_BUILD_KCMS)
    add_subdirectory(kcm)
endif()

set(tileseditor_SOURCES
    main.cpp
    tileseditoreffect.cpp
)

kconfig_add_kcfg_files(tileseditor_SOURCES
    tileseditorconfig.kcfgc
)

kwin4_add_effect_module(kwin4_effect_tileseditor ${tileseditor_SOURCES})

target_link_libraries(kwin4_effect_tileseditor PRIVATE
    kwineffects

    KF5::ConfigGui
    KF5::GlobalAccel
    KF5::I18n

    Qt::Quick
    )

install(DIRECTORY qml DESTINATION ${KDE_INSTALL_DATADIR}/kwin/effects/tileseditor)
