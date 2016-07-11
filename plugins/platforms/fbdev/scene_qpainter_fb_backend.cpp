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
#include "scene_qpainter_fb_backend.h"
#include "fb_backend.h"
#include "composite.h"
#include "cursor.h"
#include "virtual_terminal.h"
// Qt
#include <QPainter>

namespace KWin
{
FramebufferQPainterBackend::FramebufferQPainterBackend(FramebufferBackend *backend)
    : QObject()
    , QPainterBackend()
    , m_renderBuffer(backend->size(), QImage::Format_RGB32)
    , m_backend(backend)
{
    m_renderBuffer.fill(Qt::black);

    m_backend->map();

    m_backBuffer = QImage((uchar*)backend->mappedMemory(),
                          backend->bytesPerLine() / (backend->bitsPerPixel() / 8),
                          backend->bufferSize() / backend->bytesPerLine(),
                          backend->bytesPerLine(), backend->imageFormat());

    m_backBuffer.fill(Qt::black);
    connect(VirtualTerminal::self(), &VirtualTerminal::activeChanged, this,
        [this] (bool active) {
            if (active) {
                Compositor::self()->bufferSwapComplete();
                Compositor::self()->addRepaintFull();
            } else {
                Compositor::self()->aboutToSwapBuffers();
            }
        }
    );
}

FramebufferQPainterBackend::~FramebufferQPainterBackend() = default;

QImage *FramebufferQPainterBackend::buffer()
{
    return &m_renderBuffer;
}

bool FramebufferQPainterBackend::needsFullRepaint() const
{
    return false;
}

void FramebufferQPainterBackend::prepareRenderingFrame()
{
}

void FramebufferQPainterBackend::present(int mask, const QRegion &damage)
{
    Q_UNUSED(mask)
    Q_UNUSED(damage)
    if (!VirtualTerminal::self()->isActive()) {
        return;
    }
    QPainter p(&m_backBuffer);
    p.drawImage(QPoint(0, 0), m_backend->isBGR() ? m_renderBuffer.rgbSwapped() : m_renderBuffer);
}

bool FramebufferQPainterBackend::usesOverlayWindow() const
{
    return false;
}

}
