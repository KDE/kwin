/********************************************************************
 KWin - the KDE window manager
 This file is part of the KDE project.

Copyright 2017 Roman Gilg <subdiff@gmail.com>

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

#include "colorcorrectdbusinterface.h"
#include "colorcorrectadaptor.h"

#include "manager.h"

namespace KWin {
namespace ColorCorrect {

ColorCorrectDBusInterface::ColorCorrectDBusInterface(Manager *parent)
    : QObject(parent)
    , m_manager(parent)
{
    connect(m_manager, &Manager::configChange, this, &ColorCorrectDBusInterface::nightColorConfigChanged);
    new ColorCorrectAdaptor(this);
    QDBusConnection::sessionBus().registerObject(QStringLiteral("/ColorCorrect"), this);
}

QHash<QString, QVariant> ColorCorrectDBusInterface::nightColorInfo()
{
    return m_manager->info();
}

bool ColorCorrectDBusInterface::setNightColorConfig(QHash<QString, QVariant> data)
{
    return m_manager->changeConfiguration(data);
}

void ColorCorrectDBusInterface::nightColorAutoLocationUpdate(double latitude, double longitude)
{
    m_manager->autoLocationUpdate(latitude, longitude);
}

}
}
