/*
    SPDX-FileCopyrightText: 2022 Vlad Zahorodnii <vlad.zahorodnii@kde.org>

    SPDX-License-Identifier: GPL-2.0-or-later
*/

#pragma once

#include <kwin_export.h>

#include "core/region.h"

#include <QMatrix4x4>
#include <memory>

class QPainter;

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

class KWIN_EXPORT ItemRenderer
{
public:
    ItemRenderer();
    virtual ~ItemRenderer();

    virtual QPainter *painter() const;

    virtual std::unique_ptr<Texture> createTexture(GraphicsBuffer *buffer) = 0;
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

    virtual void renderBackground(const RenderTarget &renderTarget, const RenderViewport &viewport, const Region &deviceRegion) = 0;
    virtual void renderItem(const RenderTarget &renderTarget, const RenderViewport &viewport, Item *item, int mask, const Region &deviceRegion, const WindowPaintData &data, const std::function<bool(Item *)> &filter, const std::function<bool(Item *)> &holeFilter) = 0;
};

} // namespace KWin
