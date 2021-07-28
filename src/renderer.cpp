/*
    SPDX-FileCopyrightText: 2021 Vlad Zahorodnii <vlad.zahorodnii@kde.org>

    SPDX-License-Identifier: GPL-2.0-or-later
*/

#include "renderer.h"

namespace KWin
{

Renderer::Renderer(QObject *parent)
    : QObject(parent)
{
}

PlatformSurfaceTexture *Renderer::createPlatformSurfaceTextureInternal(SurfacePixmapInternal *pixmap)
{
    Q_UNUSED(pixmap)
    return nullptr;
}

PlatformSurfaceTexture *Renderer::createPlatformSurfaceTextureX11(SurfacePixmapX11 *pixmap)
{
    Q_UNUSED(pixmap)
    return nullptr;
}

PlatformSurfaceTexture *Renderer::createPlatformSurfaceTextureWayland(SurfacePixmapWayland *pixmap)
{
    Q_UNUSED(pixmap)
    return nullptr;
}

} // namespace KWin
