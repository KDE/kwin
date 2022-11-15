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

qt_add_dbus_adaptor(overview_SOURCES org.kde.KWin.Effect.Overview1.xml overvieweffect.h KWin::OverviewEffect)

qt_add_dbus_adaptor(overview_SOURCES org.kde.KWin.Effect.Overview1.xml overvieweffect.h KWin::OverviewEffect)

kwin_add_builtin_effect(overview ${overview_SOURCES})
target_link_libraries(overview PRIVATE
    kwineffects

    KF6::ConfigGui
    KF6::GlobalAccel
    KF6::I18n

    Qt::Quick
)

install(DIRECTORY qml DESTINATION ${KDE_INSTALL_DATADIR}/kwin/effects/overview)
