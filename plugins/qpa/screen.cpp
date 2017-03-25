/********************************************************************
 KWin - the KDE window manager
 This file is part of the KDE project.

Copyright (C) 2015 Martin Gräßlin <mgraesslin@kde.org>

This program is free software; you can redistribute it and/or modify
it under the terms of the GNU General Public License as published by
the Free Software Foundation; either version 2 of the License, or
(at your option) any later version.

This program is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with this program.  If not, see <http://www.gnu.org/licenses/>.
*********************************************************************/
#include "screen.h"
#include "platformcursor.h"
#include "wayland_server.h"

#include <KWayland/Client/output.h>

namespace KWin
{
namespace QPA
{

Screen::Screen(KWayland::Client::Output *o)
    : QPlatformScreen()
    , m_output(QPointer<KWayland::Client::Output>(o))
    , m_cursor(new PlatformCursor)
{
    // TODO: connect to resolution changes
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
    return m_output ? QRect(m_output->globalPosition(), m_output->pixelSize() / m_output->scale()) : QRect(0, 0, 1, 1);
}

QSizeF Screen::physicalSize() const
{
    return m_output ? m_output->physicalSize() : QPlatformScreen::physicalSize();
}

QPlatformCursor *Screen::cursor() const
{
    return m_cursor.data();
}

QDpi Screen::logicalDpi() const
{
    static int force_dpi = qEnvironmentVariableIsSet("QT_WAYLAND_FORCE_DPI") ? qEnvironmentVariableIntValue("QT_WAYLAND_FORCE_DPI") : -1;
    if (force_dpi > 0) {
        return QDpi(force_dpi, force_dpi);
    }

    return QPlatformScreen::logicalDpi();
}

qreal Screen::devicePixelRatio() const
{
    return m_output ? (qreal)m_output->scale() : 1.0;
}

}
}
