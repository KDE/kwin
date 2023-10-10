# SPDX-FileCopyrightText: 2021 Vlad Zahorodnii <vlad.zahorodnii@kde.org>
#
# SPDX-License-Identifier: BSD-3-Clause

if (KWIN_BUILD_KCMS)
    add_subdirectory(kcm)
endif()

set(sidepeek_SOURCES
    main.cpp
    sidepeekeffect.cpp
)

kwin_add_builtin_effect(sidepeek ${sidepeek_SOURCES})
kconfig_add_kcfg_files(sidepeek
    sidepeekconfig.kcfgc
)


target_link_libraries(sidepeek PRIVATE
    kwineffects

    KF6::ConfigGui
    KF6::GlobalAccel
    KF6::I18n

    Qt::Quick
)

install(DIRECTORY qml DESTINATION ${KDE_INSTALL_DATADIR}/kwin/effects/sidepeek)
