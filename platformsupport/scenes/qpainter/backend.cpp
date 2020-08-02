/*
    KWin - the KDE window manager
    This file is part of the KDE project.

    SPDX-FileCopyrightText: 2013 Martin Gräßlin <mgraesslin@kde.org>

    SPDX-License-Identifier: GPL-2.0-or-later
*/
#include "backend.h"
#include <logging.h>

#include <QtGlobal>

namespace KWin
{

QPainterBackend::QPainterBackend()
    : m_failed(false)
{
}

QPainterBackend::~QPainterBackend()
{
}

OverlayWindow* QPainterBackend::overlayWindow()
{
    return nullptr;
}

void QPainterBackend::showOverlay()
{
}

void QPainterBackend::screenGeometryChanged(const QSize &size)
{
    Q_UNUSED(size)
}

void QPainterBackend::setFailed(const QString &reason)
{
    qCWarning(KWIN_QPAINTER) << "Creating the QPainter backend failed: " << reason;
    m_failed = true;
}

bool QPainterBackend::perScreenRendering() const
{
    return false;
}

QImage *QPainterBackend::bufferForScreen(int screenId)
{
    Q_UNUSED(screenId)
    return buffer();
}

}
