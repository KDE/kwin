/*
    SPDX-FileCopyrightText: 2022 Vlad Zahorodnii <vlad.zahorodnii@kde.org>

    SPDX-License-Identifier: GPL-2.0-or-later
*/

#pragma once

#include <kwin_export.h>

#include <QMatrix4x4>

class QPainter;

namespace KWin
{

class ImageItem;
class Item;
class RenderTarget;
class ViewPort;
class Scene;
class WindowPaintData;

class KWIN_EXPORT ItemRenderer
{
public:
    ItemRenderer();
    virtual ~ItemRenderer();

    virtual QPainter *painter() const;

    virtual void beginFrame(const RenderTarget &renderTarget, const ViewPort &viewPort);
    virtual void endFrame();

    virtual void renderBackground(const RenderTarget &renderTarget, const ViewPort &viewPort, const QRegion &region) = 0;
    virtual void renderItem(const RenderTarget &renderTarget, const ViewPort &viewPort, Item *item, int mask, const QRegion &region, const WindowPaintData &data) = 0;

    virtual ImageItem *createImageItem(Scene *scene, Item *parent = nullptr) = 0;
};

} // namespace KWin
