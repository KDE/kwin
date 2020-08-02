/*
    KWin - the KDE window manager
    This file is part of the KDE project.

    SPDX-FileCopyrightText: 2015 Martin Gräßlin <mgraesslin@kde.org>

    SPDX-License-Identifier: GPL-2.0-or-later
*/
#include "scene_qpainter_fb_backend.h"
#include "fb_backend.h"
#include "composite.h"
#include "logind.h"
#include "cursor.h"
#include "virtual_terminal.h"
// Qt
#include <QPainter>

namespace KWin
{
FramebufferQPainterBackend::FramebufferQPainterBackend(FramebufferBackend *backend)
    : QObject()
    , QPainterBackend()
    , m_renderBuffer(backend->screenSize(), QImage::Format_RGB32)
    , m_backend(backend)
    , m_needsFullRepaint(true)
{
    m_renderBuffer.fill(Qt::black);
    m_backend->map();

    m_backBuffer = QImage((uchar*)m_backend->mappedMemory(),
                          m_backend->bytesPerLine() / (m_backend->bitsPerPixel() / 8),
                          m_backend->bufferSize() / m_backend->bytesPerLine(),
                          m_backend->bytesPerLine(), m_backend->imageFormat());
    m_backBuffer.fill(Qt::black);

    connect(VirtualTerminal::self(), &VirtualTerminal::activeChanged, this,
        [] (bool active) {
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

QImage* FramebufferQPainterBackend::buffer()
{
    return bufferForScreen(0);
}

QImage* FramebufferQPainterBackend::bufferForScreen(int screenId)
{
    Q_UNUSED(screenId)
    return &m_renderBuffer;
}

bool FramebufferQPainterBackend::needsFullRepaint() const
{
    return m_needsFullRepaint;
}

void FramebufferQPainterBackend::prepareRenderingFrame()
{
    m_needsFullRepaint = true;
}

void FramebufferQPainterBackend::present(int mask, const QRegion &damage)
{
    Q_UNUSED(mask)
    Q_UNUSED(damage)

    if (!LogindIntegration::self()->isActiveSession()) {
        return;
    }
    m_needsFullRepaint = false;

    QPainter p(&m_backBuffer);
    p.drawImage(QPoint(0, 0), m_backend->isBGR() ? m_renderBuffer.rgbSwapped() : m_renderBuffer);
}

bool FramebufferQPainterBackend::usesOverlayWindow() const
{
    return false;
}

bool FramebufferQPainterBackend::perScreenRendering() const
{
    return true;
}

}
