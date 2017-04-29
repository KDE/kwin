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
#include "screens.h"

#include <QPainter>

namespace KWin
{
VirtualQPainterBackend::VirtualQPainterBackend(VirtualBackend *backend)
    : QPainterBackend()
    , m_backend(backend)
{
    connect(screens(), &Screens::changed, this, &VirtualQPainterBackend::createOutputs);
    createOutputs();
}

VirtualQPainterBackend::~VirtualQPainterBackend() = default;

QImage *VirtualQPainterBackend::buffer()
{
    return &m_backBuffers[0];
}

QImage *VirtualQPainterBackend::bufferForScreen(int screen)
{
    return &m_backBuffers[screen];
}

bool VirtualQPainterBackend::needsFullRepaint() const
{
    return true;
}

void VirtualQPainterBackend::prepareRenderingFrame()
{
}

void VirtualQPainterBackend::createOutputs()
{
    m_backBuffers.clear();
    for (int i = 0; i < screens()->count(); ++i) {
        QImage buffer(screens()->size(i) * screens()->scale(i), QImage::Format_RGB32);
        buffer.fill(Qt::black);
        m_backBuffers << buffer;
    }
}

void VirtualQPainterBackend::present(int mask, const QRegion &damage)
{
    Q_UNUSED(mask)
    Q_UNUSED(damage)
    if (m_backend->saveFrames()) {
        for (int i=0; i < m_backBuffers.size() ; i++) {
            m_backBuffers[i].save(QStringLiteral("%1/screen%2-%3.png").arg(m_backend->screenshotDirPath(), QString::number(i), QString::number(m_frameCounter++)));
        }
    }
}

bool VirtualQPainterBackend::usesOverlayWindow() const
{
    return false;
}

bool VirtualQPainterBackend::perScreenRendering() const
{
    return true;
}

}
