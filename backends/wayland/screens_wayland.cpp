/********************************************************************
 KWin - the KDE window manager
 This file is part of the KDE project.

Copyright (C) 2013 Martin Gräßlin <mgraesslin@kde.org>

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
#include "screens_wayland.h"

#include "wayland_backend.h"
#include "logging.h"
#include "main.h"

#include <QDebug>

namespace KWin
{

WaylandScreens::WaylandScreens(Wayland::WaylandBackend *backend, QObject* parent)
    : Screens(parent)
    , m_backend(backend)
{
}

WaylandScreens::~WaylandScreens()
{
}

void WaylandScreens::init()
{
    Screens::init();
    connect(m_backend, &Wayland::WaylandBackend::shellSurfaceSizeChanged,
            this, &WaylandScreens::startChangedTimer);
    updateCount();
}

QRect WaylandScreens::geometry(int screen) const
{
    if (screen == 0) {
        return QRect(QPoint(0, 0), size(screen));
    }
    return QRect();
}

QSize WaylandScreens::size(int screen) const
{
    if (screen == 0) {
        return m_backend->shellSurfaceSize();
    }
    return QSize();
}

int WaylandScreens::number(const QPoint &pos) const
{
    Q_UNUSED(pos)
    return 0;
}

void WaylandScreens::updateCount()
{
    setCount(1);
}

}
