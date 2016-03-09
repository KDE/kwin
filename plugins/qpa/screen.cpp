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

#include <KWayland/Client/output.h>

namespace KWin
{
namespace QPA
{

Screen::Screen(KWayland::Client::Output *o)
    : QPlatformScreen()
    , m_output(o)
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
    return m_output->geometry();
}

QSizeF Screen::physicalSize() const
{
    return m_output->physicalSize();
}

QPlatformCursor *Screen::cursor() const
{
    return m_cursor.data();
}

}
}
