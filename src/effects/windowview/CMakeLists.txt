# SPDX-FileCopyrightText: 2021 Vlad Zahorodnii <vlad.zahorodnii@kde.org>
#
# SPDX-License-Identifier: BSD-3-Clause

if (KWIN_BUILD_KCMS)
    add_subdirectory(kcm)
endif()

set(windowview_SOURCES
    main.cpp
    windowvieweffect.cpp
)

kconfig_add_kcfg_files(windowview_SOURCES
    windowviewconfig.kcfgc
)

qt_add_dbus_adaptor(windowview_SOURCES org.kde.KWin.Effect.WindowView1.xml windowvieweffect.h KWin::WindowViewEffect)

kwin4_add_effect_module(windowview ${windowview_SOURCES})

target_link_libraries(windowview PRIVATE
    kwineffects

    KF6::ConfigGui
    KF6::GlobalAccel
    KF6::I18n

    Qt::Quick
    )

install(DIRECTORY qml DESTINATION ${KDE_INSTALL_DATADIR}/kwin/effects/windowview)
