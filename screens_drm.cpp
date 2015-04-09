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
#include "screens_drm.h"
#include "drm_backend.h"

namespace KWin
{

DrmScreens::DrmScreens(DrmBackend *backend, QObject *parent)
    : Screens(parent)
    , m_backend(backend)
{
}

DrmScreens::~DrmScreens() = default;

void DrmScreens::init()
{
    KWin::Screens::init();
    updateCount();
    emit changed();
}

QRect DrmScreens::geometry(int screen) const
{
    if (screen == 0) {
        return QRect(QPoint(0, 0), size(screen));
    }
    return QRect();
}

QSize DrmScreens::size(int screen) const
{
    if (screen == 0) {
        return m_backend->size();
    }
    return QSize();
}

void DrmScreens::updateCount()
{
    setCount(1);
}

int DrmScreens::number(const QPoint &pos) const
{
    Q_UNUSED(pos)
    return 0;
}

}
