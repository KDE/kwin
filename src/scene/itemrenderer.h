/*
    SPDX-FileCopyrightText: 2022 Vlad Zahorodnii <vlad.zahorodnii@kde.org>

    SPDX-License-Identifier: GPL-2.0-or-later
*/

#pragma once

#include <kwin_export.h>

#include "core/outputlayer.h"
#include "core/region.h"
#include "core/rendertarget.h"

#include <QMatrix4x4>
#include <memory>

namespace KWin
{

class Atlas;
class GraphicsBuffer;
class ImageItem;
class Item;
class NinePatch;
class RenderViewport;
class Scene;
class Texture;
class WindowPaintData;
class SyncReleasePoint;
class ColorDescription;
class RenderDevice;
class FileDescriptor;
class RenderView;
class OutputFrame;

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

    [[nodiscard]] virtual std::optional<OutputLayerBeginFrameInfo> beginFrame(const OutputLayerBeginFrameInfo &beginInfo,
                                                                              RenderView *view,
                                                                              OutputFrame *frame) = 0;
    [[nodiscard]] virtual FileDescriptor endFrame(const Region &repainted, RenderView *view, OutputFrame *frame) = 0;

    virtual void renderBackground(const RenderTarget &renderTarget, const RenderViewport &viewport, const Region &deviceRegion) = 0;
    [[nodiscard]] virtual bool renderItem(const RenderTarget &renderTarget, const RenderViewport &viewport, Item *item, int mask, const Region &deviceRegion, const WindowPaintData &data, const std::function<bool(Item *)> &filter, const std::function<bool(Item *)> &holeFilter) = 0;

    virtual void setLayerDebugging(bool enable);

    RenderDevice *renderDevice() const;

protected:
    RenderDevice *const m_renderDevice;
};

} // namespace KWin
