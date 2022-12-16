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

class Item;
class RenderTarget;
class WindowPaintData;

class KWIN_EXPORT ItemRenderer
{
public:
    ItemRenderer();
    virtual ~ItemRenderer();

    QMatrix4x4 renderTargetProjectionMatrix() const;
    QRect renderTargetRect() const;
    void setRenderTargetRect(const QRectF &rect);
    qreal renderTargetScale() const;
    void setRenderTargetScale(qreal scale);

    QRegion mapToRenderTarget(const QRegion &region) const;
    virtual QPainter *painter() const;

    virtual void beginFrame(RenderTarget *renderTarget);
    virtual void endFrame();

    virtual void renderBackground(const QRegion &region) = 0;
    virtual void renderItem(Item *item, int mask, const QRegion &region, const WindowPaintData &data) = 0;

protected:
    QMatrix4x4 m_renderTargetProjectionMatrix;
    QRectF m_renderTargetRect;
    qreal m_renderTargetScale = 1;
};

} // namespace KWin
