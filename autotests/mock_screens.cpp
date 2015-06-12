/********************************************************************
KWin - the KDE window manager
This file is part of the KDE project.

Copyright (C) 2014 Martin Gräßlin <mgraesslin@kde.org>

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
#include "mock_screens.h"

namespace KWin
{

MockScreens::MockScreens(QObject *parent)
    : Screens(parent)
{
}

MockScreens::~MockScreens() = default;

QRect MockScreens::geometry(int screen) const
{
    if (screen >= m_geometries.count()) {
        return QRect();
    }
    return m_geometries.at(screen);
}

QString MockScreens::name(int screen) const
{
    Q_UNUSED(screen);
    return QLatin1String("MoccaScreen"); // mock-a-screen =)
}

float MockScreens::refreshRate(int screen) const
{
    Q_UNUSED(screen);
    return 60.0f;
}

QSize MockScreens::size(int screen) const
{
    return geometry(screen).size();
}

int MockScreens::number(const QPoint &pos) const
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

void MockScreens::init()
{
    Screens::init();
    m_scheduledGeometries << QRect(0, 0, 100, 100);
    updateCount();
}

void MockScreens::updateCount()
{
    m_geometries = m_scheduledGeometries;
    setCount(m_geometries.size());
    emit changed();
}

void MockScreens::setGeometries(const QList< QRect > &geometries)
{
    m_scheduledGeometries = geometries;
    startChangedTimer();
}

}
