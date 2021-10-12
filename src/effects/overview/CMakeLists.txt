# SPDX-FileCopyrightText: 2021 Vlad Zahorodnii <vlad.zahorodnii@kde.org>
#
# SPDX-License-Identifier: BSD-3-Clause

add_subdirectory(kcm)

set(overview_SOURCES
    expoarea.cpp
    expolayout.cpp
    main.cpp
    overvieweffect.cpp
)

kconfig_add_kcfg_files(overview_SOURCES
    overviewconfig.kcfgc
)

kwin4_add_effect_module(kwin4_effect_overview ${overview_SOURCES})

install(DIRECTORY qml DESTINATION ${KDE_INSTALL_DATADIR}/kwin/effects/overview)
