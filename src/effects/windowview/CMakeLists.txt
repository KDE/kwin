# SPDX-FileCopyrightText: 2021 Vlad Zahorodnii <vlad.zahorodnii@kde.org>
#
# SPDX-License-Identifier: BSD-3-Clause

set(windowview_SOURCES
    main.cpp
    windowvieweffect.cpp
)

qt_add_dbus_adaptor(windowview_SOURCES org.kde.KWin.Effect.WindowView1.xml windowvieweffect.h KWin::WindowViewEffect)

kwin4_add_effect_module(kwin4_effect_windowview ${windowview_SOURCES})

install(DIRECTORY qml DESTINATION ${KDE_INSTALL_DATADIR}/kwin/effects/windowview)
