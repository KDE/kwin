/*
    SPDX-FileCopyrightText: 2021 Vlad Zahorodnii <vlad.zahorodnii@kde.org>

    SPDX-License-Identifier: GPL-2.0-or-later
*/

#pragma once

#include "kwin_export.h"

#include <QRegion>

namespace KWin
{

class RenderLayer;
class RenderTarget;
class SurfaceItem;

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
     * Returns the repaints schduled for the next frame.
     */
    virtual QRegion repaints() const;

    /**
     * This function is called by the compositor before starting compositing. Reimplement
     * this function to do frame initialization.
     */
    virtual void prePaint();

    /**
     * This function is called by the compositor after finishing compositing. Reimplement
     * this function to do post frame cleanup.
     */
    virtual void postPaint();

    /**
     * Returns the direct scanout candidate hint. It can be used to avoid compositing the
     * render layer.
     */
    virtual SurfaceItem *scanoutCandidate() const;

    /**
     * This function is called when the compositor wants the render layer delegate
     * to repaint its contents.
     */
    virtual void paint(RenderTarget *renderTarget, const QRegion &region) = 0;

private:
    RenderLayer *m_layer = nullptr;
};

} // namespace KWin
