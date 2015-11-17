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
#include <KWayland/Client/output.h>
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
    connect(m_backend, &Wayland::WaylandBackend::outputsChanged,
            this, &WaylandScreens::startChangedTimer);
    updateCount();
}

QRect WaylandScreens::geometry(int screen) const
{
    if (screen >= m_geometries.size()) {
        return QRect();
    }
    return m_geometries.at(screen);
}

QSize WaylandScreens::size(int screen) const
{
    return geometry(screen).size();
}

int WaylandScreens::number(const QPoint &pos) const
{
    int bestScreen = 0;
    int minDistance = INT_MAX;
    for (int i = 0; i < m_geometries.size(); ++i) {
        const QRect &geo = m_geometries.at(i);
        if (geo.contains(pos)) {
            return i;
        }
        int distance = QPoint(geo.topLeft() - pos).manhattanLength();
        distance = qMin(distance, QPoint(geo.topRight() - pos).manhattanLength());
        distance = qMin(distance, QPoint(geo.bottomRight() - pos).manhattanLength());
        distance = qMin(distance, QPoint(geo.bottomLeft() - pos).manhattanLength());
        if (distance < minDistance) {
            minDistance = distance;
            bestScreen = i;
        }
    }
    return bestScreen;
}

void WaylandScreens::updateCount()
{
    m_geometries.clear();
    int count = 0;
    const QList<KWayland::Client::Output*> &outputs = m_backend->outputs();
    for (auto it = outputs.begin(); it != outputs.end(); ++it) {
        if ((*it)->pixelSize().isEmpty()) {
            continue;
        }
        count++;
        m_geometries.append(QRect((*it)->globalPosition(), (*it)->pixelSize()));
    }
    if (m_geometries.isEmpty()) {
        // we need a fake screen
        m_geometries.append(QRect(0, 0, displayWidth(), displayHeight()));
        setCount(1);
        return;
    }
    setCount(m_geometries.count());
    emit changed();
}

}
