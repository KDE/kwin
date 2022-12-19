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

    void beginFrame(RenderTarget *renderTarget) override;
    void endFrame() override;

    void renderBackground(const QRegion &region) override;
    void renderItem(Item *item, int mask, const QRegion &region, const WindowPaintData &data) override;

    ImageItem *createImageItem(Scene *scene, Item *parent = nullptr) override;

private:
    void renderSurfaceItem(QPainter *painter, SurfaceItem *surfaceItem) const;
    void renderDecorationItem(QPainter *painter, DecorationItem *decorationItem) const;
    void renderImageItem(QPainter *painter, ImageItem *imageItem) const;
    void renderItem(QPainter *painter, Item *item) const;

    std::unique_ptr<QPainter> m_painter;
};

} // namespace KWin
