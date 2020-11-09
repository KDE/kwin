/*
    KWin - the KDE window manager
    This file is part of the KDE project.

    SPDX-FileCopyrightText: 2015 Martin Gräßlin <mgraesslin@kde.org>

    SPDX-License-Identifier: GPL-2.0-or-later
*/
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

QImage *VirtualQPainterBackend::bufferForScreen(int screen)
{
    return &m_backBuffers[screen];
}

bool VirtualQPainterBackend::needsFullRepaint(int screenId) const
{
    Q_UNUSED(screenId)
    return true;
}

void VirtualQPainterBackend::prepareRenderingFrame(int screenId)
{
    Q_UNUSED(screenId)
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

void VirtualQPainterBackend::present(int screenId, int mask, const QRegion &damage)
{
    Q_UNUSED(mask)
    Q_UNUSED(damage)
    if (m_backend->saveFrames()) {
        m_backBuffers[screenId].save(QStringLiteral("%1/screen%2-%3.png").arg(m_backend->screenshotDirPath(), QString::number(screenId), QString::number(m_frameCounter++)));
    }
}

bool VirtualQPainterBackend::perScreenRendering() const
{
    return true;
}

}
