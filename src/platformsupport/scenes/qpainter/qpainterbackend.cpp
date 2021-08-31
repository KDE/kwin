/*
    KWin - the KDE window manager
    This file is part of the KDE project.

    SPDX-FileCopyrightText: 2013 Martin Gräßlin <mgraesslin@kde.org>

    SPDX-License-Identifier: GPL-2.0-or-later
*/
#include "qpainterbackend.h"
#include "platformqpaintersurfacetexture_internal.h"
#include "platformqpaintersurfacetexture_wayland.h"
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

PlatformSurfaceTexture *QPainterBackend::createPlatformSurfaceTextureInternal(SurfacePixmapInternal *pixmap)
{
    return new PlatformQPainterSurfaceTextureInternal(this, pixmap);
}

PlatformSurfaceTexture *QPainterBackend::createPlatformSurfaceTextureWayland(SurfacePixmapWayland *pixmap)
{
    return new PlatformQPainterSurfaceTextureWayland(this, pixmap);
}

void QPainterBackend::setFailed(const QString &reason)
{
    qCWarning(KWIN_QPAINTER) << "Creating the QPainter backend failed: " << reason;
    m_failed = true;
}

}
