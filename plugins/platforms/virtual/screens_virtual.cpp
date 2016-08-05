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
#include "screens_virtual.h"
#include "virtual_backend.h"

namespace KWin
{

VirtualScreens::VirtualScreens(VirtualBackend *backend, QObject *parent)
    : Screens(parent)
    , m_backend(backend)
{
}

VirtualScreens::~VirtualScreens() = default;

void VirtualScreens::init()
{
    updateCount();
    KWin::Screens::init();
    connect(m_backend, &VirtualBackend::sizeChanged,
            this, &VirtualScreens::startChangedTimer);
    connect(m_backend, &VirtualBackend::outputGeometriesChanged, this,
        [this] (const QVector<QRect> &geometries) {
            const int oldCount = m_geometries.count();
            m_geometries = geometries;
            if (oldCount != m_geometries.count()) {
                setCount(m_geometries.count());
            } else {
                emit changed();
            }
        }
    );
    emit changed();
}

QRect VirtualScreens::geometry(int screen) const
{
    if (screen >= m_geometries.count()) {
        return QRect();
    }
    return m_geometries.at(screen);
}

QSize VirtualScreens::size(int screen) const
{
    return geometry(screen).size();
}

void VirtualScreens::updateCount()
{
    m_geometries.clear();
    const QSize size = m_backend->size();
    for (int i = 0; i < m_backend->outputCount(); ++i) {
        m_geometries.append(QRect(size.width() * i, 0, size.width(), size.height()));
    }
    setCount(m_backend->outputCount());
}

int VirtualScreens::number(const QPoint &pos) const
{
    int bestScreen = 0;
    int minDistance = INT_MAX;
    for (int i = 0; i < m_geometries.count(); ++i) {
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

}
