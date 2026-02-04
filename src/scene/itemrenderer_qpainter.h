/*
    SPDX-FileCopyrightText: 2022 Vlad Zahorodnii <vlad.zahorodnii@kde.org>

    SPDX-License-Identifier: GPL-2.0-or-later
*/

#pragma once

#include "scene/itemrenderer.h"

class QPainter;

namespace KWin
{

class DecorationItem;
class SurfaceItem;

class KWIN_EXPORT ItemRendererQPainter : public ItemRenderer
{
public:
    ItemRendererQPainter();
    ~ItemRendererQPainter() override;

    QPainter *painter() const override;

    std::unique_ptr<Texture> createTexture(GraphicsBuffer *buffer) override;
    std::unique_ptr<Texture> createTexture(const QImage &image) override;

    std::unique_ptr<NinePatch> createNinePatch(const QImage &image) override;
    std::unique_ptr<NinePatch> createNinePatch(const QImage &topLeftPatch,
                                               const QImage &topPatch,
                                               const QImage &topRightPatch,
                                               const QImage &rightPatch,
                                               const QImage &bottomRightPatch,
                                               const QImage &bottomPatch,
                                               const QImage &bottomLeftPatch,
                                               const QImage &leftPatch) override;

    std::unique_ptr<Atlas> createAtlas(const QList<QImage> &sprites) override;

    void beginFrame(const RenderTarget &renderTarget, const RenderViewport &viewport) override;
    void endFrame() override;

    void renderBackground(const RenderTarget &renderTarget, const RenderViewport &viewport, const Region &deviceRegion) override;
    void renderItem(const RenderTarget &renderTarget, const RenderViewport &viewport, Item *item, int mask, const Region &deviceRegion, const WindowPaintData &data, const std::function<bool(Item *)> &filter, const std::function<bool(Item *)> &holeFilter) override;

private:
    void renderSurfaceItem(QPainter *painter, SurfaceItem *surfaceItem) const;
    void renderDecorationItem(QPainter *painter, DecorationItem *decorationItem) const;
    void renderImageItem(QPainter *painter, ImageItem *imageItem) const;
    void renderItem(QPainter *painter, Item *item, const std::function<bool(Item *)> &filter) const;

    std::unique_ptr<QPainter> m_painter;
};

} // namespace KWin
