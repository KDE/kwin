/*
    KWin - the KDE window manager
    This file is part of the KDE project.

    SPDX-FileCopyrightText: 2014 Martin Gräßlin <mgraesslin@kde.org>

    SPDX-License-Identifier: GPL-2.0-or-later
*/
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
