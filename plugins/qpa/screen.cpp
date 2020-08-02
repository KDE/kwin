/*
    KWin - the KDE window manager
    This file is part of the KDE project.

    SPDX-FileCopyrightText: 2015 Martin Gräßlin <mgraesslin@kde.org>

    SPDX-License-Identifier: GPL-2.0-or-later
*/
#include "screen.h"
#include "platformcursor.h"
#include "screens.h"

namespace KWin
{
namespace QPA
{

Screen::Screen(int screen)
    : QPlatformScreen()
    , m_screen(screen)
    , m_cursor(new PlatformCursor)
{
}

Screen::~Screen() = default;

int Screen::depth() const
{
    return 32;
}

QImage::Format Screen::format() const
{
    return QImage::Format_ARGB32_Premultiplied;
}

QRect Screen::geometry() const
{
    return m_screen != -1 ? screens()->geometry(m_screen) : QRect(0, 0, 1, 1);
}

QSizeF Screen::physicalSize() const
{
    return m_screen != -1 ? screens()->physicalSize(m_screen) : QPlatformScreen::physicalSize();
}

QPlatformCursor *Screen::cursor() const
{
    return m_cursor.data();
}

QDpi Screen::logicalDpi() const
{
    static int forceDpi = qEnvironmentVariableIsSet("QT_WAYLAND_FORCE_DPI") ? qEnvironmentVariableIntValue("QT_WAYLAND_FORCE_DPI") : -1;
    if (forceDpi > 0) {
        return QDpi(forceDpi, forceDpi);
    }

    return QDpi(96, 96);
}

qreal Screen::devicePixelRatio() const
{
    return m_screen != -1 ? screens()->scale(m_screen) : 1.0;
}

}
}
