/*
    SPDX-FileCopyrightText: 2022 Vlad Zahorodnii <vlad.zahorodnii@kde.org>

    SPDX-License-Identifier: GPL-2.0-or-later
*/

#pragma once

#include <kwin_export.h>

#include <QMatrix4x4>
#include <memory>

class QPainter;

namespace KWin
{

class ImageItem;
class Item;
class RenderTarget;
class RenderViewport;
class Scene;
class WindowPaintData;

class KWIN_EXPORT ItemRenderer
{
public:
    ItemRenderer();
    virtual ~ItemRenderer();

    virtual QPainter *painter() const;

    virtual void beginFrame(const RenderTarget &renderTarget, const RenderViewport &viewport);
    virtual void endFrame();

    virtual void renderBackground(const RenderTarget &renderTarget, const RenderViewport &viewport, const QRegion &region) = 0;
    virtual void renderItem(const RenderTarget &renderTarget, const RenderViewport &viewport, Item *item, int mask, const QRegion &region, const WindowPaintData &data) = 0;

    virtual std::unique_ptr<ImageItem> createImageItem(Item *parent = nullptr) = 0;
};

} // namespace KWin
