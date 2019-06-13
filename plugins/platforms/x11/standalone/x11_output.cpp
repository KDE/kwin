/********************************************************************
 KWin - the KDE window manager
 This file is part of the KDE project.

Copyright 2019 Roman Gilg <subdiff@gmail.com>

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
#include "x11_output.h"
#include "screens.h"

namespace KWin
{

X11Output::X11Output(QObject *parent)
    : AbstractOutput(parent)
{
}

QString X11Output::name() const
{
    return m_name;
}

void X11Output::setName(QString set)
{
    m_name = set;
}

QRect X11Output::geometry() const
{
    if (m_geometry.isValid()) {
        return m_geometry;
    }
    return QRect(QPoint(0, 0), Screens::self()->displaySize()); // xinerama, lacks RandR
}

void X11Output::setGeometry(QRect set)
{
    m_geometry = set;
}

int X11Output::refreshRate() const
{
    return m_refreshRate;
}

void X11Output::setRefreshRate(int set)
{
    m_refreshRate = set;
}

}
