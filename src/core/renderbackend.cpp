/*
    SPDX-FileCopyrightText: 2021 Vlad Zahorodnii <vlad.zahorodnii@kde.org>

    SPDX-License-Identifier: GPL-2.0-or-later
*/

#include "renderbackend.h"
#include "scene/surfaceitem.h"

#include <drm_fourcc.h>

namespace KWin
{

RenderBackend::RenderBackend(QObject *parent)
    : QObject(parent)
{
}

OverlayWindow *RenderBackend::overlayWindow() const
{
    return nullptr;
}

bool RenderBackend::checkGraphicsReset()
{
    return false;
}

QHash<uint32_t, QVector<uint64_t>> RenderBackend::supportedFormats() const
{
    return QHash<uint32_t, QVector<uint64_t>>{{DRM_FORMAT_XRGB8888, QVector<uint64_t>{DRM_FORMAT_MOD_LINEAR}}};
}

std::unique_ptr<SurfaceTexture> RenderBackend::createSurfaceTextureInternal(SurfacePixmapInternal *pixmap)
{
    return nullptr;
}

std::unique_ptr<SurfaceTexture> RenderBackend::createSurfaceTextureX11(SurfacePixmapX11 *pixmap)
{
    return nullptr;
}

std::unique_ptr<SurfaceTexture> RenderBackend::createSurfaceTextureWayland(SurfacePixmapWayland *pixmap)
{
    return nullptr;
}

} // namespace KWin
