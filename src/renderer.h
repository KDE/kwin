/*
    SPDX-FileCopyrightText: 2021 Vlad Zahorodnii <vlad.zahorodnii@kde.org>

    SPDX-License-Identifier: GPL-2.0-or-later
*/

#pragma once

#include <kwin_export.h>

#include <QObject>

#include <chrono>

namespace KWin
{

class AbstractOutput;
class PlatformSurfaceTexture;
class SurfacePixmapInternal;
class SurfacePixmapX11;
class SurfacePixmapWayland;

/**
 * The Renderer class is the base class for all renderers.
 */
class KWIN_EXPORT Renderer : public QObject
{
public:
    explicit Renderer(QObject *parent = nullptr);

    virtual PlatformSurfaceTexture *createPlatformSurfaceTextureInternal(SurfacePixmapInternal *pixmap);
    virtual PlatformSurfaceTexture *createPlatformSurfaceTextureX11(SurfacePixmapX11 *pixmap);
    virtual PlatformSurfaceTexture *createPlatformSurfaceTextureWayland(SurfacePixmapWayland *pixmap);

    /**
     * Queries the render time of the last frame for the given @a output.
     */
    virtual std::chrono::nanoseconds renderTime(AbstractOutput *output) = 0;
};

} // namespace KWin
