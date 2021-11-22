# SPDX-FileCopyrightText: 2021 Vlad Zahorodnii <vlad.zahorodnii@kde.org>
#
# SPDX-License-Identifier: BSD-3-Clause

if (KWIN_BUILD_KCMS)
    add_subdirectory(kcm)
endif()

set(overview_SOURCES
    main.cpp
    overvieweffect.cpp
)

kconfig_add_kcfg_files(overview_SOURCES
    overviewconfig.kcfgc
)

kwin4_add_effect_module(kwin4_effect_overview ${overview_SOURCES})
target_link_libraries(kwin4_effect_overview PRIVATE
    kwineffects

    KF5::ConfigGui
    KF5::GlobalAccel
    KF5::I18n

    Qt::Quick
)

install(DIRECTORY qml DESTINATION ${KDE_INSTALL_DATADIR}/kwin/effects/overview)
