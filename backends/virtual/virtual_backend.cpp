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
#include "virtual_backend.h"
#include "scene_qpainter_virtual_backend.h"
#include "screens_virtual.h"
#include "wayland_server.h"
// KWayland
#include <KWayland/Server/seat_interface.h>

namespace KWin
{

VirtualBackend::VirtualBackend(QObject *parent)
    : AbstractBackend(parent)
{
    setSoftWareCursor(true);
    setSupportsPointerWarping(true);
    // currently only QPainter - enforce it
    qputenv("KWIN_COMPOSE", QByteArrayLiteral("Q"));
}

VirtualBackend::~VirtualBackend() = default;

void VirtualBackend::init()
{
    m_size = initialWindowSize();
    setReady(true);
    waylandServer()->seat()->setHasPointer(true);
    waylandServer()->seat()->setHasKeyboard(true);
    waylandServer()->seat()->setHasTouch(true);
    emit screensQueried();
}

Screens *VirtualBackend::createScreens(QObject *parent)
{
    return new VirtualScreens(this, parent);
}

QPainterBackend *VirtualBackend::createQPainterBackend()
{
    return new VirtualQPainterBackend(this);
}

}
