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
#include "scene_qpainter_virtual_backend.h"
#include "virtual_backend.h"
#include "cursor.h"

#include <QPainter>

namespace KWin
{
VirtualQPainterBackend::VirtualQPainterBackend(VirtualBackend *backend)
    : QPainterBackend()
    , m_backBuffer(backend->size(), QImage::Format_RGB32)
    , m_backend(backend)
{
}

VirtualQPainterBackend::~VirtualQPainterBackend() = default;

QImage *VirtualQPainterBackend::buffer()
{
    return &m_backBuffer;
}

bool VirtualQPainterBackend::needsFullRepaint() const
{
    return true;
}

void VirtualQPainterBackend::prepareRenderingFrame()
{
}

void VirtualQPainterBackend::screenGeometryChanged(const QSize &size)
{
    if (m_backBuffer.size() != size) {
        m_backBuffer = QImage(size, QImage::Format_RGB32);
        m_backBuffer.fill(Qt::black);
    }
}

void VirtualQPainterBackend::present(int mask, const QRegion &damage)
{
    Q_UNUSED(mask)
    Q_UNUSED(damage)
    if (m_backend->saveFrames()) {
        m_backBuffer.save(QStringLiteral("%1/%2.png").arg(m_backend->screenshotDirPath()).arg(QString::number(m_frameCounter++)));
    }
}

bool VirtualQPainterBackend::usesOverlayWindow() const
{
    return false;
}

}
