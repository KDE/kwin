/*
    SPDX-FileCopyrightText: 2021 Vlad Zahorodnii <vlad.zahorodnii@kde.org>

    SPDX-License-Identifier: GPL-2.0-or-later
*/

#pragma once

#include "effect/globals.h"
#include "kwin_export.h"

#include <QRegion>
#include <chrono>

namespace KWin
{

class RenderLayer;
class RenderTarget;
class RenderViewport;
class SurfaceItem;
class OutputFrame;
class Item;

/**
 * The RenderLayerDelegate class represents a render layer's contents.
 */
class KWIN_EXPORT RenderLayerDelegate
{
public:
    virtual ~RenderLayerDelegate() = default;

    RenderLayer *layer() const;
    void setLayer(RenderLayer *layer);

    /**
     * This function is called by the compositor after compositing the frame.
     */
    virtual void frame(OutputFrame *frame);

    /**
     * This function is called by the compositor before starting painting. Reimplement
     * this function to do frame initialization.
     */
    virtual QRegion prePaint();

    /**
     * This function is called by the compositor after finishing painting. Reimplement
     * this function to do post frame cleanup.
     */
    virtual void postPaint();

    /**
     * Returns the direct scanout candidate hint. It can be used to avoid compositing the
     * render layer.
     */
    virtual QList<SurfaceItem *> scanoutCandidates(ssize_t maxCount) const;

    /**
     * This function is called when the compositor wants the render layer delegate
     * to repaint its contents.
     */
    virtual void paint(const RenderTarget &renderTarget, const QRegion &region) = 0;

    virtual double desiredHdrHeadroom() const;

    void hideItem(Item *item);
    void showItem(Item *item);
    /**
     * @returns whether or not the Item should be rendered for this delegate specifically.
     * If it returns true, the Item is already rendered in some other way (like on a hardware plane)
     */
    bool shouldRenderItem(Item *item) const;

private:
    RenderLayer *m_layer = nullptr;
    QList<Item *> m_hiddenItems;
};

} // namespace KWin
