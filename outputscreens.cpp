/********************************************************************
 KWin - the KDE window manager
 This file is part of the KDE project.

Copyright 2018 Roman Gilg <subdiff@gmail.com>

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
#include "outputscreens.h"
#include "platform.h"
#include "abstract_output.h"

namespace KWin
{

OutputScreens::OutputScreens(Platform *platform, QObject *parent)
    : Screens(parent),
      m_platform(platform)
{
}

OutputScreens::~OutputScreens() = default;

void OutputScreens::init()
{
    updateCount();
    KWin::Screens::init();
    emit changed();
}

QString OutputScreens::name(int screen) const
{
    const auto enOuts = m_platform->enabledOutputs();
    if (screen >= enOuts.size()) {
        return Screens::name(screen);
    }
    return enOuts.at(screen)->name();
}

bool OutputScreens::isInternal(int screen) const
{
    const auto enOuts = m_platform->enabledOutputs();
    if (screen >= enOuts.size()) {
        return false;
    }
    return enOuts.at(screen)->isInternal();
}

QRect OutputScreens::geometry(int screen) const
{
    const auto enOuts = m_platform->enabledOutputs();
    if (screen >= enOuts.size()) {
        return QRect();
    }
    return enOuts.at(screen)->geometry();
}

QSize OutputScreens::size(int screen) const
{
    const auto enOuts = m_platform->enabledOutputs();
    if (screen >= enOuts.size()) {
        return QSize();
    }
    return enOuts.at(screen)->geometry().size();
}

qreal OutputScreens::scale(int screen) const
{
    const auto enOuts = m_platform->enabledOutputs();
    if (screen >= enOuts.size()) {
        return 1;
    }
    return enOuts.at(screen)->scale();
}

QSizeF OutputScreens::physicalSize(int screen) const
{
    const auto enOuts = m_platform->enabledOutputs();
    if (screen >= enOuts.size()) {
        return Screens::physicalSize(screen);
    }
    return enOuts.at(screen)->physicalSize();
}

float OutputScreens::refreshRate(int screen) const
{
    const auto enOuts = m_platform->enabledOutputs();
    if (screen >= enOuts.size()) {
        return Screens::refreshRate(screen);
    }
    return enOuts.at(screen)->refreshRate() / 1000.0f;
}

Qt::ScreenOrientation OutputScreens::orientation(int screen) const
{
    const auto enOuts = m_platform->enabledOutputs();
    if (screen >= enOuts.size()) {
        return Qt::PrimaryOrientation;
    }
    return enOuts.at(screen)->orientation();
}

void OutputScreens::updateCount()
{
    setCount(m_platform->enabledOutputs().size());
}

int OutputScreens::number(const QPoint &pos) const
{
    int bestScreen = 0;
    int minDistance = INT_MAX;
    const auto enOuts = m_platform->enabledOutputs();
    for (int i = 0; i < enOuts.size(); ++i) {
        const QRect &geo = enOuts.at(i)->geometry();
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

} // namespace
