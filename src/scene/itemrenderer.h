/*
    SPDX-FileCopyrightText: 2022 Vlad Zahorodnii <vlad.zahorodnii@kde.org>

    SPDX-License-Identifier: GPL-2.0-or-later
*/

#pragma once

#include <kwin_export.h>

#include "core/region.h"

#include <QColor>
#include <QMatrix4x4>
#include <memory>

namespace KWin
{

class Atlas;
class GraphicsBuffer;
class ImageItem;
class Item;
class NinePatch;
class RenderTarget;
class RenderViewport;
class Scene;
class Texture;
class WindowPaintData;
class SyncReleasePoint;
class ColorDescription;
class RenderDevice;
class FileDescriptor;

class KWIN_EXPORT ItemRenderer
{
public:
    explicit ItemRenderer(RenderDevice *device);
    virtual ~ItemRenderer();

    virtual std::unique_ptr<Texture> createTexture(GraphicsBuffer *buffer, const FileDescriptor &sync,
                                                   const std::shared_ptr<SyncReleasePoint> &releasePoint,
                                                   const std::shared_ptr<ColorDescription> &color) = 0;
    virtual std::unique_ptr<Texture> createTexture(const QImage &image) = 0;

    virtual std::unique_ptr<NinePatch> createNinePatch(const QImage &image) = 0;
    virtual std::unique_ptr<NinePatch> createNinePatch(const QImage &topLeftPatch,
                                                       const QImage &topPatch,
                                                       const QImage &topRightPatch,
                                                       const QImage &rightPatch,
                                                       const QImage &bottomRightPatch,
                                                       const QImage &bottomPatch,
                                                       const QImage &bottomLeftPatch,
                                                       const QImage &leftPatch) = 0;

    virtual std::unique_ptr<Atlas> createAtlas(const QList<QImage> &sprites) = 0;

    virtual void beginFrame(const RenderTarget &renderTarget, const RenderViewport &viewport);
    virtual void endFrame();

    /*!
     * Clears the background of \a deviceRegion to \a color.
     *
     * An invalid \a color clears to fully transparent black, which is what
     * callers that have no opinion about the background should pass.
     */
    virtual void renderBackground(const RenderTarget &renderTarget, const RenderViewport &viewport, const Region &deviceRegion, const QColor &color) = 0;
    [[nodiscard]] virtual bool renderItem(const RenderTarget &renderTarget, const RenderViewport &viewport, Item *item, int mask, const Region &deviceRegion, const WindowPaintData &data, const std::function<bool(Item *)> &filter, const std::function<bool(Item *)> &holeFilter) = 0;

    virtual void setLayerDebugging(bool enable);

    RenderDevice *renderDevice() const;

protected:
    RenderDevice *const m_renderDevice;
};

} // namespace KWin
