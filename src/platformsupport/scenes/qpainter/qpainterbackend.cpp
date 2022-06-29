/*
    KWin - the KDE window manager
    This file is part of the KDE project.

    SPDX-FileCopyrightText: 2013 Martin Gräßlin <mgraesslin@kde.org>

    SPDX-License-Identifier: GPL-2.0-or-later
*/
#include "qpainterbackend.h"
#include "qpaintersurfacetexture_internal.h"
#include "qpaintersurfacetexture_wayland.h"
#include "utils/common.h"

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

CompositingType QPainterBackend::compositingType() const
{
    return QPainterCompositing;
}

std::unique_ptr<SurfaceTexture> QPainterBackend::createSurfaceTextureInternal(SurfacePixmapInternal *pixmap)
{
    return std::make_unique<QPainterSurfaceTextureInternal>(this, pixmap);
}

std::unique_ptr<SurfaceTexture> QPainterBackend::createSurfaceTextureWayland(SurfacePixmapWayland *pixmap)
{
    return std::make_unique<QPainterSurfaceTextureWayland>(this, pixmap);
}

void QPainterBackend::setFailed(const QString &reason)
{
    qCWarning(KWIN_QPAINTER) << "Creating the QPainter backend failed: " << reason;
    m_failed = true;
}

}
