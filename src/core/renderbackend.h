/*
    SPDX-FileCopyrightText: 2021 Vlad Zahorodnii <vlad.zahorodnii@kde.org>

    SPDX-License-Identifier: GPL-2.0-or-later
*/

#pragma once

#include "libkwineffects/kwinglobals.h"
#include "libkwineffects/rendertarget.h"

#include <QObject>

#include <memory>

namespace KWin
{

class GraphicsBuffer;
class GraphicsBufferAllocator;
class Output;
class OverlayWindow;
class OutputLayer;
class SurfacePixmap;
class SurfacePixmapX11;
class SurfaceTexture;

/**
 * The RenderBackend class is the base class for all rendering backends.
 */
class KWIN_EXPORT RenderBackend : public QObject
{
    Q_OBJECT

public:
    explicit RenderBackend(QObject *parent = nullptr);

    virtual CompositingType compositingType() const = 0;
    virtual OverlayWindow *overlayWindow() const;

    virtual bool checkGraphicsReset();

    virtual OutputLayer *primaryLayer(Output *output) = 0;
    virtual OutputLayer *cursorLayer(Output *output);
    virtual void present(Output *output) = 0;

    virtual GraphicsBufferAllocator *graphicsBufferAllocator() const;

    virtual bool testImportBuffer(GraphicsBuffer *buffer);
    virtual QHash<uint32_t, QVector<uint64_t>> supportedFormats() const;

    virtual std::unique_ptr<SurfaceTexture> createSurfaceTextureX11(SurfacePixmapX11 *pixmap);
    virtual std::unique_ptr<SurfaceTexture> createSurfaceTextureWayland(SurfacePixmap *pixmap);
};

} // namespace KWin
